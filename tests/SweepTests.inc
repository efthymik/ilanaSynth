// Broad bug sweep (2026-11-22): regressions for what it fixed.
// Included by TableTest.cpp; ILANA_SWEEP_TEST=1 runs only these.

void runSweepFixTests()
{
    const auto setPlain = [] (IlanaSynthAudioProcessor& p, const juce::String& id, float value)
    {
        if (auto* parameter = p.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto readPlain = [] (IlanaSynthAudioProcessor& p, const juce::String& id)
    {
        auto* parameter = p.apvts.getParameter (id);
        return parameter != nullptr ? parameter->convertFrom0to1 (parameter->getValue()) : -1.0f;
    };

    // FX CHAIN 1 / CHAIN 2: the chain not in the rack is saved with the patch
    // and comes back; another patch loaded starts on one chain again.
    {
        IlanaSynthAudioProcessor p;
        p.setNonRealtime (true);
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        setPlain (p, "fx_slot1", 9.0f);  // chain 1: Delay
        p.switchFxChain();               // chain 2 starts as a copy
        setPlain (p, "fx_slot1", 13.0f); // chain 2: Reverb
        juce::MemoryBlock state;
        p.getStateInformation (state);

        IlanaSynthAudioProcessor q;
        q.setNonRealtime (true);
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (! q.isShowingChainA() && (int) readPlain (q, "fx_slot1") == 13, "fx chains: a reload shows chain 2 as saved");
        q.switchFxChain();
        check (q.isShowingChainA() && (int) readPlain (q, "fx_slot1") == 9, "fx chains: chain 1 comes back from the saved patch");

        q.loadFactoryPreset (1);
        check (q.isShowingChainA(), "fx chains: a factory preset starts on chain 1");
        const auto rack = (int) readPlain (q, "fx_slot1");
        q.switchFxChain();
        check ((int) readPlain (q, "fx_slot1") == rack, "fx chains: chain 2 of the old patch does not pour over a new one");
    }

    // Init on an LFO: its drawn shape is the sine a new patch has, not a flat line.
    {
        IlanaSynthAudioProcessor p;
        p.setNonRealtime (true);
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        p.initModule (IlanaSynthAudioProcessor::ModuleKind::Lfo, 0);
        const auto quarter = IlanaSynthAudioProcessor::lfoDrawSteps / 4;
        check (std::abs (p.getLfoCustomPoint (0, quarter) - 1.0f) < 0.01f, "lfo init: the drawn shape is a sine again");
    }

    // OSC 3 at LEVEL 0 opened by a route: it plays at its note (it was skipped
    // in the per-block set-up, so it had no pitch).
    {
        fixedRandomSeeds() = true;
        struct Restore { ~Restore() { fixedRandomSeeds() = false; } } restore;
        const auto render = [&] (bool routed)
        {
            IlanaSynthAudioProcessor p;
            p.setNonRealtime (true);
            p.prepareToPlay (48000.0, 256);
            p.loadFactoryPreset (0);
            setPlain (p, "osc1_level", 0.0f);
            setPlain (p, "osc2_level", 0.0f);
            setPlain (p, "sub_on", 1.0f);
            setPlain (p, "sub_mode", 0.0f);
            setPlain (p, "sub_level", 0.0f);
            if (routed)
            {
                p.setModSlotValue (0, "src", (float) Mod::Source::Velocity);
                p.setModSlotValue (0, "dst", (float) Mod::Destination::SubLevel);
                p.setModSlotValue (0, "amt", 1.0f);
            }
            p.runAsyncUpdateForTest();
            juce::AudioBuffer<float> buffer (2, 256);
            double sum = 0.0;
            int crossings[2] {};
            float last = 0.0f;
            for (int block = 0; block < 120; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 120), 0);
                if (block == 60) // the wheel up: the pitch follows (set up per block)
                    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
                p.processBlock (buffer, midi);
                for (int i = 0; i < 256; ++i)
                {
                    const auto x = buffer.getSample (0, i);
                    if ((block >= 20 && block < 60) || block >= 80)
                    {
                        sum += (double) x * x;
                        crossings[block >= 80 ? 1 : 0] += (last < 0.0f && x >= 0.0f) ? 1 : 0;
                    }
                    last = x;
                }
            }
            return std::pair<double, double> { 10.0 * std::log10 (sum / (80.0 * 256.0) + 1.0e-20),
                                               (double) crossings[1] / juce::jmax (1, crossings[0]) };
        };
        const auto [level, bendRatio] = render (true);
        check (level > -50.0, "osc 3: LEVEL 0 opened by a route plays (" + juce::String (level, 1) + " dB)");
        check (bendRatio > 1.05, "osc 3: LEVEL 0 opened by a route follows the pitch wheel (x" + juce::String (bendRatio, 3) + ")");
    }

    // Round 2 (her "fix everything"): renders with fixed seeds.
    fixedRandomSeeds() = true;
    struct RestoreSeeds { ~RestoreSeeds() { fixedRandomSeeds() = false; } } restoreSeeds;
    const auto renderNote = [] (IlanaSynthAudioProcessor& p, int blocks, const std::function<void (int, juce::MidiBuffer&)>& each,
                                int fromBlock, int toBlock, bool startNote = true)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        double sum = 0.0;
        int crossings = 0;
        float last = 0.0f;
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0 && startNote)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110), 0);
            if (each)
                each (block, midi);
            p.processBlock (buffer, midi);
            if (block >= fromBlock && block < toBlock)
                for (int i = 0; i < 256; ++i)
                {
                    const auto x = buffer.getSample (0, i);
                    sum += (double) x * x;
                    crossings += (last < 0.0f && x >= 0.0f) ? 1 : 0;
                    last = x;
                }
        }
        return std::pair<double, int> { 10.0 * std::log10 (sum / ((toBlock - fromBlock) * 256.0) + 1.0e-20), crossings };
    };

    // PLAY's Filter 2 switch is a real on/off: off passes the signal on
    // whatever the type (a high pass used to be "switched off" by its
    // cutoff, which muted it).
    {
        const auto level = [&] (int type, float cutoff, bool on)
        {
            IlanaSynthAudioProcessor p;
            p.setNonRealtime (true);
            p.prepareToPlay (48000.0, 256);
            p.loadFactoryPreset (0);
            setPlain (p, "f2_type", (float) type);
            setPlain (p, "f2_cutoff", cutoff);
            setPlain (p, "f2_on", on ? 1.0f : 0.0f);
            p.runAsyncUpdateForTest();
            return renderNote (p, 60, {}, 20, 60).first;
        };
        const auto highPassOn = level (2, 18000.0f, true);
        const auto highPassOff = level (2, 18000.0f, false);
        const auto openLowPass = level (0, 20000.0f, true);
        check (highPassOff > highPassOn + 6.0, "filter 2 off: a high pass switched off passes the sound ("
                                                 + juce::String (highPassOn, 1) + " -> " + juce::String (highPassOff, 1) + " dB)");
        check (std::abs (highPassOff - openLowPass) < 1.5, "filter 2 off: sounds like the old open low pass ("
                                                              + juce::String (highPassOff - openLowPass, 2) + " dB)");
    }

    // BEND moved while the wheel is held reaches the held bend.
    {
        IlanaSynthAudioProcessor p;
        p.setNonRealtime (true);
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        setPlain (p, "bend_range", 2.0f);
        p.runAsyncUpdateForTest();
        auto* range = p.apvts.getParameter ("bend_range");
        const auto first = renderNote (p, 50, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        }, 20, 50).second;
        range->setValueNotifyingHost (range->convertTo0to1 (12.0f));
        p.runAsyncUpdateForTest();
        const auto second = renderNote (p, 30, {}, 0, 30, false).second;
        check ((double) second / juce::jmax (1, first) > 1.4, "bend range: a held bend follows BEND 2 -> 12 (x"
                                                                + juce::String ((double) second / juce::jmax (1, first), 2) + ")");
    }

    // A section colour is one undo step, like any other edit.
    {
        IlanaSynthAudioProcessor p;
        p.setNonRealtime (true);
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        const auto before = p.getSectionColour (0);
        p.performEdit ("Section colour", [&] { p.setSectionColour (0, 0xff123456u); });
        check (p.getSectionColour (0) == 0xff123456u, "colours: the pick lands");
        p.getUndoManager().undo();
        check (p.getSectionColour (0) == before, "colours: undo takes the colour back");
    }
}
