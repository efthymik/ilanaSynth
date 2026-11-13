#pragma once

// The Modal Bowed presets (2026-11-12): the violin, the viola and the cello
// of the Modal Bowed exciter, set up so the engine is heard as fitted.
// The Init patch's F1 (12 kHz, 12 dB low-pass) dulled everything above 9 kHz
// and its AMP release cut the bow's own let-go, so here FILTER 1 is wide open
// (20 kHz, no resonance), the AMP holds at 1 with a 5 s release (the bow's
// lift-off sets the tail; the attack stays 0 for the same reason), the SUB and
// OSC 2 are off and there is no effect (dry, for fidelity). Aftertouch is the
// exciter's own (it adds to the bow pressure); the mod wheel's vibrato comes
// from the Lead category's default expression, as for Solo Cello. SIZE: violin 1,
// viola 1.17, cello 2.1 (the three fitted bodies). Appended after every earlier
// preset (indices never move). Included from PresetLibrary.h inside Presets::Library.

inline void addModalBowedPresets (const std::function<void (const FactoryPreset&)>& add)
{
    using B = Builder;

    const auto bowed = [&add] (const char* name, float size)
    {
        add (B (name, "Lead")
                 .set ("osc1_on", 1).set ("osc1_mode", 1).set ("osc1_excite", 13)
                 .set ("osc1_modal_size", size)
                 .set ("osc2_on", 0).set ("sub_on", 0).set ("subosc_on", 0)
                 .filter1 (LP, 20000.0f, 0.0f)
                 .amp (0.0f, 0.3f, 1.0f, 5.0f)
                 .macro (1, "PRESSURE", { { param ("osc1_bow_pressure"), 0.35f } })
                 .macro (2, "SPEED", { { param ("osc1_bow_speed"), 0.35f } })
                 .macro (3, "VIBRATO", { { param ("osc1_hammer_hard"), 0.3f } })
                 .macro (4, "BODY", { { param ("osc1_modal_size"), 0.08f } }));
    };

    bowed ("Modal Violin", 1.0f);
    bowed ("Modal Viola", 1.17f);
    bowed ("Modal Cello", 2.1f);
}
