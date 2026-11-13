// Per-note routes into the knobs a voice reads (2026-11-12, ilana: "per
// voice howl, size, etc for all modal engines", "if im randomizing the size
// ... it shouldnt retrigger the random on an already held note", "all
// engines, including wavetable"). A route from RANDOM, velocity, key, an
// envelope or a retriggered LFO into one of them (voiceKnobOffsetFor) is
// worked out by each voice for itself: a held note keeps its value while
// new notes take their own. Included by TableTest.cpp; ILANA_VOICEKNOB_TEST=1
// runs only these.

namespace VoiceKnobLab
{
// Plays `first`, then `second` four blocks later, and reads the knob (by
// its offset in VoiceParams) in each voice before and after.
struct Reading
{
    float firstBefore = 0.0f, firstAfter = 0.0f, second = 0.0f;
    bool found = false;
};

inline Reading play (IlanaSynthAudioProcessor& processor, const juce::String& knob, int first, int velocityFirst,
                     int second, int velocitySecond)
{
    const auto offset = voiceKnobOffsetFor (Mod::destinationForParamId (knob));
    const auto read = [offset] (const Voice* voice)
    {
        return *reinterpret_cast<const float*> (reinterpret_cast<const char*> (&voice->paramsForTest()) + offset);
    };

    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    Reading reading;
    const auto render = [&] (int blocks, int note, int velocity)
    {
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0 && note >= 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    };

    render (4, first, velocityFirst);
    const auto* a = processor.voicePlayingForTest (first);
    if (a == nullptr || offset < 0)
        return reading;
    reading.firstBefore = read (a);
    render (4, second, velocitySecond);
    const auto* b = processor.voicePlayingForTest (second);
    if (b == nullptr || processor.voicePlayingForTest (first) != a)
        return reading;
    reading.firstAfter = read (a);
    reading.second = read (b);
    reading.found = true;
    return reading;
}

inline void route (IlanaSynthAudioProcessor& processor, int slot, Mod::Source source, const juce::String& knob, float depth)
{
    const auto prefix = "mod" + juce::String (slot);
    setParam (processor, prefix + "_src", (float) (int) source);
    setParam (processor, prefix + "_dst", (float) Mod::destinationForParamId (knob));
    setParam (processor, prefix + "_amt", depth);
}
} // namespace VoiceKnobLab

void runVoiceKnobTests()
{
    using namespace VoiceKnobLab;
    std::cout << "--- per-note routes into the engines' knobs ---" << std::endl;

    // Every knob the table names is a destination with a parameter behind it.
    {
        auto count = 0;
        for (int d = 0; d < Mod::getNumDestinations(); ++d)
            count += voiceKnobOffsetFor (d) >= 0 ? 1 : 0;
        check (count == 6 * 35 + 9, "voice knobs: every listed knob is a destination (" + juce::String (count) + ")");
    }

    // RANDOM into a modal exciter's SIZE: drawn once per note, a held note
    // keeps its own when the next one starts.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "osc1_mode", 1.0f);
        setParam (processor, "osc1_excite", 12.0f); // Modal Guitar
        route (processor, 1, Mod::Source::Random, "osc1_modal_size", 0.4f);
        const auto r = play (processor, "osc1_modal_size", 48, 100, 55, 100);
        check (r.found && r.firstBefore == r.firstAfter, "voice knobs: a held note keeps its RANDOM SIZE when a new note starts ("
                                                             + juce::String (r.firstBefore, 4) + " -> " + juce::String (r.firstAfter, 4) + ")");
        check (r.found && r.firstBefore != 1.0f && r.second != r.firstBefore,
               "voice knobs: each note draws its own SIZE (" + juce::String (r.firstBefore, 4) + ", " + juce::String (r.second, 4) + ")");
    }

    // Velocity into Modal Bowed's WOLF (bridge buzz: its upper half is the
    // howl): soft and hard notes held together keep their own.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "osc1_mode", 1.0f);
        setParam (processor, "osc1_excite", 13.0f); // Modal Bowed
        setParam (processor, "osc1_bridge_buzz", 0.0f);
        route (processor, 1, Mod::Source::Velocity, "osc1_bridge_buzz", 1.0f);
        const auto r = play (processor, "osc1_bridge_buzz", 48, 20, 55, 127);
        check (r.found && std::abs (r.firstAfter - 20.0f / 127.0f) < 0.01f && std::abs (r.second - 1.0f) < 0.01f,
               "voice knobs: velocity into WOLF per note (" + juce::String (r.firstAfter, 3) + ", " + juce::String (r.second, 3) + ")");
    }

    // The wavetable side: velocity into WARP 2.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "osc1_warp2_amt", 0.0f);
        route (processor, 1, Mod::Source::Velocity, "osc1_warp2_amt", 1.0f);
        const auto r = play (processor, "osc1_warp2_amt", 60, 127, 64, 20);
        check (r.found && r.firstBefore == r.firstAfter && std::abs (r.firstAfter - 1.0f) < 0.01f
                   && std::abs (r.second - 20.0f / 127.0f) < 0.01f,
               "voice knobs: velocity into WARP 2 per note (" + juce::String (r.firstAfter, 3) + ", " + juce::String (r.second, 3) + ")");
    }

    // A synth-wide source (a macro) still moves the knob for every voice, as before.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "osc1_mode", 1.0f);
        setParam (processor, "osc1_excite", 12.0f);
        setParam (processor, "macro1", 1.0f);
        route (processor, 1, Mod::Source::Macro1, "osc1_modal_size", 0.2f);
        const auto r = play (processor, "osc1_modal_size", 48, 100, 55, 100);
        check (r.found && r.firstAfter == r.second && r.second > 1.0f,
               "voice knobs: a macro route moves every voice alike (" + juce::String (r.firstAfter, 4) + ", " + juce::String (r.second, 4) + ")");
    }
}
