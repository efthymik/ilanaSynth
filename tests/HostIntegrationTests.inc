// Host integration (blind-spots batch A3): learned and macro CCs reach the
// sound in the block they arrive in, before the message thread has run (an
// offline export); CCs and the audio thread's changes stay out of the undo
// history; REPEATABLE RANDOM plays a random arp the same from the same bar.
// Included by TableTest.cpp; ILANA_HOST_TEST=1 runs only these.

namespace HostLab
{
struct Head : juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (120.0);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        return info;
    }
};

inline void set (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

inline void block (IlanaSynthAudioProcessor& processor, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();
    processor.processBlock (buffer, midi);
}

// A random arp's note-ons from the transport's start, as a clip plays it:
// first `stoppedBlocks` blocks with the transport stopped and the chord held
// (so the arp's generator has run a while), the chord let go, then the
// transport plays from bar 1 with the chord pressed on its first sample,
// for 600 blocks of 1/16 steps (about 50 notes).
inline std::vector<int> randomArpNotes (bool repeatable, int stoppedBlocks)
{
    IlanaSynthAudioProcessor processor;
    Head head;
    processor.setPlayHead (&head);
    processor.prepareToPlay (48000.0, 256);
    set (processor, "arp_on", 1.0f);
    set (processor, "arp_mode", 3.0f); // Random
    set (processor, "arp_octaves", 3.0f);
    set (processor, "arp_div", 4.0f);  // 1/16
    set (processor, "repeat_random", repeatable ? 1.0f : 0.0f);

    const int chord[] { 60, 63, 67, 70 };
    std::vector<int> notes;
    for (int i = 0; i < stoppedBlocks + 1 + 600; ++i)
    {
        juce::MidiBuffer midi;
        head.playing = i > stoppedBlocks;
        head.ppq = head.playing ? (double) (i - stoppedBlocks - 1) * 256.0 / 48000.0 * 2.0 : 0.0;
        if (i == 0 || i == stoppedBlocks + 1)
            for (auto note : chord)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (i == stoppedBlocks)
            for (auto note : chord)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        block (processor, midi);
        if (head.playing)
            for (const auto metadata : processor.getSynthMidiForTest())
                if (metadata.getMessage().isNoteOn())
                    notes.push_back (metadata.getMessage().getNoteNumber());
    }
    processor.setPlayHead (nullptr);
    return notes;
}
} // namespace HostLab

void runHostIntegrationTests()
{
    using namespace HostLab;

    // A macro's CC (macro 1 listens to CC 20) and a learned CC are heard in
    // their own block, with the message thread not yet run; then the
    // message thread sets the parameters, and none of it is an undo step.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.startParamLearn ("macro2");
        {
            juce::MidiBuffer learn;
            learn.addEvent (juce::MidiMessage::controllerEvent (1, 70, 0), 0);
            block (processor, learn);
        }
        processor.runAsyncUpdateForTest();
        check (processor.getParamCc ("macro2") == 70, "host: CC 70 is learned for macro 2");

        processor.getUndoManager().clearUndoHistory();
        set (processor, "f1_cutoff", 900.0f); // her edit
        processor.getUndoManager().undo();
        check (processor.getUndoManager().canRedo(), "host: her edit is undone and can be redone");

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 200);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 70, 127), 200);
        block (processor, midi);
        const auto raw1 = processor.apvts.getRawParameterValue ("macro1")->load();
        check (std::abs (processor.macroValue (0) - 1.0f) < 1.0e-6f && raw1 < 0.99f,
               "host: macro 1's CC is heard in its block (" + juce::String (processor.macroValue (0)) + ", the knob still "
                   + juce::String (raw1) + ")");
        check (std::abs (processor.macroValue (1) - 1.0f) < 1.0e-6f, "host: the learned CC is heard in its block");

        processor.runAsyncUpdateForTest();
        check (processor.apvts.getRawParameterValue ("macro1")->load() > 0.999f
                   && processor.apvts.getRawParameterValue ("macro2")->load() > 0.999f,
               "host: the message thread then sets the knobs");
        check (processor.getUndoManager().canRedo() && ! processor.getUndoManager().canUndo(),
               "host: the CCs make no undo step and keep Redo");

        // The host's automation, on another thread, is not her edit either.
        std::thread host ([&processor] { set (processor, "f1_reso", 0.7f); });
        host.join();
        check (processor.getUndoManager().canRedo() && ! processor.getUndoManager().canUndo(),
               "host: automation from the audio thread makes no undo step and keeps Redo");
        processor.getUndoManager().redo();
        check (std::abs (processor.apvts.getRawParameterValue ("f1_cutoff")->load() - 900.0f) < 1.0f,
               "host: Redo still brings her edit back");
    }

    // REPEATABLE RANDOM: two instances whose generators ran a different
    // while play the same random arp from bar 1; off, they differ.
    {
        const auto wasFixed = fixedRandomSeeds().exchange (false);
        const auto a = randomArpNotes (true, 7), b = randomArpNotes (true, 31);
        const auto c = randomArpNotes (false, 7), d = randomArpNotes (false, 31);
        fixedRandomSeeds().store (wasFixed);
        check (a.size() > 20 && a == b, "host: REPEATABLE RANDOM plays the same random arp from the same bar ("
                                            + juce::String ((int) a.size()) + " and " + juce::String ((int) b.size()) + " notes)");
        check (c.size() > 20 && c != d, "host: free random arps differ (the check above can fail)");
    }
}
