// Host hardening: a host block longer than prepareToPlay announced keeps the
// beat clock moving across its chunks; a NaN on the input does not latch the
// Input Env follower (FX build); notes held while the host bypassed the
// plug-in are released afterwards. Included by TableTest.cpp, after
// HostIntegrationTests.inc (it uses HostLab).

namespace HostHardening
{
using Notes = std::vector<std::pair<int, int>>; // (position, note) of the note-ons the synth was given

inline Notes noteOnsOf (const juce::MidiBuffer& midi)
{
    Notes notes;
    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOn())
            notes.emplace_back (metadata.samplePosition, metadata.getMessage().getNoteNumber());
    return notes;
}

inline void setUpArp (IlanaSynthAudioProcessor& processor)
{
    HostLab::set (processor, "arp_on", 1.0f);
    HostLab::set (processor, "arp_div", 4.0f); // 1/16: a step every 6000 samples at 120 BPM, 48 kHz
}
} // namespace HostHardening

void runHostHardeningTests()
{
    using namespace HostLab;
    using namespace HostHardening;

    // A 8192-sample block on a processor prepared for 4096 is two chunks; the
    // arp's second chunk must play what a second normal block of 4096 plays.
    {
        constexpr int size = 4096;
        const double ppqPerBlock = (double) size / 48000.0 * 2.0;
        const int chord[] { 60, 64, 67 };

        auto start = [&chord] (IlanaSynthAudioProcessor& processor, Head& head)
        {
            processor.setPlayHead (&head);
            processor.prepareToPlay (48000.0, size);
            setUpArp (processor);
            head.playing = true;
            head.ppq = 0.0;
            juce::MidiBuffer midi;
            for (auto note : chord)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            return midi;
        };

        Notes expected, oversize;
        {
            IlanaSynthAudioProcessor processor;
            Head head;
            auto midi = start (processor, head);
            juce::AudioBuffer<float> buffer (2, size);
            buffer.clear();
            processor.processBlock (buffer, midi);
            head.ppq = ppqPerBlock;
            juce::MidiBuffer none;
            buffer.clear();
            processor.processBlock (buffer, none);
            expected = noteOnsOf (processor.getSynthMidiForTest());
            processor.setPlayHead (nullptr);
        }
        {
            IlanaSynthAudioProcessor processor;
            Head head;
            auto midi = start (processor, head);
            juce::AudioBuffer<float> buffer (2, size * 2);
            buffer.clear();
            processor.processBlock (buffer, midi);
            oversize = noteOnsOf (processor.getSynthMidiForTest()); // (the last chunk's)
            processor.setPlayHead (nullptr);
        }
        check (! expected.empty(), "hardening: the arp plays a step in the second block (" + juce::String ((int) expected.size()) + " notes)");
        check (expected == oversize, "hardening: an over-long block's second chunk plays the arp step a normal second block plays");
    }

    // A NaN sample on the input must not latch the Input Env (FX build only).
    if constexpr (IlanaSynthAudioProcessor::isEffectBuild)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        buffer.clear();
        buffer.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
        buffer.setSample (1, 20, std::numeric_limits<float>::infinity());
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 40; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        check (processor.getInputEnvelope() < 0.01f, "hardening: a NaN / Inf input does not latch the Input Env ("
                                                          + juce::String (processor.getInputEnvelope()) + ")");
    }

    // Notes held when the host bypasses, and let go while it is bypassed,
    // are released at the next processed block.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        if (! IlanaSynthAudioProcessor::isEffectBuild)
            check (processor.getActiveVoiceCount() > 0, "hardening: a note is sounding before the bypass");
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            buffer.clear();
            processor.processBlockBypassed (buffer, midi);
        }
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        check (processor.getActiveVoiceCount() == 0, "hardening: the note held across a bypassed note-off is released afterwards");
    }
}
