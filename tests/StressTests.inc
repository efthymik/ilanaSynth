// Adversarial polish pass (2026-10-13). Included by TableTest.cpp;
// ILANA_STRESS_TEST=1 runs only these. A held chord is rendered through
// awkward host behaviour and the output must stay finite and bounded:
//  - 1-sample, odd and 4096-sample blocks mixed at random,
//  - the sample rate changing mid-session (44.1, 96, 48 kHz),
//  - preset spam (a load every few blocks) with notes held,
//  - parameter automation storm (every parameter jumped each block).

void runStressTests()
{
    std::cout << "--- stress ---" << std::endl;

    const auto run = [] (const juce::String& what, auto&& body, float bound = 8.0f)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        auto peak = 0.0f;
        auto bad = false;
        const auto render = [&] (int size, bool notes)
        {
            juce::AudioBuffer<float> buffer (2, size);
            juce::MidiBuffer midi;
            if (notes)
                for (const auto note : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < size; ++i)
                {
                    const auto v = buffer.getSample (ch, i);
                    if (! std::isfinite (v))
                        bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                }
        };
        body (processor, render);
        check (! bad && peak < bound, what + " (finite, peak " + juce::String (peak, 3) + ")");
    };

    run ("mixed block sizes 1 to 4096", [] (auto&, auto& render)
    {
        juce::Random random (7);
        render (512, true);
        const int sizes[] { 1, 1, 2, 3, 7, 16, 17, 64, 333, 1024, 4096 };
        for (int i = 0; i < 300; ++i)
            render (sizes[random.nextInt ((int) std::size (sizes))], false);
    });

    run ("sample rate changes mid-session", [] (auto& processor, auto& render)
    {
        render (512, true);
        for (const auto rate : { 44100.0, 96000.0, 48000.0, 192000.0, 44100.0 })
        {
            processor.releaseResources();
            processor.prepareToPlay (rate, 256);
            for (int i = 0; i < 20; ++i)
                render (256, i == 0);
        }
    });

    run ("preset spam with notes held", [] (auto& processor, auto& render)
    {
        const auto count = juce::jmin (150, processor.getNumAllPresets());
        render (512, true);
        for (int i = 0; i < count; ++i)
        {
            processor.loadPresetByIndex (i * 3 % juce::jmax (1, processor.getNumAllPresets()));
            for (int b = 0; b < 3; ++b)
                render (128, b == 0);
        }
    });

    run ("parameter automation storm", [] (auto& processor, auto& render)
    {
        juce::Random random (11);
        render (512, true);
        for (int i = 0; i < 120; ++i)
        {
            for (auto* parameter : processor.getParameters())
                if (random.nextInt (4) == 0)
                    parameter->setValueNotifyingHost (random.nextFloat());
            render (256, i % 10 == 0);
        }
    },
    // Its random patches stack gains with the output clip off (MASTER up to
    // +12 dB, UTILITY, EQ and TILT boosts), so a sound one can peak near 9
    // (#149's two new parameters moved its patch onto one: MASTER -6 dB,
    // UTILITY +10, EQ +13 / +15, TILT +12). A runaway still blows far past
    // 32 (+30 dB) or goes non-finite.
    32.0f);

    // One voice gone NaN (weak points 2026-11-15): before, its NaN summed
    // into every sample, so the whole mix was zeroed and the effects cleared
    // every block for as long as it rang. Now that voice is stopped within a
    // block or two and the other notes play on, at the same level.
    for (const auto multiCore : { false, true })
    {
        const struct Force { explicit Force (int v) { IlanaSynthAudioProcessor::forceVoiceThreads = v; }
                             ~Force() { IlanaSynthAudioProcessor::forceVoiceThreads = -1; } } force (multiCore ? 1 : 0);
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        const auto render = [&processor] (bool notes)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            if (notes)
                for (const auto note : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            auto finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    finite = finite && std::isfinite (buffer.getSample (ch, i));
            return std::pair<bool, float> { finite, buffer.getRMSLevel (0, 0, buffer.getNumSamples()) };
        };
        render (true);
        for (int i = 0; i < 8; ++i)
            render (false);
        const auto before = render (false).second;
        Voice* broken = nullptr;
        for (int i = 0; i < processor.getSynthForTest().getNumVoices() && broken == nullptr; ++i)
            if (auto* voice = dynamic_cast<Voice*> (processor.getSynthForTest().getVoice (i)); voice != nullptr && voice->isVoiceActive())
                broken = voice;
        check (broken != nullptr, "NaN voice: a note is sounding");
        if (broken == nullptr)
            continue;
        broken->breakForTest();
        auto allFinite = true;
        for (int i = 0; i < 3; ++i)
            allFinite = render (false).first && allFinite;
        const auto after = render (false);
        const auto label = juce::String (multiCore ? " (MULTI-CORE)" : "");
        check (allFinite && after.first, "NaN voice: output stays finite" + label);
        check (processor.getSynthForTest().getNonFiniteVoicesStopped() == 1, "NaN voice: the broken voice is stopped ("
               + juce::String (processor.getSynthForTest().getNonFiniteVoicesStopped()) + ")" + label);
        check (after.second > 0.4f * before, "NaN voice: the other notes play on (rms " + juce::String (after.second, 4)
               + " vs " + juce::String (before, 4) + ")" + label);
    }
}
