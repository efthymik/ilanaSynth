#pragma once

// The showcase pack (2026-11-08, blind-spots review): the features no
// factory preset played yet (VECTOR's Triangle, Pentagon and Hexagon,
// macros 5-8, the Modal Piano exciter, the Octaver, Tilt, Utility and
// Filter effects) and the sounds ilana's taste was missing (hoover, reese
// screech, brass, chip and pitched-vocal leads, a kick, a clap, a hat).
// Appended after every earlier preset (indices never move). Their levels
// are fitted with the level tools after a build (PresetTrims.h).
// Included from PresetLibrary.h inside Presets::Library, after the M10 pack
// (it uses that pack's tables and exciters).

inline void addShowcasePresets (const std::function<void (const FactoryPreset&)>& add)
{
    using B = Builder;
    constexpr int FxFilter = 42; // the synth's filters as an effect
    enum VectorShapes { VTriangle = 1, VPentagon = 3, VHexagon = 4 };

    // ---- VECTOR shapes ------------------------------------------------------

    // Six voices on the hexagon's corners: two choirs, air, a warm bed an
    // octave down, glass an octave up, a whisper. The path walks it slowly.
    add (B ("Hexagon Choir Pad", "Pad")
             .osc1 (VowelsFemale, 0.3f, 0.6f).osc2 (PadChoir, 0.4f, 0.6f).osc3 (AirPad, 0.3f, 0.6f)
             .osc (4, WarmPad, 0.4f, 0.6f, -12).osc (5, GlassHarmonics, 0.3f, 0.5f, 12).osc (6, Whisper, 0.5f, 0.6f)
             .unison (1, 3, 12.0f, 0.8f).unison (2, 3, 10.0f, 0.8f)
             .set ("vec_on", 1).set ("vec_shape", VHexagon)
             .set ("vec_a", 0).set ("vec_b", 1).set ("vec_c", 2).set ("vec_d", 3).set ("vec_e", 4).set ("vec_f", 5)
             .set ("vec_path", 1).set ("vec_rate", 0.05f).set ("vec_drift", 0.2f)
             .filter1 (LP, 6000.0f, 0.1f).amp (1.0f, 0.5f, 1.0f, 2.5f)
             .macro (1, "X", { { param ("vec_x"), 0.4f } })
             .macro (2, "Y", { { param ("vec_y"), 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .macro (5, "DRIFT", { { param ("vec_drift"), 0.6f } })
             .macro (6, "SWELL", { { D::AmpAttack, 0.3f } })
             .macro (7, "GLASS", { { D::Osc5Level, 0.4f } })
             .macro (8, "WIDTH", { { D::Osc1Spread, 0.2f }, { D::Osc2Spread, 0.2f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.3f, 0.3f).reverb (Hall, 0.85f, 0.35f));

    // Three corners: a saw-to-square, a formant ladder and a synced square.
    // The wheel crosses the triangle, pressure climbs it.
    add (B ("Triangle Vector Lead", "Lead")
             .osc1 (SawToSquare, 0.3f, 0.7f).osc2 (FormantLadder, 0.4f, 0.7f).osc (4, SquareSync, 0.5f, 0.7f)
             .set ("vec_on", 1).set ("vec_shape", VTriangle).set ("vec_a", 0).set ("vec_b", 1).set ("vec_c", 3)
             .filter1 (LadderLP, 3000.0f, 0.2f, 1.5f, 1.4f, 0.4f)
             .fenv (0.003f, 0.5f, 0.5f, 0.3f).amp (0.004f, 0.3f, 0.9f, 0.3f)
             .mod (Wheel, Target (param ("vec_x"), 0.5f)).mod (AT, Target (param ("vec_y"), 0.5f))
             .legato (0.04f)
             .macro (1, "X", { { param ("vec_x"), 0.4f } })
             .macro (2, "Y", { { param ("vec_y"), 0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .macro (5, "BITE", { { D::Filter1Reso, 0.35f }, { D::Filter1Drive, 0.4f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.4f, 0.12f));

    // Five struck corners: glass, a bell an octave up, a tine, a marimba and
    // an FM 1:2. Each note lands somewhere else on the pentagon.
    add (B ("Pentagon Glass Keys", "Keys")
             .osc1 (GlassHarmonics, 0.3f, 0.6f).osc2 (BellT, 0.3f, 0.5f, 12).osc3 (TineT, 0.3f, 0.6f)
             .osc (4, MarimbaT, 0.4f, 0.6f).osc (5, Fm12, 0.3f, 0.5f)
             .set ("vec_on", 1).set ("vec_shape", VPentagon)
             .set ("vec_a", 0).set ("vec_b", 1).set ("vec_c", 2).set ("vec_d", 3).set ("vec_e", 4)
             .set ("vec_path", 1).set ("vec_rate", 0.12f)
             .filter1 (LP, 7000.0f, 0.0f).amp (0.001f, 2.5f, 0.0f, 1.0f)
             .macro (1, "X", { { param ("vec_x"), 0.4f } })
             .macro (2, "Y", { { param ("vec_y"), 0.4f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .macro (5, "MOTION", { { param ("vec_rate"), 0.4f } })
             .macro (6, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.25f, 0.2f).reverb (Room, 0.55f, 0.2f));

    // ---- Modal Piano, Tilt, Octaver, Utility, Filter FX ---------------------

    // The Modal Piano played soft: a lighter hammer, more damping, the top
    // tilted away and a small room. Its knobs otherwise as Modal Grand's.
    add (B ("Hushed Grand", "Keys")
             .piano (1, 0.8f, 0.3f, 1, 0.0f, 0.0f, 0.35f, 0.0f, 0.5f, 0.25f, 0.5f)
             .set ("osc1_excite", 11)
             .set ("osc2_on", 0).set ("sub_on", 0)
             .filter1 (LP, 20000.0f, 0.0f)
             .amp (0.001f, 5.0f, 1.0f, 1.0f).velocity (0.2f)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "DAMP", { { param ("osc1_string_damp"), 0.4f } })
             .macro (3, "TILT", { { param ("fx_tilt"), 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxTilt, FxLimiter, FxReverb }).set ("fx_tilt", -0.35f).limiter (-0.5f).reverb (Room, 0.45f, 0.18f)
             .master (7.0f));

    // A reese with the Octaver an octave under it, then made mono below the
    // mix by Utility so the low end stays in the middle.
    add (B ("Octave Drop Bass", "Bass")
             .osc1 (ReeseT, 0.4f, 0.75f).unison (1, 2, 12.0f, 0.4f).sub (0, 0.3f)
             .filter1 (LP, 700.0f, 0.2f, 1.5f, 1.6f, 0.3f).fenv (0.001f, 0.3f, 0.3f, 0.2f)
             .amp (0.002f, 0.3f, 0.9f, 0.12f).mono (0.02f)
             .lfo (1, Tri, 0.4f).mod (Lfo1, D::Osc1Frame, 0.2f)
             .macro (1, "OCTAVE", { { param ("fx_octaver_mix"), 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (3, "GRIT", { { D::FxDriveAmount, 0.5f }, { D::Filter1Drive, 0.3f } })
             .macro (4, "MOVE", { { D::Lfo1Rate, 0.4f }, { D::Osc1Frame, 0.3f } })
             .fx ({ FxOctaver, FxDrive, FxUtility }).set ("fx_octaver_mix", 0.35f).driveFx (2.0f, 0.3f)
             .set ("fx_util_mono", 1));

    // A trance pluck: a hypersaw through a fast filter envelope, Tilt
    // lifting the top before a dotted echo.
    add (B ("Trance Pluck", "Pluck")
             .osc1 (Basic, 0.0f, 0.7f).unison (1, 7, 25.0f, 0.8f, Hypersaw)
             .filter1 (LP, 700.0f, 0.2f, 4.0f, 1.2f, 0.4f).fenv (0.001f, 0.22f, 0.0f, 0.2f)
             .amp (0.001f, 0.45f, 0.0f, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "DECAY", { { D::FeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (3, "TILT", { { param ("fx_tilt"), 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxTilt, FxDelay, FxReverb }).set ("fx_tilt", 0.25f)
             .delay (D1_8D, 0.4f, 0.25f, true).reverb (Hall, 0.6f, 0.15f));

    // ---- Leads ilana's taste was missing --------------------------------------

    // The rave hoover: wide detuned pulses with an octave under them, a pitch
    // scoop into every note, chorus and a little drive.
    add (B ("Rave Hoover", "Lead")
             .osc1 (Pwm, 0.4f, 0.7f).unison (1, 7, 35.0f, 0.8f)
             .osc2 (Pwm, 0.6f, 0.5f, -12).unison (2, 3, 20.0f, 0.6f)
             .menv (0.0f, 0.18f, 0.0f, 0.1f).mod (ModEnv, D::Osc1Pitch, -0.04f).mod (ModEnv, D::Osc2Pitch, -0.04f)
             .lfo (1, Tri, 0.7f).mod (Lfo1, D::Osc1Frame, 0.3f).mod (Lfo1, D::Osc2Frame, 0.2f)
             .filter1 (LP, 6000.0f, 0.1f).amp (0.005f, 0.3f, 0.9f, 0.3f)
             .legato (0.06f)
             .macro (1, "SCOOP", { { D::MeDecay, 0.4f } })
             .macro (2, "PWM", { { D::Osc1Frame, 0.4f }, { D::Lfo1Rate, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxDrive, FxReverb }).chorus (0.6f, 0.5f, 0.45f).driveFx (1.6f, 0.3f)
             .reverb (Hall, 0.6f, 0.15f));

    // A reese screech (dnb): growl and neuro tables a fifth apart, FM for
    // teeth, then the band-pass Filter effect swept by a sixteenth LFO.
    add (B ("Reese Screech", "Lead")
             .osc1 (NeuroT, 0.4f, 0.7f).unison (1, 2, 14.0f, 0.5f)
             .osc2 (GrowlT, 0.5f, 0.5f, 7).fm (0.25f)
             .lfoSync (1, Tri, D1_16).mod (Lfo1, D::Osc1Frame, 0.35f).mod (Lfo1, Target (param ("fx_filter_cutoff"), 0.25f))
             .amp (0.003f, 0.3f, 0.9f, 0.2f).mono (0.02f)
             .macro (1, "SCREECH", { { D::FmAmount, 0.4f } })
             .macro (2, "SWEEP", { { param ("fx_filter_cutoff"), 0.4f } })
             .macro (3, "PEAK", { { param ("fx_filter_reso"), 0.25f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxFilter, FxDrive, FxOtt, FxLimiter })
             .set ("fx_filter_type", 1).set ("fx_filter_cutoff", 1800.0f).set ("fx_filter_reso", 0.7f)
             .set ("fx_filter_drive", 2.5f).driveFx (2.5f, 0.4f).ott (0.35f, 0.4f).limiter (-1.0f));

    // Synth brass: a brass table and a saw-to-square under a swelling filter,
    // brighter the harder it is played; the wheel adds vibrato.
    add (B ("Synth Brass Lead", "Lead")
             .osc1 (BrassT, 0.4f, 0.7f).unison (1, 3, 10.0f, 0.6f)
             .osc2 (SawToSquare, 0.6f, 0.35f).unison (2, 2, 8.0f, 0.5f)
             .filter1 (LP, 1400.0f, 0.15f, 2.5f, 1.3f, 0.4f).fenv (0.06f, 0.5f, 0.5f, 0.3f).filterVelocity (0.5f)
             .amp (0.02f, 0.3f, 0.85f, 0.3f)
             .lfo (1, Sine, 5.3f).mod (Lfo1, Target (D::Osc1Pitch, 0.012f), 0.0f, Wheel).mod (Lfo1, Target (D::Osc2Pitch, 0.012f), 0.0f, Wheel)
             .mono (0.02f)
             .macro (1, "BRASS", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "SWELL", { { D::FeAttack, 0.4f }, { D::AmpAttack, 0.2f } })
             .macro (3, "BITE", { { D::Filter1Reso, 0.3f }, { D::Filter1Drive, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.25f, 0.25f).reverb (Plate, 0.5f, 0.15f));

    // Crushed chip square (Crystal Castles): a quantised pulse into the
    // bit crusher; the wheel turns on an octave trill.
    add (B ("Arcade Pulse Lead", "Lead")
             .osc1 (Pwm, 0.5f, 0.7f).warp (1, WQuantize, 0.6f)
             .filter1 (LP, 12000.0f, 0.0f).amp (0.001f, 0.2f, 0.85f, 0.12f)
             .lfo (1, Square, 15.0f).mod (Lfo1, Target (D::Osc1Pitch, 0.25f), 0.0f, Wheel)
             .mono (0.0f)
             .macro (1, "DUTY", { { D::Osc1Frame, 0.5f } })
             .macro (2, "BITS", { { D::FxCrushMix, 0.4f } })
             .macro (3, "CRUNCH", { { D::Osc1Warp, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxCrush, FxDelay }).crush (6.0f, 4.0f, 0.6f).delay (D1_8D, 0.3f, 0.15f, true));

    // A sustained pitched vocal lead (Arca, Jane Remover): vowel tables that
    // drift between shapes, the Vowel effect, a quick glide between notes.
    add (B ("Vox Glide Lead", "Lead")
             .osc1 (VowelsFemale, 0.3f, 0.7f).unison (1, 3, 8.0f, 0.5f)
             .osc2 (Diphthong, 0.5f, 0.35f, 12)
             .lfo (1, SmoothRandom, 0.3f).mod (Lfo1, D::Osc1Frame, 0.3f).mod (Lfo1, D::Osc2Frame, 0.2f)
             .lfo (2, Sine, 5.5f).mod (Lfo2, Target (D::Osc1Pitch, 0.012f), 0.0f, Wheel).mod (Lfo2, Target (D::Osc2Pitch, 0.012f), 0.0f, Wheel)
             .filter1 (LP, 6500.0f, 0.1f).amp (0.03f, 0.3f, 0.9f, 0.4f)
             .legato (0.04f)
             .macro (1, "VOWEL", { { D::Osc1Frame, 0.5f }, { param ("fx_vowel_morph"), 0.5f } })
             .macro (2, "THROAT", { { D::Osc2Level, 0.4f } })
             .macro (3, "AIR", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.25f }, { D::FxReverbMix, 0.3f } })
             .fx ({ FxVowel, FxChorus, FxDelay, FxReverb }).set ("fx_vowel_morph", 0.4f).set ("fx_vowel_mix", 0.4f)
             .chorus (0.3f, 0.3f, 0.3f).delay (D1_4, 0.3f, 0.15f, true).reverb (Hall, 0.7f, 0.2f));

    // ---- Drums ----------------------------------------------------------------

    // A rave kick: a sine falling two octaves in 80 ms, a click, drive.
    add (B ("Rave Kick", "Drums")
             .osc1 (SineT, 0.0f, 0.95f)
             .menv (0.0f, 0.08f, 0.0f, 0.05f, -0.6f).mod (ModEnv, D::Osc1Pitch, 0.5f)
             .env4 (0.0f, 0.008f, 0.0f, 0.01f).mod (Env4, D::NoiseLevel, 0.4f)
             .amp (0.0f, 0.5f, 0.0f, 0.1f, -0.3f)
             .filter1 (LP, 9000.0f, 0.0f).voices (1)
             .macro (1, "PUNCH", { { D::Osc1Pitch, 0.15f }, { D::MeDecay, 0.3f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.5f } })
             .macro (3, "CLICK", { { D::NoiseLevel, 0.3f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.6f } })
             .fx ({ FxDrive, FxComp, FxLimiter }).driveFx (3.0f, 0.5f).comp (-18.0f, 4.0f, 6.0f).limiter (-0.5f).master (0.0f));

    // A snapping clap: band-passed noise in a short burst, a small plate.
    add (B ("Snap Clap", "Drums")
             .osc1 (SineT, 0.0f, 0.0f).noise (0.9f)
             .filter1 (BP, 1300.0f, 0.3f, 0.0f, 1.3f)
             .amp (0.0f, 0.18f, 0.0f, 0.12f, -0.6f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (2, "SNAP", { { D::Filter1Reso, 0.3f } })
             .macro (3, "TAIL", { { D::AmpDecay, 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxComp, FxReverb }).comp (-18.0f, 4.0f, 6.0f).reverb (Plate, 0.3f, 0.15f).master (0.0f));

    // A closed hat: high-passed noise over a metallic FM tone, very short.
    add (B ("Closed Hat", "Drums")
             .osc1 (FmMetal, 0.8f, 0.4f, 24).osc2 (SineT, 0.0f, 0.0f, 31).modOnly (2).fmRoute (2, 1, 0.7f)
             .noise (0.8f)
             .filter1 (HP, 7000.0f, 0.2f)
             .amp (0.0f, 0.05f, 0.0f, 0.04f, -0.6f)
             .macro (1, "OPEN", { { D::AmpDecay, 0.6f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "METAL", { { D::FmAmount, 0.3f } })
             .macro (4, "WIDTH", { { param ("fx_width"), 0.4f } })
             .fx ({ FxWidener }).width (1.3f, 1.0f).master (0.0f));
}
