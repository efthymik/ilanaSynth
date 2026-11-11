#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>

// The Modal Piano's data (content/piano/modal-piano.bin, from
// tools/build_modal_piano.py): for each recorded note and velocity, the
// partials' frequencies (as ratios to the note's nominal pitch), each
// partial's level at a fixed set of times after the strike, and the start of
// the recording itself (the hammer's attack).
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
    };

    // The guitar's attacks are separate takes per velocity, not one
    // instrument's strikes: mixed sample by sample their plucks partly
    // cancel (2026-11-11), so it plays the nearer layer's at the blended
    // level. (The grand keeps its mix: its layers line up.)
    bool nearestAttack = false;

    static constexpr int noiseBandCount = 6;
    static constexpr float noiseBands[noiseBandCount][2] { { 0, 500 }, { 500, 1000 }, { 1000, 2000 },
                                                          { 2000, 4000 }, { 4000, 8000 }, { 8000, 16000 } };
    static constexpr int noiseFftSize = 16384;

    std::vector<float> times;       // seconds after the strike
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
