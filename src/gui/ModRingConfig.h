#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

#include "../PluginProcessor.h"

struct ModRingConfig
{
    // How a route's amount moves the knob's value: as a share of the knob's
    // travel (plain parameters), in octaves (value x 2^(amount x scale)) or
    // in the knob's own unit (value + amount x scale).
    enum class Unit { travel, octaves, value };

    int destination = 0;
    float scale = 1.0f;
    Unit unit = Unit::travel;

    // The knob's value moved by `amount` (a route's depth x its source), as
    // the voices hear it, kept inside the knob's range.
    float modulated (const juce::NormalisableRange<float>& range, float plain, float amount) const
    {
        switch (unit)
        {
            case Unit::octaves: return juce::jlimit (range.start, range.end, plain * std::exp2 (amount * scale));
            case Unit::value:   return juce::jlimit (range.start, range.end, plain + amount * scale);
            case Unit::travel:  break;
        }
        return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (plain) + amount * scale));
    }
};

// Which modulation destination a knob drives, and how a full-depth route
// moves it. Since 2026-10-23 every one spans the knob's whole range at
// 100 % (Mod::Range); the octave and own-unit ones say so, so the ring
// lands where the sound does on a skewed knob.
inline ModRingConfig modRingConfigFor (const juce::String& id)
{
    using D = Mod::Destination;
    using Unit = ModRingConfig::Unit;
    const auto explicitDest = [] (D d, float scale) { return ModRingConfig { (int) d, scale }; };
    const auto inOctaves = [] (D d, float octaves) { return ModRingConfig { (int) d, octaves, Unit::octaves }; };
    namespace R = Mod::Range;

    struct OscIds { const char* prefix; D pitch, frame, level, pan, detune, spread, warp, blend, start, end; };
    static const OscIds oscs[] {
        { "osc1", D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1Pan, D::Osc1Detune, D::Osc1Spread, D::Osc1Warp,
          D::Osc1Blend, D::Osc1SampleStart, D::Osc1SampleEnd },
        { "osc2", D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2Pan, D::Osc2Detune, D::Osc2Spread, D::Osc2Warp,
          D::Osc2Blend, D::Osc2SampleStart, D::Osc2SampleEnd },
        { "sub", D::SubPitch, D::SubFrame, D::SubLevel, D::SubPan, D::SubDetune, D::SubSpread, D::SubWarp,
          D::SubBlend, D::SubSampleStart, D::SubSampleEnd },
        { "osc4", D::Osc4Pitch, D::Osc4Frame, D::Osc4Level, D::Osc4Pan, D::Osc4Detune, D::Osc4Spread, D::Osc4Warp,
          D::Osc4Blend, D::Osc4SampleStart, D::Osc4SampleEnd },
        { "osc5", D::Osc5Pitch, D::Osc5Frame, D::Osc5Level, D::Osc5Pan, D::Osc5Detune, D::Osc5Spread, D::Osc5Warp,
          D::Osc5Blend, D::Osc5SampleStart, D::Osc5SampleEnd },
        { "osc6", D::Osc6Pitch, D::Osc6Frame, D::Osc6Level, D::Osc6Pan, D::Osc6Detune, D::Osc6Spread, D::Osc6Warp,
          D::Osc6Blend, D::Osc6SampleStart, D::Osc6SampleEnd },
    };

    for (const auto& osc : oscs)
    {
        const juce::String prefix (osc.prefix);

        if (! id.startsWith (prefix + "_"))
            continue;

        const auto field = id.fromFirstOccurrenceOf (prefix + "_", false, false);

        if (field == "semi" || field == "fine") return explicitDest (osc.pitch, 1.0f);
        if (field == "frame")        return explicitDest (osc.frame, 1.0f);
        if (field == "level")        return explicitDest (osc.level, 1.0f);
        if (field == "pan")          return explicitDest (osc.pan, 1.0f);
        if (field == "detune")       return { (int) osc.detune, R::detuneCents, Unit::value };
        if (field == "spread")       return explicitDest (osc.spread, 1.0f);
        if (field == "warp_amt")     return explicitDest (osc.warp, 1.0f);
        if (field == "uni_blend")    return explicitDest (osc.blend, 1.0f);
        if (field == "sample_start") return explicitDest (osc.start, 1.0f);
        if (field == "sample_end")   return explicitDest (osc.end, 1.0f);
    }

    struct OctaveEntry { const char* id; D destination; float octaves; };
    static const OctaveEntry octaveEntries[] {
        { "f1_cutoff", D::Filter1Cutoff, R::cutoffOctaves }, { "f2_cutoff", D::Filter2Cutoff, R::cutoffOctaves },
        { "lfo1_rate", D::Lfo1Rate, R::lfoRateOctaves }, { "lfo2_rate", D::Lfo2Rate, R::lfoRateOctaves },
        { "lfo3_rate", D::Lfo3Rate, R::lfoRateOctaves }, { "lfo4_rate", D::Lfo4Rate, R::lfoRateOctaves },
        { "mseg_rate", D::MsegRate, R::msegRateOctaves },
        { "amp_attack", D::AmpAttack, R::attackOctaves }, { "amp_decay", D::AmpDecay, R::decayOctaves },
        { "amp_release", D::AmpRelease, R::releaseOctaves },
        { "fe_attack", D::FeAttack, R::attackOctaves }, { "fe_decay", D::FeDecay, R::decayOctaves },
        { "fe_release", D::FeRelease, R::releaseOctaves },
        { "me_attack", D::MeAttack, R::attackOctaves }, { "me_decay", D::MeDecay, R::decayOctaves },
        { "me_release", D::MeRelease, R::releaseOctaves },
        { "f2e_attack", D::F2eAttack, R::attackOctaves }, { "f2e_decay", D::F2eDecay, R::decayOctaves },
        { "f2e_release", D::F2eRelease, R::releaseOctaves },
        { "e4_attack", D::E4Attack, R::attackOctaves }, { "e4_decay", D::E4Decay, R::decayOctaves },
        { "e4_release", D::E4Release, R::releaseOctaves },
        { "fx_comb_freq", D::FxCombFreq, R::combOctaves }, { "fx_phaser_rate", D::FxPhaserRate, R::phaserOctaves },
    };

    for (const auto& entry : octaveEntries)
        if (id == entry.id)
            return inOctaves (entry.destination, entry.octaves);

    struct Entry { const char* id; D destination; float scale; };
    static const Entry entries[] {
        { "noise_level", D::NoiseLevel, 1.0f },
        { "f1_reso", D::Filter1Reso, 1.0f },
        { "f1_drive", D::Filter1Drive, 1.0f }, { "f1_env", D::Filter1Env, 1.0f }, { "f1_fm", D::Filter1Fm, 1.0f },
        { "f1_morph", D::Filter1Morph, 1.0f },
        { "f2_reso", D::Filter2Reso, 1.0f },
        { "f2_drive", D::Filter2Drive, 1.0f }, { "f2_env", D::Filter2Env, 1.0f }, { "f2_fm", D::Filter2Fm, 1.0f },
        { "f2_morph", D::Filter2Morph, 1.0f },
        { "fm_amount", D::FmAmount, 1.0f }, { "fm_feedback", D::FmFeedback, 1.0f },
        { "fm_1to2", D::Fm1to2, 1.0f }, { "fm_1to3", D::Fm1to3, 1.0f }, { "fm_2to3", D::Fm2to3, 1.0f },
        { "fm_3to1", D::Fm3to1, 1.0f }, { "fm_3to2", D::Fm3to2, 1.0f },
        { "fm_fb2", D::Fm2Feedback, 1.0f }, { "fm_fb3", D::Fm3Feedback, 1.0f },
        { "ring_mod", D::RingMod, 1.0f }, { "drift", D::Drift, 1.0f },
        { "res_amount", D::ResAmount, 1.0f }, { "res_decay", D::ResDecay, 1.0f }, { "res_offset", D::ResOffset, 1.0f },
        { "amp_sustain", D::AmpSustain, 1.0f }, { "fe_sustain", D::FeSustain, 1.0f }, { "me_sustain", D::MeSustain, 1.0f },
        { "f2e_sustain", D::F2eSustain, 1.0f }, { "e4_sustain", D::E4Sustain, 1.0f },
        { "fx_drive_amount", D::FxDriveAmount, 1.0f }, { "fx_crush_mix", D::FxCrushMix, 1.0f },
        { "fx_chorus_depth", D::FxChorusDepth, 1.0f }, { "fx_delay_mix", D::FxDelayMix, 1.0f },
        { "fx_delay_feedback", D::FxDelayFeedback, 1.0f }, { "fx_smear_mix", D::FxSmearMix, 1.0f },
        { "fx_freeze_mix", D::FxFreezeMix, 1.0f }, { "fx_reverb_mix", D::FxReverbMix, 1.0f },
        { "fx_reverb_size", D::FxReverbSize, 1.0f },
    };

    for (const auto& entry : entries)
        if (id == entry.id)
            return explicitDest (entry.destination, entry.scale);

    for (int lfo = 4; lfo < Mod::numLfoSources; ++lfo)
        if (id == "lfo" + juce::String (lfo + 1) + "_rate")
            return inOctaves (Mod::lfoRateDestinationFor (lfo), R::lfoRateOctaves);

    // Everything else that can be modulated is a plain-parameter destination,
    // offset in the knob's own normalised range.
    return { Mod::destinationForParamId (id), 1.0f };
}

// A parameter's value with whatever modulates it right now (LFOs, envelopes,
// macros, the newest note's per-voice routes), as the voices hear it: what a
// picture should draw (ilana 2026-11-21, "check for blindspots like this":
// pictures that read the bare knob missed every route into it). `plain` is
// the knob's own value (a held or dragged knob passes its live value).
inline float modulatedParamValue (const IlanaSynthAudioProcessor& processor, const juce::String& id, float plain)
{
    if (const auto config = modRingConfigFor (id); config.destination != 0)
        if (const auto* parameter = processor.apvts.getParameter (id))
            return config.modulated (parameter->getNormalisableRange(), plain, processor.getModDisplay (config.destination));

    return plain;
}

inline float modulatedParamValue (const IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    const auto* raw = processor.apvts.getRawParameterValue (id);
    return modulatedParamValue (processor, id, raw != nullptr ? raw->load() : 0.0f);
}
