// The FX rack's pictures draw what the sound does (display blind spots, 2026-11-21): a knob the mod matrix or
// an FX LFO moves is drawn at the value the effect runs, a second copy of an effect reads its own engine, and
// the mixes and the gate's swing, Euclid and moving patterns show. Included by Snapshot.cpp before the group
// table; run with `--uitest --only FX_DISPLAY`. ILANA_UITEST_SHOTS=dir (and ILANA_SHOT_SUFFIX=before|after)
// writes the pictures as dir/NN-name-SUFFIX.png.
#pragma once

#include "gui/pages/FxWidgets.h"

namespace FxBlindUi
{
// Audio for `blocks` blocks with a note held (the modulation and the meters read the audio thread's work).
inline void play (IlanaSynthAudioProcessor& processor, int blocks)
{
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
    for (int block = 0; block < blocks; ++block)
    {
        audio.clear();
        processor.processBlock (audio, midi);
        midi.clear();
    }
}

inline void release (IlanaSynthAudioProcessor& processor)
{
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    processor.processBlock (audio, off);
}

inline int differs (const juce::Image& x, const juce::Image& y)
{
    auto count = 0;
    for (int j = 0; j < juce::jmin (x.getHeight(), y.getHeight()); ++j)
        for (int i = 0; i < juce::jmin (x.getWidth(), y.getWidth()); ++i)
            count += x.getPixelAt (i, j) != y.getPixelAt (i, j) ? 1 : 0;
    return count;
}

// Before the slot existed on the display the call does nothing (so the "before" pictures render from the old tree).
template <typename Display>
void showSlot (Display& display, int slot)
{
    if constexpr (requires { display.setSlot (slot); })
        display.setSlot (slot);
}
} // namespace FxBlindUi

void uiGroup_FX_DISPLAY (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    using namespace FxBlindUi;
    const UiKeepPatch keep (processor);
    const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
    const auto suffix = juce::SystemStats::getEnvironmentVariable ("ILANA_SHOT_SUFFIX", "after");

    // (Every FX parameter this group sets is on the Init preset it starts from: the group loads that, and ends on it.)
    const auto initIndex = juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init"));
    processor.loadFactoryPreset (initIndex);
    settle (200);

    const auto loadFx = [&] (int type)
    {
        processor.setFxPatcherOn (false);
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            processor.assignFxSlot (slot, slot == 1 ? type : 0);
        settle (150);
    };
    const auto display = [&] (int type, int width = 440, int height = 96)
    {
        auto made = std::make_unique<FxDisplay> (processor);
        made->setSize (width, height);
        made->setType (type, fxColour (type));
        return made;
    };
    const auto snap = [&] (FxDisplay& shown)
    {
        shown.refreshNow();
        return shown.createComponentSnapshot (shown.getLocalBounds());
    };
    const auto shot = [&] (juce::Component& component, const juce::String& name)
    {
        if (shotsDir.isNotEmpty())
        {
            juce::File (shotsDir).createDirectory();
            save (component, juce::File (shotsDir).getChildFile (name + "-" + suffix + ".png"));
        }
    };
    const auto spareSlot = [&]
    {
        for (int slot = 0; slot < Mod::maxSlots; ++slot)
            if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::None && routing.destination == 0)
                return slot;
        return 0;
    };
    const auto route = [&] (int slot, Mod::Destination destination)
    {
        processor.setModSlotValue (slot, "src", (float) (int) Mod::Source::Macro1);
        processor.setModSlotValue (slot, "dst", (float) (int) destination);
        processor.setModSlotValue (slot, "amt", 1.0f);
    };
    const auto macro = [&] (float value) { setParam ("macro1", value); play (processor, 8); };

    // ----- The mod matrix moves the picture (FxDriveAmount, FxPhaserRate) -----
    const auto modSlot = spareSlot();
    loadFx (2);
    setParam ("fx_drive_on", 1.0f);
    setParam ("fx_drive_amount", 3.0f);
    setParam ("fx_drive_mix", 1.0f);
    route (modSlot, Mod::Destination::FxDriveAmount);
    {
        auto drive = display (2);
        macro (0.0f);
        const auto lowImage = snap (*drive);
        macro (1.0f);
        const auto highImage = snap (*drive);
        expect (differs (lowImage, highImage) > 100, "FX_DISPLAY: the drive's transfer curve follows a mod route on DRIVE AMOUNT ("
                                                         + juce::String (differs (lowImage, highImage)) + " px)");
#ifndef FXBLIND_BASELINE
        macro (0.0f);
        drive->refreshNow();
        const auto lowSignature = drive->getSettingsSignature();
        macro (0.5f);
        drive->refreshNow();
        expect (drive->getSettingsSignature() != lowSignature, "FX_DISPLAY: the poll's signature holds the modulated amount (the picture repaints)");
#endif
        macro (1.0f);
        shot (*drive, "01-drive-amount-modulated");
    }

    loadFx (6);
    setParam ("fx_phaser_on", 1.0f);
    setParam ("fx_phaser_rate", 0.5f);
    setParam ("fx_phaser_mix", 1.0f);
    route (modSlot, Mod::Destination::FxPhaserRate);
    {
        auto phaser = display (6);
        macro (0.0f);
        const auto lowImage = snap (*phaser);
        macro (1.0f);
        const auto highImage = snap (*phaser);
        expect (differs (lowImage, highImage) > 20, "FX_DISPLAY: the phaser's RATE reading follows a mod route (" + juce::String (differs (lowImage, highImage)) + " px)");
        shot (*phaser, "02-phaser-rate-modulated");
    }
    processor.clearModSlot (modSlot);
    setParam ("macro1", 0.0f);

    // ----- The delay: echoes by MIX, the right row at its own time -----
    loadFx (9);
    setParam ("fx_delay_on", 1.0f);
    setParam ("fx_delay_sync", 0.0f);
    setParam ("fx_delay_time", 300.0f);
    setParam ("fx_delay_time_r", 450.0f);
    setParam ("fx_delay_pingpong", 0.0f);
    setParam ("fx_delay_feedback", 0.6f);
    setParam ("fx_taps_on", 1.0f);
    setParam ("fx_taps_pattern", 1.0f);
    setParam ("fx_taps_mix", 0.5f);
    {
        auto delay = display (9);
        setParam ("fx_delay_mix", 0.2f);
        const auto quiet = snap (*delay);
        shot (*delay, "03-delay-mix-and-right-taps");
        setParam ("fx_delay_mix", 0.9f);
        const auto loud = snap (*delay);
        expect (differs (quiet, loud) > 100, "FX_DISPLAY: the delay's echoes follow MIX (" + juce::String (differs (quiet, loud)) + " px)");
        setParam ("fx_delay_mix", 0.2f);
    }

    // ----- Mixes the pictures did not draw -----
    struct MixCase { int type; const char* mixId; const char* name; std::vector<std::pair<const char*, float>> settings; };
    const MixCase mixCases[] {
        { 25, "fx_ring_mix", "04-ring-mod-mix", { { "fx_ring_freq", 300.0f } } },
        { 24, "fx_shifter_mix", "05-freq-shifter-mix", { { "fx_shifter_shift", 150.0f } } },
        { 14, "fx_flanger_mix", "06-flanger-mix", {} },
        { 15, "fx_dim_mix", "07-dimension-mix", {} },
        { 7, "fx_chorus_mix", "08-chorus-mix", { { "fx_chorus_on", 1.0f } } },
        { 10, "fx_stutter_mix", "09-stutter-mix", {} },
        { 28, "fx_feedback_mix", "10-feedback-loop-mix", { { "fx_feedback_amount", 0.8f }, { "fx_feedback_delay", 40.0f }, { "fx_feedback_tone", 0.7f } } },
    };
    for (const auto& mixCase : mixCases)
    {
        loadFx (mixCase.type);
        for (const auto& [id, value] : mixCase.settings)
            setParam (id, value);
        auto shown = display (mixCase.type);
        setParam (mixCase.mixId, 1.0f);
        const auto full = snap (*shown);
        setParam (mixCase.mixId, 0.3f);
        const auto low = snap (*shown);
        expect (differs (full, low) > 30, juce::String ("FX_DISPLAY: the picture of ") + mixCase.name + " follows its MIX knob (" + juce::String (differs (full, low)) + " px)");
        shot (*shown, mixCase.name);
        setParam (mixCase.mixId, 0.5f);
    }

    // The Airwindows echo's picture follows its module MIX.
    {
        auto echoAlgorithm = -1;
        for (int index = 0; index < airwindows::count() && echoAlgorithm < 0; ++index)
            if (juce::String (airwindows::registry()[(size_t) index].category) == "Delay")
                echoAlgorithm = index;
        expect (echoAlgorithm >= 0, "FX_DISPLAY: an Airwindows echo to draw");
        if (echoAlgorithm >= 0)
        {
            loadFx (30);
            setParam ("fx_aw_algo", (float) echoAlgorithm);
            auto shown = display (30);
            setParam ("fx_aw_mix", 1.0f);
            const auto full = snap (*shown);
            setParam ("fx_aw_mix", 0.3f);
            const auto low = snap (*shown);
            expect (differs (full, low) > 30, "FX_DISPLAY: the Airwindows echo's impulse follows its MIX (" + juce::String (differs (full, low)) + " px)");
            shot (*shown, "11-airwindows-echo-mix");
        }
    }

    // ----- An FX LFO cable moves the picture of its slot -----
    processor.loadFactoryPreset (initIndex);
    settle (200);
    processor.addFxEffect (2);
    processor.setFxPatcherOn (true);
    setParam ("fx_drive_amount", 3.0f);
    setParam ("fx_drive_mix", 1.0f);
    {
        const auto lfo = processor.addFxLfo (0.0f, 0.0f);
        processor.setFxLfoCable (lfo, 0, true);
        processor.assignFxLfo (lfo, 0, "fx_drive_amount", true, 1.0f);
        play (processor, 40);
        const auto offset = processor.getFxLfoOffset (0, "fx_drive_amount");
        expect (std::abs (offset) > 0.05f, "FX_DISPLAY: the FX LFO moves DRIVE AMOUNT (" + juce::String (offset) + ")");
        auto own = display (2), plain = display (2);
        showSlot (*own, 0);
        const auto moved = snap (*own), still = snap (*plain);
        expect (differs (moved, still) > 100, "FX_DISPLAY: a card in a slot with an FX LFO cable draws the moved value ("
                                                  + juce::String (differs (moved, still)) + " px)");
        shot (*own, "12-drive-fx-lfo");
    }

    // ----- A second copy of an effect reads its own engine -----
    processor.loadFactoryPreset (initIndex);
    settle (200);
    processor.addFxEffect (4);
    processor.addFxEffect (4); // (the new one is in front; the first keeps its own copy of the settings)
    processor.setFxPatcherOn (true);
    settle (200);
    setParam ("fx_comp_threshold", -40.0f);
    setParam ("fx_comp_ratio", 8.0f);
    processor.setFxInstanceValue (0, "fx_comp_threshold", 0.0f);
    play (processor, 60);
    {
        auto copy = display (4), front = display (4);
        showSlot (*copy, 0);
        showSlot (*front, 1);
        copy->refreshNow();
        front->refreshNow();
#ifndef FXBLIND_BASELINE
        const auto copyGain = processor.getCompGainReduction (0), frontGain = processor.getCompGainReduction (1);
        expect (frontGain < 0.97f && copyGain > 0.99f, "FX_DISPLAY: the engines of two compressors report their own gain reduction (copy "
                                                           + juce::String (copyGain) + ", in front " + juce::String (frontGain) + ")");
        expect (front->getMeterDb() < -0.3f && copy->getMeterDb() > -0.3f, "FX_DISPLAY: a copy's card shows its own meter, not the front card's (copy "
                                                                              + juce::String (copy->getMeterDb()) + " dB, in front " + juce::String (front->getMeterDb()) + " dB)");
#endif
        shot (*copy, "13-compressor-copy-meter");
    }
    release (processor);

    // ----- The trance gate: SWING, EUCLID and the moving patterns -----
    processor.loadFactoryPreset (initIndex);
    settle (200);
    {
        GateGrid grid (processor);
        grid.setSize (480, 90);
        const auto grab = [&] { return grid.createComponentSnapshot (grid.getLocalBounds()); };
        setParam ("fx_gate_pattern", 0.0f);
        setParam ("fx_gate_steps", 16.0f);
        setParam ("fx_gate_swing", 0.0f);
        const auto straight = grab();
        shot (grid, "14-gate-straight");
        setParam ("fx_gate_swing", 0.35f);
        const auto swung = grab();
        expect (differs (straight, swung) > 100, "FX_DISPLAY: the gate grid draws SWING (" + juce::String (differs (straight, swung)) + " px)");
        shot (grid, "15-gate-swing");
        setParam ("fx_gate_swing", 0.0f);
        setParam ("euc_on", 1.0f);
        setParam ("euc_target", 2.0f);
        setParam ("euc_steps", 24.0f);
        setParam ("euc_hits", 7.0f);
        const auto euclid = grab();
        expect (differs (straight, euclid) > 100, "FX_DISPLAY: the gate grid draws the Euclid rhythm that takes the gate over (" + juce::String (differs (straight, euclid)) + " px)");
        shot (grid, "16-gate-euclid");
        setParam ("euc_on", 0.0f);
        setParam ("fx_gate_pattern", 4.0f);
        const auto random = grab();
        expect (differs (straight, random) > 100, "FX_DISPLAY: the gate grid says Random is a new set of steps every cycle (" + juce::String (differs (straight, random)) + " px)");
        shot (grid, "17-gate-random");
        setParam ("fx_gate_pattern", 0.0f);
    }

    processor.setFxPatcherOn (false);
    processor.loadFactoryPreset (neuroWobble);
    settle (300);
}
