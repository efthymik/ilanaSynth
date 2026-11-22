// Display blind spots (ilana 2026-11-21, "check for blindspots like this":
// pictures that missed a knob, mode or route the sound follows). Each scene
// sets a patch up from Init, routes Macro 1 (or a note's velocity / envelope)
// into what the picture used to miss, runs the audio so the modulation and
// the voices' monitors are live, and captures the picture.
//
// Included by Snapshot.cpp (needs UiCtx, settle, save, findAll). Used by the
// BLINDSPOTS UI test group (each scene's picture must change with its route)
// and by `ilanaSnapshot --blindspots DIR` for the report's pictures. Uses only
// APIs that predate the fixes, so a build of the old tree can render the
// same scenes for the "before" pictures.
#pragma once

// (Snapshot.cpp includes gui/pages/PlayPage.h at its top, outside its namespace.)

struct BlindspotShot
{
    juce::String name;    // file stem, e.g. "01-env-attack"
    juce::Image plain;    // without the route (or note) the picture used to miss
    juce::Image routed;   // with it
};

inline std::vector<BlindspotShot> runBlindspotScenes (UiCtx& ctx, const juce::File& dir)
{
    auto& processor = ctx.processor;
    auto& editor = *ctx.editor;
    std::vector<BlindspotShot> shots;
    constexpr int spareSlot = 60;

    const auto set = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto route = [&processor] (int slot, Mod::Source source, int destination, float depth)
    {
        processor.setModSlotValue (slot, "src", (float) (int) source);
        processor.setModSlotValue (slot, "dst", (float) destination);
        processor.setModSlotValue (slot, "amt", depth);
    };
    const auto unroute = [&processor] (int slot)
    {
        processor.setModSlotValue (slot, "amt", 0.0f);
        processor.setModSlotValue (slot, "src", 0.0f);
        processor.setModSlotValue (slot, "dst", 0.0f);
    };
    const auto destinationOf = [] (const juce::String& id) { return modRingConfigFor (id).destination; };
    auto noteHeld = false;
    const auto run = [&processor, &noteHeld] (double seconds, int noteOn = -1, float velocity = 1.0f)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        const auto blocks = juce::jmax (1, juce::roundToInt (seconds * 48000.0 / 256.0));
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0 && noteOn >= 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, noteOn, velocity), 0);
                noteHeld = true;
            }
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    };
    const auto release = [&processor, &noteHeld]
    {
        if (! noteHeld)
            return;
        juce::AudioBuffer<float> buffer (2, 256);
        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        noteHeld = false;
    };
    const auto init = [&]
    {
        release();
        processor.loadFactoryPreset (ctx.names.indexOf ("Init"));
        unroute (spareSlot);
        set ("macro1", 0.0f);
        settle (100);
    };
    // The picture as it is now, kept on screen a moment so its timers catch up.
    const auto capture = [&] (juce::Component& picture, const juce::String& file)
    {
        settle (350);
        if (dir != juce::File())
            save (picture, dir.getChildFile (file));
        return picture.createComponentSnapshot (picture.getLocalBounds());
    };
    // A standalone picture shown over the editor for the scene.
    const auto host = [&editor] (juce::Component& picture, int width, int height)
    {
        editor.addAndMakeVisible (picture);
        picture.setBounds (20, 60, width, height);
        picture.toFront (false);
    };
    // plain: before `apply`; routed: after it (the picture the old code got wrong).
    const auto scene = [&] (const juce::String& name, juce::Component& picture, const std::function<void()>& apply, double seconds = 0.3)
    {
        BlindspotShot shot;
        shot.name = name;
        run (seconds);
        shot.plain = capture (picture, name + "-plain.png");
        apply();
        run (seconds);
        shot.routed = capture (picture, name + ".png");
        shots.push_back (shot);
    };

    // 1. ENV: a macro shortens ATTACK and lowers SUSTAIN.
    {
        init();
        set ("amp_attack", 1.2f);
        set ("amp_decay", 0.6f);
        set ("amp_sustain", 0.8f);
        set ("amp_release", 1.0f);
        EnvelopeDisplay picture (processor, "amp");
        host (picture, 520, 190);
        route (spareSlot, Mod::Source::Macro1, destinationOf ("amp_attack"), -0.6f);
        route (spareSlot - 1, Mod::Source::Macro1, destinationOf ("amp_sustain"), -0.5f);
        scene ("01-env-attack-sustain", picture, [&] { set ("macro1", 1.0f); });
        unroute (spareSlot - 1);
    }

    // 2. FILTER: ENV AMT sweeping a held note's cutoff, a route on RESO.
    {
        init();
        set ("f1_type", (float) FilterType::LowPass);
        set ("f1_cutoff", 300.0f);
        set ("f1_reso", 0.2f);
        set ("f1_env", 3.0f);
        set ("fe_attack", 0.001f);
        set ("fe_decay", 4.0f);
        set ("fe_sustain", 0.9f);
        FilterDisplay picture (processor);
        host (picture, 600, 220);
        route (spareSlot, Mod::Source::Macro1, destinationOf ("f1_reso"), 0.6f);
        scene ("02-filter-env-reso", picture, [&] { set ("macro1", 1.0f); run (0.05, 60); });
    }

    // 3. FILTER: Comb Damped's teeth die away up the spectrum.
    {
        init();
        set ("f1_cutoff", 400.0f);
        set ("f1_reso", 0.85f);
        FilterDisplay picture (processor);
        host (picture, 600, 220);
        set ("f1_type", (float) FilterType::CombPlus);
        scene ("03-filter-comb-damped", picture, [&] { set ("f1_type", (float) FilterType::CombDamped); });
    }

    // 4. FILTER: AW Z LP at POLES 1 is four biquads (48 dB/oct), not one.
    {
        init();
        set ("f1_cutoff", 1000.0f);
        set ("f1_reso", 0.3f);
        set ("f1_type", (float) FilterType::AwZLow);
        set ("f1_morph", 0.0f);
        FilterDisplay picture (processor);
        host (picture, 600, 220);
        scene ("04-filter-aw-z-poles", picture, [&] { set ("f1_morph", 1.0f); });
    }

    // 5. FILTER: parallel, BALANCE all the way to Filter 1.
    {
        init();
        set ("filters_parallel", 1.0f);
        set ("f1_type", (float) FilterType::LowPass);
        set ("f1_cutoff", 400.0f);
        set ("f2_type", (float) FilterType::HighPass);
        set ("f2_cutoff", 4000.0f);
        FilterDisplay picture (processor);
        host (picture, 600, 220);
        scene ("05-filter-parallel-balance", picture, [&] { set ("filter_balance", -1.0f); });
    }

    // 6. FILTER: Disperser read as an open filter (dimmed).
    {
        init();
        set ("f1_cutoff", 800.0f);
        set ("f1_morph", 0.7f);
        FilterDisplay picture (processor);
        host (picture, 600, 220);
        set ("f1_type", (float) FilterType::LowPass);
        scene ("06-filter-disperser", picture, [&] { set ("f1_type", (float) FilterType::Disperser); });
    }

    // 7. LFO: a macro doubles LFO 1's RATE (the ruler's times).
    {
        init();
        set ("lfo1_sync", 0.0f);
        set ("lfo1_rate", 1.0f);
        LfoDisplay picture (processor, 0);
        host (picture, 520, 180);
        route (spareSlot, Mod::Source::Macro1, destinationOf ("lfo1_rate"), 0.1f);
        scene ("07-lfo-rate", picture, [&] { set ("macro1", 1.0f); });
    }

    // 8-9. OSC 1 wave: WARP ENVELOPE opening a warp at 0; SPEC AMT from velocity.
    {
        init();
        set ("osc1_on", 1.0f);
        set ("osc1_mode", 0.0f);
        set ("osc1_table", 0.0f);
        set ("osc1_frame", 0.0f);
        set ("amp_attack", 0.001f);
        set ("amp_sustain", 1.0f);
        uiSelectOscTab (ctx, 0);
        settle (200);
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (editor, waves);
        WaveDisplay* wave = nullptr;
        for (auto* candidate : waves)
            if (candidate->getOscIndex() == 0 && (wave == nullptr || (visibleInTree (candidate) && ! candidate->isCompact())))
                wave = candidate;
        if (wave != nullptr)
        {
            set ("osc1_warp", 2.0f);
            set ("osc1_warp_amt", 0.0f);
            scene ("08-osc-warp-envelope", *wave, [&] { set ("osc1_pd_env", 1.0f); set ("osc1_pd_env_amt", 0.8f); run (0.05, 60); });
            release();
            set ("osc1_pd_env", 0.0f);
            set ("osc1_warp", 0.0f);
            set ("osc1_spectral", 1.0f);
            set ("osc1_spectral_amt", 0.0f);
            scene ("09-osc-spectral-velocity", *wave, [&]
            {
                route (spareSlot, Mod::Source::Velocity, destinationOf ("osc1_spectral_amt"), 0.85f);
                run (0.05, 60, 1.0f);
            });
        }
    }

    // 10. PLAY SUB picture: COLOUR all the way.
    {
        init();
        set ("subosc_on", 1.0f);
        set ("sub_shape", 0.0f);
        SubWaveDisplay picture (processor);
        host (picture, 220, 70);
        scene ("10-play-sub-colour", picture, [&] { set ("sub_colour", 1.0f); });
    }

    // 11. PLAY WEST picture: a macro on FOLD.
    {
        init();
        set ("west_on", 1.0f);
        set ("west_fold", 0.1f);
        WestDisplay picture (processor);
        host (picture, 260, 110);
        route (spareSlot, Mod::Source::Macro1, destinationOf ("west_fold"), 0.8f);
        scene ("11-play-west-fold", picture, [&] { set ("macro1", 1.0f); });
    }

    // 12. Physical string: POSITION 0.6 (it sounds at 30 % of the string).
    {
        init();
        set ("osc1_mode", 1.0f);
        set ("osc1_excite", 0.0f);
        set ("osc1_string_excite_pos", 0.0f);
        StringPartialsView picture (processor, 0);
        host (picture, 300, 60);
        scene ("12-string-position", picture, [&] { set ("osc1_string_excite_pos", 0.6f); }, 0.6);
    }

    // 13. FM page: OSC 1 > OSC 2 at 0, opened by a macro.
    {
        init();
        set ("osc1_on", 1.0f);
        set ("osc2_on", 1.0f);
        set ("fm_1to2", 0.0f);
        FmDiagram picture (processor);
        host (picture, 420, 300);
        route (spareSlot, Mod::Source::Macro1, destinationOf ("fm_1to2"), 0.7f);
        scene ("13-fm-route-opened", picture, [&] { set ("macro1", 1.0f); });
    }

    init();
    return shots;
}
