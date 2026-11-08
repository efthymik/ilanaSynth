#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// Renders the sounding voices of one sub-block on several cores. Each voice
// renders into its own buffer (on whichever thread takes it), then the
// audio thread adds the buffers to the output in voice order, so the sum,
// and the sound, is the same as rendering them one after another into the
// output: with stereo output a voice adds exactly one value per sample and
// channel, and adding its own buffer adds that same value, in the same order.
//
// Nothing on the audio thread allocates or locks: the threads and buffers
// are made in prepare (off the audio thread), a sub-block is handed over
// through atomics, the workers sleep on an atomic wait between sub-blocks
// (a futex / WaitOnAddress, no mutex), and the audio thread renders voices
// too, then waits (a short spin, then the same atomic wait) until the last
// voice a worker took is done.
//
// The audio thread waits for the workers, so they run at audio priority
// (stability review 1): real-time threads on the Mac and Linux, MMCSS "Pro
// Audio" on Windows (as the host's own audio threads; JUCE's real-time
// option there would raise the whole host process to REALTIME_PRIORITY_CLASS).
// On the Mac they also join the host's audio workgroup when the host names
// one (an AU's render context; setWorkgroup), so they share its deadline and
// stay off the efficiency cores.
//
// One pool per instance, not one shared by all: a host renders tracks on
// several audio threads at once, and a shared pool would make one track's
// audio thread wait on another track's voices (and need a many-producer
// queue). Each instance has at most three workers, asleep between sub-blocks.
class VoiceThreads
{
public:
    using RenderFunction = std::function<void (int voice, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)>;

    ~VoiceThreads() { stop(); }

    // Off the audio thread. numWorkers 0 stops the threads (one core).
    // blockSamples and sampleRate set the real-time threads' time budget.
    void prepare (int numWorkers, int maxVoices, int maxSamples, RenderFunction newRender,
                  int blockSamples = 512, double sampleRate = 48000.0)
    {
        stop();
        render = std::move (newRender);
        capacity = juce::jmax (1, maxSamples);
        scratch.setSize (2 * juce::jmax (1, maxVoices), capacity, false, true, true);
        // The channels' pointers, taken once: AudioBuffer::getWritePointer
        // also marks the buffer not clear, a write the threads would race on.
        channelPointers.assign (scratch.getArrayOfWritePointers(), scratch.getArrayOfWritePointers() + scratch.getNumChannels());
        jobVoices.assign ((size_t) juce::jmax (1, maxVoices), 0);
        quit.store (false);
        for (int i = 0; i < numWorkers; ++i)
        {
            auto worker = std::make_unique<Worker> (*this);
            if (startWorker (*worker, juce::jmax (1, blockSamples), sampleRate > 0.0 ? sampleRate : 48000.0))
                workers.push_back (std::move (worker));
        }
    }

    void stop()
    {
        quit.store (true);
        generation.fetch_add (1);
        generation.notify_all();
        for (auto& worker : workers)
            worker->waitForThreadToExit (-1);
        workers.clear();
    }

    // The host's audio workgroup (macOS; elsewhere it is empty and joining
    // does nothing). Audio thread safe: no lock, no allocation (a copy
    // retains the handle). Returns false when a worker was reading the last
    // one just then: call again on a later block.
    bool setWorkgroup (const juce::AudioWorkgroup& newWorkgroup) noexcept
    {
        auto expected = false;
        if (! workgroupBusy.compare_exchange_strong (expected, true, std::memory_order_acquire))
            return false;
        workgroup = newWorkgroup;
        workgroupBusy.store (false, std::memory_order_release);
        workgroupVersion.fetch_add (1, std::memory_order_release);
        return true;
    }

    int getNumWorkers() const noexcept { return (int) workers.size(); }

    // Whether a sub-block of this shape can go to the threads (otherwise the
    // caller renders the voices one after another as before).
    bool canRender (const juce::AudioBuffer<float>& output, int startSample, int numSamples, int numVoices) const noexcept
    {
        return ! workers.empty() && output.getNumChannels() == 2 && startSample + numSamples <= capacity
               && numVoices <= (int) jobVoices.size() && numVoices >= 2;
    }

    // Audio thread: render the given voices (indices, in voice order) and
    // add them to the output in that order.
    void renderAndSum (const int* voices, int numVoices, juce::AudioBuffer<float>& output, int startSample, int numSamples)
    {
        // A worker still finishing the last sub-block may take one more
        // ticket from it; those are all past its count. So every field is
        // written before the counter is reset, and the counter carries the
        // count it was set with (high half), so no ticket is ever judged by
        // another sub-block's count.
        std::copy (voices, voices + numVoices, jobVoices.begin());
        jobStart.store (startSample, std::memory_order_relaxed);
        jobLength.store (numSamples, std::memory_order_relaxed);
        done.store (0, std::memory_order_relaxed);
        next.store ((std::uint64_t) numVoices << 32, std::memory_order_release);
        generation.fetch_add (1, std::memory_order_release);
        generation.notify_all();

        runJobs();

        // A short spin (a worker is usually nearly done), then sleep on the
        // counter: a worker the OS has not scheduled yet no longer keeps this
        // thread burning its time slice at yield priority.
        for (int spin = 0; spin < 2000 && done.load (std::memory_order_acquire) < numVoices; ++spin)
            std::atomic_signal_fence (std::memory_order_seq_cst);

        for (auto seenDone = done.load (std::memory_order_acquire); seenDone < numVoices;
             seenDone = done.load (std::memory_order_acquire))
            done.wait (seenDone, std::memory_order_acquire);

        for (int k = 0; k < numVoices; ++k)
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::add (output.getWritePointer (channel, startSample),
                                                  channelPointers[(size_t) (2 * k + channel)] + startSample, numSamples);
    }

private:
   #if JUCE_WINDOWS
    // The worker joins MMCSS's "Pro Audio" task at high priority while it
    // runs, as the host's audio threads do (avrt.dll, loaded by name).
    struct ProAudioTask
    {
        ProAudioTask()
        {
            if (! library.open ("avrt.dll"))
                return;
            using SetTask = void* (__stdcall*) (const wchar_t*, unsigned long*);
            using SetPriority = int (__stdcall*) (void*, int);
            revert = reinterpret_cast<Revert> (library.getFunction ("AvRevertMmThreadCharacteristics"));
            if (auto* setTask = reinterpret_cast<SetTask> (library.getFunction ("AvSetMmThreadCharacteristicsW")))
            {
                unsigned long taskIndex = 0;
                handle = setTask (L"Pro Audio", &taskIndex);
            }
            if (handle != nullptr)
                if (auto* setPriority = reinterpret_cast<SetPriority> (library.getFunction ("AvSetMmThreadPriority")))
                    setPriority (handle, 1); // AVRT_PRIORITY_HIGH
        }

        ~ProAudioTask()
        {
            if (handle != nullptr && revert != nullptr)
                revert (handle);
        }

        using Revert = int (__stdcall*) (void*);
        juce::DynamicLibrary library;
        void* handle = nullptr;
        Revert revert = nullptr;
    };
   #endif

    struct Worker final : juce::Thread
    {
        explicit Worker (VoiceThreads& o) : juce::Thread ("ilanaSynth voices"), owner (o) {}
        void run() override
        {
           #if JUCE_WINDOWS
            const ProAudioTask task;
           #endif
            owner.workerLoop();
        }
        VoiceThreads& owner;
    };

    static bool startWorker (Worker& worker, int blockSamples, double sampleRate)
    {
       #if JUCE_WINDOWS
        juce::ignoreUnused (blockSamples, sampleRate);
        return worker.startThread (juce::Thread::Priority::highest);
       #else
        // Real time where the system allows it (Linux needs the rights for
        // it), else the highest normal priority.
        const auto options = juce::Thread::RealtimeOptions{}.withPriority (8)
                                                            .withApproximateAudioProcessingTime (blockSamples, sampleRate);
        return worker.startRealtimeThread (options) || worker.startThread (juce::Thread::Priority::highest);
       #endif
    }


    void workerLoop()
    {
        // The workgroup this thread is in (kept alive while joined; the
        // token is destroyed first, on this thread, leaving it).
        juce::AudioWorkgroup joined;
        juce::WorkgroupToken token;
        unsigned joinedVersion = 0;

        // seen is read before quit: a stop() that lands before this thread
        // first runs (prepare then prepare again, quickly) is either seen
        // here or has moved generation past seen, so the wait returns;
        // waiting first could sleep through it and hang stop()'s join.
        auto seen = generation.load();
        while (! quit.load())
        {
            generation.wait (seen);
            seen = generation.load();
            if (quit.load())
                return;
            if (const auto version = workgroupVersion.load (std::memory_order_acquire); version != joinedVersion)
                joinWorkgroup (joined, token, joinedVersion, version);
            runJobs();
        }
    }

    void joinWorkgroup (juce::AudioWorkgroup& joined, juce::WorkgroupToken& token, unsigned& joinedVersion, unsigned version)
    {
        for (auto expected = false; ! workgroupBusy.compare_exchange_weak (expected, true, std::memory_order_acquire); expected = false)
            std::atomic_signal_fence (std::memory_order_seq_cst);
        auto wanted = workgroup;
        workgroupBusy.store (false, std::memory_order_release);
        joinedVersion = version;
        wanted.join (token); // leaves the old one first
        joined = std::move (wanted);
    }

    void runJobs()
    {
        // The same rounding of tiny values as the audio thread.
        juce::ScopedNoDenormals noDenormals;
        while (true)
        {
            // A ticket under its count belongs to a sub-block that can't end
            // before this item is done, so its fields stay put meanwhile (and
            // the acquire orders them after the audio thread wrote them).
            const auto ticket = next.fetch_add (1, std::memory_order_acq_rel);
            const auto k = (int) (ticket & 0xffffffffu);
            if (k >= (int) (ticket >> 32))
                return;
            const auto start = jobStart.load (std::memory_order_relaxed);
            const auto length = jobLength.load (std::memory_order_relaxed);
            float* channels[2] { channelPointers[(size_t) (2 * k)], channelPointers[(size_t) (2 * k + 1)] };
            juce::AudioBuffer<float> view (channels, 2, capacity);
            juce::FloatVectorOperations::clear (channels[0] + start, length);
            juce::FloatVectorOperations::clear (channels[1] + start, length);
            render (jobVoices[(size_t) k], view, start, length);
            // Only the last voice of the sub-block wakes the audio thread.
            if (done.fetch_add (1, std::memory_order_acq_rel) + 1 >= (int) (ticket >> 32))
                done.notify_all();
        }
    }

    RenderFunction render;
    juce::AudioBuffer<float> scratch;
    std::vector<float*> channelPointers;
    int capacity = 0;
    std::vector<int> jobVoices;
    std::atomic<int> jobStart { 0 }, jobLength { 0 };
    std::atomic<std::uint64_t> next { 0 };
    std::atomic<int> done { 0 };
    std::atomic<std::uint32_t> generation { 0 };
    std::atomic<bool> quit { false };
    juce::AudioWorkgroup workgroup;
    std::atomic<bool> workgroupBusy { false };
    std::atomic<unsigned> workgroupVersion { 0 };
    std::vector<std::unique_ptr<Worker>> workers;
};
