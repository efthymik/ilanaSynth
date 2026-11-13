#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>

// The Modal Piano's data (content/piano/modal-piano.bin, from
// tools/build_modal_piano.py): for each recorded note and velocity, the
// partials' frequencies (as ratios to the note's nominal pitch), each
// partial's level at a fixed set of times after the strike, and the start of
// the recording itself (the hammer's attack). The Modal Guitar's (version 5,
// tools/build_modal_guitar.py) has no recording: its pluck is modelled from
// early levels, start phases, a rise and a noise burst.
struct ModalPianoTable
{
    struct Entry
    {
        int note = 60, velocity = 64, partials = 0;
        std::vector<float> ratios;  // [partials]
        std::vector<float> levels;  // [partials * times], dB
        std::vector<float> attack;  // at the table's rate (the left channel)
        std::vector<float> attackRight; // (version 4; else the left's)
        // The board's resonances (and the other strings') this note rings:
        // fixed in Hz, the same for every velocity of a note.
        std::vector<float> modes;       // Hz
        std::vector<float> modeLevels;  // [modes * times], dB
        // The noise between them, per band (noiseBands): the median FFT bin
        // (16384 points, Hann) away from every partial and mode, in dB.
        std::vector<float> noiseLevels; // [bands * times], dB
        // The stereo picture (version 4; else centred): each partial's and
        // mode's balance (dB, left over right) and phase (right against
        // left, radians), the same for every velocity of a note.
        std::vector<float> balance, phase;         // [partials]
        std::vector<float> modeBalance, modePhase; // [modes]
        float attackDb = -200.0f; // the attack's level over its first 80 ms (both channels), dB
        // Each partial in the attack (left channel) where it hands over to
        // the partials, 20-80 ms: its sine and cosine parts (sin (w t + p)
        // = s sin w t + c cos w t, t from the attack's start), so the
        // partials start in step with it; out of step they cancel it
        // mid-crossfade, a hole heard as a blip before the note
        // (2026-11-13). Empty: none (no attack).
        std::vector<float> attackSin, attackCos; // [partials]
        // The modelled pluck (version 5; the table has no attack samples):
        // when the pluck starts after the note's start and how long it
        // rises (one raised cosine swells over both), each partial's and
        // mode's phase at the note's start (radians; noPhase: none
        // measured, a random one), and the pluck's noise per noise band,
        // at the note's start (dB, the noiseLevels' measure) and its fall
        // (dB/s).
        float pluckMs = 0.0f, riseMs = 0.0f;
        std::vector<float> startPhase, modeStartPhase; // [partials], [modes]
        std::vector<float> burstDb, burstFall;         // [noiseBandCount]
        // What each mode is, for SIZE (ModeKind, sorted at parse time):
        // the guitar's air (A0) and top (T(1,1)2) modes, the board's or
        // plate's other modes, and modes that are the strings' (the
        // grand's bass strings ring longitudinal modes at 0.4-1.2 kHz),
        // which don't move with the body.
        std::vector<unsigned char> modeKind; // [modes]
    };

    enum ModeKind : unsigned char { boardMode = 0, airMode = 1, topMode = 2, stringMode = 3 };

    // The guitar's attacks are separate takes per velocity, not one
    // instrument's strikes: mixed sample by sample their plucks partly
    // cancel (2026-11-11), so it plays the nearer layer's at the blended
    // level. (The grand keeps its mix: its layers line up. A version 5
    // table, the guitar's since 2026-11-12, has no attack: modelledPluck.)
    bool nearestAttack = false;
    // Version 5: the pluck is modelled (Entry::pluckMs ...), not played back.
    bool modelledPluck = false;
    // The guitar's box (SIZE moves its air and top modes as a coupled
    // pair), not the grand's soundboard; and the box's air and top modes
    // as the table has them (Hz; the most common ones over the notes).
    bool guitarBody = false;
    float airHz = 103.0f, topHz = 240.0f;

    static constexpr float noPhase = 100.0f; // (Entry::startPhase: none)
    static constexpr int noiseBandCount = 6;
    static constexpr float noiseBands[noiseBandCount][2] { { 0, 500 }, { 500, 1000 }, { 1000, 2000 },
                                                          { 2000, 4000 }, { 4000, 8000 }, { 8000, 16000 } };
    static constexpr int noiseFftSize = 16384;

    std::vector<float> times;       // seconds after the strike
    int lateTime = 0;               // the first time from 0.05 s (the guitar's are after its early ones)
    float rate = 44100.0f;
    std::vector<int> notes, velocities; // sorted, each listed once
    std::vector<Entry> entries;     // velocity-major: entries[v * notes.size() + n]

    // The instrument's average tone (2026-11-11, for SIZE): every partial's
    // level 0.08 s in, against its own note's total, averaged over the
    // whole table in sixth-octave bins from curveLowHz and smoothed over an
    // octave. SIZE moves this curve down (a bigger body) or up (a smaller one).
    static constexpr int curveBins = 60, curveBinsPerOctave = 6;
    static constexpr float curveLowHz = 40.0f;
    std::vector<float> bodyCurve; // [curveBins], dB

    // The curve at a frequency (flat past its ends).
    float curveAt (float hz) const
    {
        if (bodyCurve.empty() || ! (hz > 0.0f))
            return 0.0f;
        const auto x = juce::jlimit (0.0f, (float) (curveBins - 1), (float) curveBinsPerOctave * std::log2 (hz / curveLowHz));
        const auto i = juce::jmin ((int) x, curveBins - 2);
        const auto a = x - (float) i;
        return bodyCurve[(size_t) i] + a * (bodyCurve[(size_t) i + 1] - bodyCurve[(size_t) i]);
    }

    const Entry& at (int noteIndex, int velocityIndex) const
    {
        return entries[(size_t) velocityIndex * notes.size() + (size_t) noteIndex];
    }

    bool valid() const { return ! entries.empty() && entries.size() == notes.size() * velocities.size(); }

    // Parsed once, on first use (src/dsp/ModalPianoTable.cpp): the grand,
    // and the Modal Guitar's nylon-string guitar (content/guitar/modal-guitar.bin,
    // tools/build_modal_guitar.py, the same format).
    static const ModalPianoTable& get();
    static const ModalPianoTable& guitar();
};
