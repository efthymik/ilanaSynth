#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdio>

// Thread-ownership checks (docs/ARCHITECTURE.md, "Threads"). On in JUCE debug
// builds and in the test builds (ILANA_TEST_BUILD: ilanaTableTest, the
// fingerprint and render tools); compiled out of the plugins and every other
// Release build, so the audio thread runs exactly the same code as before.
//
//   ILANA_AUDIO_THREAD_SCOPE()    first line of processBlock: this thread is
//                                 the audio thread until the function returns
//   ILANA_ASSERT_NOT_AUDIO()      the function must never run inside
//                                 processBlock (allocates, takes a lock that
//                                 may wait, edits the patch)
//   ILANA_ASSERT_MESSAGE_THREAD() the function is message-thread only
//
// A violation prints, counts (IlanaThreads::violationCount, which ilanaTableTest
// requires to be 0 at the end) and, in a debug build, breaks into jassertfalse.
#if JUCE_DEBUG || defined (ILANA_TEST_BUILD)
 #define ILANA_THREAD_CHECKS 1
#else
 #define ILANA_THREAD_CHECKS 0
#endif

namespace IlanaThreads
{
#if ILANA_THREAD_CHECKS
    inline thread_local int audioDepth = 0;
    inline std::atomic<int> violationCount { 0 };

    inline bool onAudioThread() noexcept { return audioDepth > 0; }

    inline void violation (const char* what, const char* function, const char* file, int line)
    {
        ++violationCount;
        std::fprintf (stderr, "THREAD CHECK FAILED: %s in %s (%s:%d)\n", what, function, file, line);
        jassertfalse;
    }

    struct AudioScope
    {
        AudioScope() noexcept { ++audioDepth; }
        ~AudioScope() noexcept { --audioDepth; }
    };
#endif
}

#if ILANA_THREAD_CHECKS
 #define ILANA_AUDIO_THREAD_SCOPE() const IlanaThreads::AudioScope ilanaAudioThreadScope
 #define ILANA_ASSERT_NOT_AUDIO() \
    do { if (IlanaThreads::onAudioThread()) \
             IlanaThreads::violation ("called on the audio thread", __func__, __FILE__, __LINE__); } while (false)
 #define ILANA_ASSERT_MESSAGE_THREAD() \
    do { if (! juce::MessageManager::existsAndIsCurrentThread()) \
             IlanaThreads::violation ("not on the message thread", __func__, __FILE__, __LINE__); } while (false)
#else
 #define ILANA_AUDIO_THREAD_SCOPE() static_assert (true, "")
 #define ILANA_ASSERT_NOT_AUDIO() do {} while (false)
 #define ILANA_ASSERT_MESSAGE_THREAD() do {} while (false)
#endif
