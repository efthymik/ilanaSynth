#pragma once

#include "BodyTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// A bank of damped, inharmonic modes. Input is the whole oscillator mix.
// Coefficients change at the voice's 16-sample control rate; process() has no
// allocation and is skipped entirely when Classic or Off is selected.
class MaterialBody
{
public:
    void prepare (double rate) { sampleRate = std::max (8000.0, rate); lastShape = -1; reset(); }
    void reset()
    {
        y1.fill (0.0f);
        y2.fill (0.0f);
    }

    void configure (int shapeIndex, float material, float size, float decay,
                    double noteHz, int quality, float stereoOffset = 0.0f, bool legacyFixedLevel = false)
    {
        if (shapeIndex == lastShape && material == lastMaterial && size == lastSize
            && decay == lastDecay && noteHz == lastNote && quality == lastQuality
            && stereoOffset == lastStereo && legacyFixedLevel == lastLegacy)
            return;
        lastLegacy = legacyFixedLevel;
        lastShape = shapeIndex;
        lastMaterial = material;
        lastSize = size;
        lastDecay = decay;
        lastNote = noteHz;
        lastQuality = quality;
        lastStereo = stereoOffset;
        const auto& shape = BodyTuning::shapes[(size_t) std::clamp (shapeIndex, 0, 4)];
        const auto colour = std::clamp (material, 0.0f, 1.0f);
        const auto extent = std::clamp (size, 0.0f, 1.0f);
        const auto life = std::clamp (decay, 0.0f, 1.0f);
        activeModes = quality <= 0 ? 4 : quality == 1 ? 6 : 8;
        // (A fixed-pitch body, the guitar, ignores the note: SIZE tunes its modes in whole semitones, up to an octave either way; the note-following bodies snap the same way.)
        const auto pitch = shape.fixedPitch ? std::clamp ((double) shape.ratio[0] * std::exp2 (std::round ((0.5 - extent) * 24.0) / 12.0), 25.0, sampleRate * 0.4)
                                            : std::clamp (noteHz * std::exp2 (std::round ((0.5 - extent) * 24.0) / 12.0), 25.0, sampleRate * 0.4);
        const auto seconds = 0.08 + 7.92 * (double) life * (double) life;
        // A guitar's air and top-plate modes are broad: DECAY sets their Q, 6 to
        // 60 (a mode at 98 Hz then rings 0.13 to 1.35 s, the higher ones less).
        // Sharper fixed modes only ring when a harmonic happens to land on one,
        // and that note then booms 20-30 dB over its neighbours.
        const auto broadModes = shape.fixedPitch && ! legacyFixedLevel;
        const auto quality60 = 2.2 * (6.0 + 54.0 * (double) life * (double) life);
        const auto metal = std::clamp ((colour - 0.15f) / 0.55f, 0.0f, 1.0f);
        const auto glass = std::clamp ((colour - 0.8f) / 0.2f, 0.0f, 1.0f);
        const auto lifetimeScale = 0.9 + 0.6 * (double) metal - 0.75 * (double) glass;

        // The body is tuned to the note, so the drive sits on its first mode,
        // where a resonator's gain grows as 1 / (1 - r): +60 dB and more at
        // long DECAY. One factor, the first mode's exact peak gain, normalises
        // every mode, so a sustained drive comes out at unity at any DECAY and
        // pitch while the modes keep their relative balance (and their fit).
        const auto rootSeconds = (broadModes ? quality60 / pitch : seconds) * lifetimeScale;
        const auto rootRadius = std::exp (-6.907755278982137 / (sampleRate * std::max (0.015, rootSeconds)));
        const auto rootPhase = 6.283185307179586 * std::min (pitch, sampleRate * 0.45) / sampleRate;
        const auto peakNorm = (1.0 - rootRadius)
                              * std::sqrt (1.0 - 2.0 * rootRadius * std::cos (2.0 * rootPhase) + rootRadius * rootRadius);

        for (int i = 0; i < activeModes; ++i)
        {
            const auto m = (size_t) i;
            const auto ratio = (double) shape.ratio[(size_t) i] / (shape.fixedPitch ? (double) shape.ratio[0] : 1.0)
                               * (1.0 + (double) glass * (i == 0 ? 0.0 : 0.012 * i));
            const auto frequency = pitch * ratio * (1.0 + (double) stereoOffset * 0.001);
            if (frequency >= sampleRate * 0.46)
            {
                gain[m] = strikeLevel[m] = 0.0f;
                continue;
            }
            const auto t60 = (broadModes ? quality60 / frequency : seconds * (double) shape.lifetime[(size_t) i]) * lifetimeScale;
            const auto radius = std::exp (-6.907755278982137 / (sampleRate * std::max (0.015, t60)));
            const auto phase = 6.283185307179586 * frequency / sampleRate;
            a1[m] = (float) (2.0 * radius * std::cos (phase));
            a2[m] = (float) (-radius * radius);
            // 1 / sin^2 of the mode's angle: what level() needs to turn two
            // samples into the mode's amplitude.
            const auto s = std::sin (phase);
            amplitudeScale[m] = (float) (1.0 / std::max (1.0e-6, s * s));
            const auto brightness = 1.0f - 0.7f * (1.0f - colour) * (float) i / 7.0f;
            const auto weight = shape.gain[(size_t) i] * brightness;
            gain[m] = (float) (8.0 * peakNorm) * weight;            // +12 dB: plucks never reach full build-up
            // A strike rings mode i at 0.25 * weight * sin(root) / sin(mode): the
            // first mode at a fixed level, the higher ones falling off as before.
            strikeLevel[m] = 0.5f * weight * (float) std::sin (rootPhase);
        }

        // Modes past the quality's count are silent (all zero), so process()
        // runs every lane without a branch and adds exact zeros for them.
        for (auto m = (size_t) activeModes; m < (size_t) BodyTuning::modes; ++m)
            a1[m] = a2[m] = gain[m] = strikeLevel[m] = y1[m] = y2[m] = amplitudeScale[m] = 0.0f;

        // A fixed-pitch body (the guitar) does not sit on the note, so the
        // normalisation above, which assumes the drive's first harmonic lands on
        // the first mode, left it 20-60 dB under the tuned bodies: the string's
        // harmonics fell between modes a fraction of a hertz wide (ilana
        // 2026-11-12: "guitar type body ... makes no sound"). A real top plate
        // also radiates every frequency a little, so the guitar gets a direct
        // path under its modes, and the whole response is scaled so the note's
        // harmonics (1/k, as a string or saw) come out at the level a tuned body
        // gives them, within 12 dB: every note and DECAY is heard and the modes
        // still colour the notes near them. Old patches keep the old level
        // (body_legacy_level).
        direct = 0.0f;
        if (broadModes)
        {
            direct = (float) (directShare * 8.0 * (double) shape.gain[0]);
            const auto level = harmonicLevel (noteHz);
            const auto target = 4.0 * (double) shape.gain[0];
            const auto scale = level > 1.0e-9 ? std::clamp (target / level, 0.25, maxBoost) : 1.0;
            for (size_t m = 0; m < (size_t) activeModes; ++m)
                gain[m] = (float) ((double) gain[m] * scale);
            direct = (float) ((double) direct * scale);
        }
    }

    // A physical string's initial pluck also transfers a strike into the
    // body. Its ring is set by amplitude, not by DECAY; the continuously
    // rendered oscillator mix remains the driver.
    void strike (float strength)
    {
        const auto amount = std::clamp (strength, 0.0f, 1.0f);
        for (size_t m = 0; m < (size_t) BodyTuning::modes; ++m)
            y1[m] += strikeLevel[m] * amount;
    }

    // Every mode in one pass with no branches: the compiler runs the eight
    // lanes as SIMD. Each lane does the same arithmetic as a mode did one at
    // a time, and the sum is taken in mode order, so the output is the same
    // to the bit.
    float process (float input)
    {
        const auto drive = std::clamp (input, -2.0f, 2.0f);
        constexpr auto count = (size_t) BodyTuning::modes;
        alignas (32) float next[count];
        for (size_t m = 0; m < count; ++m)
        {
            const auto value = a1[m] * y1[m] + a2[m] * y2[m] + gain[m] * drive;
            next[m] = value < -8.0f ? -8.0f : (8.0f < value ? 8.0f : value);
        }
        auto sum = 0.0f;
        for (size_t m = 0; m < count; ++m)
        {
            y2[m] = y1[m];
            y1[m] = next[m];
            sum += next[m];
        }
        sum += direct * drive; // (zero but for the guitar: exact for every other body)
        return std::clamp (sum * 0.5f, -2.0f, 2.0f);
    }

    // The body's output level bound: half the sum of every mode's amplitude
    // (as process() halves the sum), each found from its last two samples
    // (for a ringing mode, A^2 sin^2 w = y1^2 + y2^2 - 2 cos w y1 y2). A
    // voice uses it to end its tail once the body has died away.
    float level() const
    {
        auto total = 0.0f;
        for (size_t m = 0; m < (size_t) BodyTuning::modes; ++m)
        {
            const auto energy = y1[m] * y1[m] + y2[m] * y2[m] - a1[m] * y1[m] * y2[m];
            total += std::sqrt (std::max (0.0f, energy) * amplitudeScale[m]);
        }
        return 0.5f * total;
    }

private:
    // The bank's steady response to the note's first harmonics, weighted 1/k:
    // the root-sum-square of |H(k f)| / k. (A tuned body gives 4 x its first
    // weight here, its root on its first mode's peak.)
    double harmonicLevel (double noteHz) const
    {
        constexpr int harmonics = 16;
        auto total = 0.0;
        for (int k = 1; k <= harmonics; ++k)
        {
            const auto hz = noteHz * k;
            if (hz >= sampleRate * 0.45)
                break;
            const auto z1 = std::polar (1.0, -6.283185307179586 * hz / sampleRate);
            const auto z2 = z1 * z1;
            std::complex<double> response;
            for (size_t m = 0; m < (size_t) activeModes; ++m)
                response += (double) gain[m] / (1.0 - (double) a1[m] * z1 - (double) a2[m] * z2);
            response += (double) direct;
            total += std::norm (0.5 * response) / (double) (k * k);
        }
        return std::sqrt (total);
    }

    alignas (32) std::array<float, BodyTuning::modes> y1 {}, y2 {}, a1 {}, a2 {}, gain {}, strikeLevel {}, amplitudeScale {};
    double sampleRate = 44100.0;
    int activeModes = 6;
    int lastShape = -1, lastQuality = -1;
    float lastMaterial = -1.0f, lastSize = -1.0f, lastDecay = -1.0f, lastStereo = 0.0f;
    double lastNote = -1.0;
    float direct = 0.0f;
    // The guitar's direct path, as a share of its first mode's peak (-12 dB),
    // and the most its response is lifted to reach a tuned body's level.
    static constexpr double directShare = 0.25, maxBoost = 4.0;
    bool lastLegacy = false;
};
