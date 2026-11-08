#pragma once

// The Modal Bowed exciter: a bowed string played as its modes, inside a
// violin-family body that SIZE morphs from violin to viola to cello and on.
//
// The string is a bank of up to 64 modes (sin (n pi x / L) shapes, each a
// damped two-pole oscillator updated per sample). Each sample the bow's
// friction is worked out at the bow point from the modes' velocity there,
// with the STK bow table the Bow exciter uses (slope 5 - 4 x pressure: the
// bow holds the string until the speed difference breaks it away), and the
// force goes back into every mode by its shape at the bow. The bridge force
// (the sum of the modes' slopes at the bridge) drives the body: a parallel
// bank of fixed-Hz resonators fitted to recordings (ModalBowedBodies.h,
// tools/fit_modal_bowed_body.py), one set of gains each for a violin, a
// viola and a cello on the same grid. SIZE divides every resonator's
// frequency and bandwidth (a bigger box rings lower) and blends the fitted
// gains between the three instruments (1, 1.17 and 2.1 times a violin);
// past either end the nearest instrument's body is scaled on.
//
// The knobs (each at its default plays the fitted violin):
//   DECAY, DAMP  the string's losses: all modes, and the upper ones more
//   STIFF        stretches the partials (a stiffer string)
//   POSITION     where the bow sits (0: 0.11 of the string from the bridge)
//   GRIP         bow pressure (aftertouch adds to it): light gives the airy
//                "surface" sound, heavy a gritty, slightly flat tone
//   SPEED        bow speed: how loud and present the note is
//   SIZE, SCALE  the body's size and the string's length (longer: lower
//                losses, a softer stretch)
//   VIBRATO      (the HAMMER knob) the player's vibrato: at its default the
//                5.2 Hz, 9 cent vibrato fitted to Spitfire's long notes,
//                starting a quarter second into the note
//   PIZZ         (the SLAP switch) plucks the same modes instead of bowing
//   DAMPER       how fast the string stops once the note is let go
// Velocity is the bite: a harder start, a faster bow at the attack.
//
// Fitted over time to ilana's Spitfire String Quartet renders (2026-11-08):
// the bow lands almost still and swells to full speed over about half a
// second (the rise of a long note), the rosin makes the bow's speed
// jitter (the noise between the partials, about 30 dB under them), a little
// hiss off the hair reaches past the body's top, and the bow lifts at once
// on release so the string and the body ring out.
// Eco keeps half the modes and the body's lower half.

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "ModalBowedBodies.h"

class ModalBowed
{
public:
    static constexpr int maxModes = 64;
    static constexpr int maxBody = ModalBowedBodies::count;
    static constexpr int blockSize = 32;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        bodyDirty = modesDirty = true;
        reset();
    }

    void reset()
    {
        std::fill (std::begin (q), std::end (q), 0.0f);
        std::fill (std::begin (qp), std::end (qp), 0.0f);
        std::fill (std::begin (bx1), std::end (bx1), 0.0f);
        std::fill (std::begin (bx2), std::end (bx2), 0.0f);
        std::fill (std::begin (by1), std::end (by1), 0.0f);
        std::fill (std::begin (by2), std::end (by2), 0.0f);
        envelope = 0.0f;
        silent = true;
        untilBlock = 0;
    }

    void setNote (double hz)
    {
        hz = juce::jlimit (20.0, sampleRate * 0.45, hz);
        if (hz != frequency)
        {
            frequency = hz;
            modesDirty = true;
        }
    }

    void setParams (float decay, float damping, float stiffness, float position, float pressure, float speed,
                    float damper, bool pizz, bool eco, float vibrato = 0.5f)
    {
        // At the Physical card's defaults (DECAY 0.75, DAMP 0.35, STIFF 0)
        // the losses and stretch of the sketch fitted to the Iowa violin.
        const auto lossAll = 0.8f * std::exp2 ((0.75f - juce::jlimit (0.0f, 1.0f, decay)) * 5.0f);
        const auto lossHigh = 3.0e-5f * std::exp2 ((juce::jlimit (0.0f, 1.0f, damping) - 0.35f) * 5.0f);
        const auto b = 1.0e-5f + 2.0e-4f * juce::jlimit (0.0f, 1.0f, stiffness) * juce::jlimit (0.0f, 1.0f, stiffness);
        const auto beta = position > 0.0f ? juce::jlimit (0.04f, 0.5f, position) : 0.11f;
        if (lossAll != loss0 || lossHigh != loss1 || b != inharmonic || beta != bowAt || eco != ecoOn)
        {
            loss0 = lossAll;
            loss1 = lossHigh;
            inharmonic = b;
            bowAt = beta;
            ecoOn = eco;
            modesDirty = bodyDirty = true;
        }
        grip = juce::jlimit (0.0f, 1.0f, pressure);
        bowSpeed = 0.05f + 0.3f * juce::jlimit (0.0f, 1.0f, speed);
        damperAmount = juce::jlimit (0.0f, 1.0f, damper);
        plucked = pizz;
        // 0.5 plays the fitted 9 cents; the knob's top about 30.
        const auto v = juce::jlimit (0.0f, 1.0f, vibrato);
        vibratoCents = 30.0f * std::pow (v, 1.737f);
    }

    // SIZE and SCALE as factors (1: the violin as fitted).
    void setShape (float size, float scale)
    {
        size = juce::jlimit (0.1f, 10.0f, size);
        scale = juce::jlimit (0.1f, 10.0f, scale);
        if (size != bodySize)
        {
            bodySize = size;
            bodyDirty = true;
        }
        if (scale != stringScale)
        {
            stringScale = scale;
            modesDirty = true;
        }
    }

    void trigger (float velocity, float randomUnit)
    {
        velocityLevel = juce::jlimit (0.0f, 1.0f, velocity);
        seed = (std::uint32_t) (randomUnit * 4294967040.0f) | 1u;
        vibratoPhase = randomUnit * juce::MathConstants<float>::twoPi;
        swell = 0.0f;
        hissState = hissLast = 0.0f;
        // The mode count is set by the note's pitch at the start (a bend
        // keeps it, so no mode comes or goes mid-note).
        const auto top = juce::jmin (9000.0, sampleRate * 0.45);
        modes = juce::jlimit (1, ecoOn ? maxModes / 2 : maxModes, (int) (top / frequency));
        modesDirty = bodyDirty = true;
        updateModes();
        updateBody();
        std::fill (std::begin (q), std::end (q), 0.0f);
        std::fill (std::begin (qp), std::end (qp), 0.0f);
        envelope = 0.0f;
        elapsed = 0;
        releasedAt = -1;
        silent = false;
        untilBlock = 0;
        level = 1.0f;
        peakBridge = peakBody = 1.0e-20f;
        if (plucked)
        {
            // A finger pluck at the bow's place: the triangle's modal
            // amplitudes, louder with velocity.
            const auto x = juce::jmax (0.08f, bowAt * 1.6f);
            for (int n = 0; n < modes; ++n)
            {
                const auto k = (float) (n + 1);
                const auto amplitude = (0.4f + 0.6f * velocityLevel) * 2.0f * std::sin (k * juce::MathConstants<float>::pi * x)
                                       / (k * k * juce::MathConstants<float>::pi * juce::MathConstants<float>::pi * x * (1.0f - x))
                                       * pluckScale;
                q[n] = qp[n] = amplitude;
            }
        }
    }

    float process (float expression, bool noteHeld)
    {
        if (silent)
            return 0.0f;
        if (! noteHeld && releasedAt < 0)
            releasedAt = elapsed;
        if (untilBlock == 0)
            nextBlock (noteHeld);
        --untilBlock;
        ++elapsed;

        // The modes' next state without the bow.
        float next[maxModes];
        auto bowVelocity = 0.0f;
        for (int n = 0; n < modes; ++n)
        {
            next[n] = turn[n] * q[n] - fall[n] * qp[n];
            bowVelocity += shape[n] * (next[n] - qp[n]);
        }
        bowVelocity *= halfRate;

        auto force = 0.0f;
        if (! plucked)
        {
            // The envelope: the bow sets on over attackSeconds, lifts on release.
            envelope += ((noteHeld ? 1.0f : 0.0f) - envelope) * (noteHeld ? attackCoefficient : releaseCoefficient);
            swell += ((noteHeld ? 1.0f : 0.0f) - swell) * swellCoefficient;
            if (envelope > 1.0e-5f)
            {
                seed = seed * 1664525u + 1013904223u;
                const auto rosin = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f);
                seed = seed * 1664525u + 1013904223u;
                hairNoise = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f) * bowSpeed * envelope * (1.0f - swellDepth + swellDepth * swell);
                const auto bite = 1.0f + 0.6f * velocityLevel * biteEnvelope;
                const auto bow = bowSpeed * envelope * (1.0f - swellDepth + swellDepth * swell) * bite * (1.0f + rosin * rosinJitter);
                const auto pressure = juce::jlimit (0.0f, 1.0f, grip + expression * 0.75f);
                const auto difference = bow - bowVelocity;
                const auto slope = 5.0f - 4.0f * pressure;
                const auto table = juce::jmin (1.0f, std::pow (std::abs (difference * slope) + 0.75f, -4.0f));
                force = difference * table * forceScale * envelope;
            }
        }
        else
            hairNoise = 0.0f;
        if (envelope <= 1.0e-5f)
            hairNoise = 0.0f;
        biteEnvelope *= biteFall;

        auto bridge = 0.0f;
        for (int n = 0; n < modes; ++n)
        {
            const auto value = next[n] + push[n] * force;
            qp[n] = q[n];
            q[n] = value;
            bridge += out[n] * value;
        }
        bridge *= level;
        peakBridge = juce::jmax (peakBridge, std::abs (bridge));

        // The body: every resonator driven by the bridge, summed.
        auto sound = 0.0f;
        for (int j = 0; j < bodyCount; ++j)
        {
            const auto y = bodyB0[j] * (bridge - bx2[j]) - bodyA1[j] * by1[j] - bodyA2[j] * by2[j];
            bx2[j] = bx1[j];
            bx1[j] = bridge;
            by2[j] = by1[j];
            by1[j] = y;
            sound += bodyGain[j] * y;
        }
        peakBody = juce::jmax (peakBody, std::abs (sound));
        sound *= outputGain;
        // The hair's hiss, high-passed, straight out (past the body's top).
        const auto hiss = hissPole * (hissState + hairNoise - hissLast);
        hissLast = hairNoise;
        hissState = hiss;
        sound += hissLevel * hiss;
        return std::isfinite (sound) ? juce::jlimit (-8.0f, 8.0f, sound) : 0.0f;
    }

private:
    void nextBlock (bool noteHeld)
    {
        untilBlock = blockSize;
        if (modesDirty)
            updateModes();
        if (bodyDirty)
            updateBody();
        updateVibrato();
        // Released: DAMPER takes the string's level down (0 lets it ring).
        if (releasedAt >= 0 && damperAmount > 0.0f)
        {
            const auto seconds = (float) (blockSize / sampleRate);
            level *= std::exp2 (-damperAmount * 60.0f * seconds);
        }
        // Silent once the bow is off and the string and body have died away.
        if (! noteHeld && (plucked || envelope < 1.0e-4f))
        {
            auto energy = 0.0f;
            for (int n = 0; n < modes; ++n)
                energy = juce::jmax (energy, std::abs (q[n] * out[n]));
            auto body = 0.0f;
            for (int j = 0; j < bodyCount; ++j)
                body += std::abs (by1[j] * bodyGain[j]);
            // (-90 dB under the note's loudest, or the damper's done.)
            if ((energy * level < 3.0e-5f * peakBridge && body < 3.0e-5f * peakBody) || level < 1.0e-5f)
                silent = true;
        }
    }

    // The vibrato bends every mode by the same ratio once a block (the
    // losses stay at the note's own pitch).
    void updateVibrato()
    {
        const auto seconds = (double) elapsed / sampleRate;
        vibratoPhase += (float) (juce::MathConstants<double>::twoPi * vibratoRate * blockSize / sampleRate);
        if (vibratoPhase > juce::MathConstants<float>::twoPi)
            vibratoPhase -= juce::MathConstants<float>::twoPi;
        const auto onset = juce::jlimit (0.0, 1.0, (seconds - vibratoDelay) / vibratoRise);
        const auto cents = plucked ? 0.0 : (double) vibratoCents * onset * std::sin ((double) vibratoPhase);
        if (cents == bentCents)
            return;
        bentCents = cents;
        const auto ratio = std::exp2 (cents / 1200.0);
        const auto k = juce::MathConstants<double>::twoPi / sampleRate;
        for (int n = 0; n < modes; ++n)
            turn[n] = (float) (2.0 * (double) decayOf[n] * std::cos (juce::jmin ((double) modeHz[n] * ratio, sampleRate * 0.49) * k));
    }

    void updateModes()
    {
        modesDirty = false;
        const auto k = 1.0 / sampleRate;
        halfRate = (float) (0.5 * sampleRate);
        // SCALE: a longer string loses less per second and is less stiff.
        const auto lossScale = 1.0 / (double) stringScale;
        const auto b = (double) inharmonic / ((double) stringScale * stringScale * stringScale);
        auto shapeSum = 0.0;
        for (int n = 0; n < modes; ++n)
        {
            const auto index = (double) (n + 1);
            const auto hz = juce::jmin (index * frequency * std::sqrt (1.0 + b * index * index), sampleRate * 0.49);
            const auto sigma = ((double) loss0 + (double) loss1 * hz * hz) * lossScale;
            const auto decay = std::exp (-sigma * k);
            turn[n] = (float) (2.0 * decay * std::cos (juce::MathConstants<double>::twoPi * hz * k));
            decayOf[n] = (float) decay;
            modeHz[n] = (float) hz;
            fall[n] = (float) (decay * decay);
            shape[n] = (float) std::sin (index * juce::MathConstants<double>::pi * (double) bowAt);
            shapeSum += (double) shape[n] * shape[n];
            // The bridge force's share: the mode's slope at the bridge (n pi / L).
            out[n] = (float) (index / (double) modes);
        }
        // With the string's mass normalised away (it cancels between the
        // bow's admittance and the force's push), the force on mode n is
        // 2 k phi_n (v_bow - v_string) table / sum phi^2.
        const auto admittance = juce::jmax (1.0e-6, shapeSum);
        for (int n = 0; n < modes; ++n)
            push[n] = (float) (2.0 * k * (double) shape[n] / admittance);
        forceScale = 1.0f;
        bentCents = 0.0;
        attackCoefficient = (float) (1.0 - std::exp (-1.0 / (attackSeconds () * sampleRate)));
        releaseCoefficient = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));
        swellCoefficient = (float) (1.0 - std::exp (-1.0 / (0.34 * sampleRate)));
        hissPole = (float) std::exp (-juce::MathConstants<double>::twoPi * 8000.0 / sampleRate);
        biteFall = (float) std::exp (-1.0 / (0.08 * sampleRate));
        if (elapsed == 0)
            biteEnvelope = 1.0f;
        // The plucked string's amplitudes, in the bowed string's units (a
        // bow at speed v moves the string about v / (2 pi f); the factor
        // sets a pizzicato level with an arco note, measured).
        pluckScale = (float) (5.0 * (double) bowSpeed / (juce::MathConstants<double>::twoPi * juce::jmax (20.0, frequency)));
    }

    // The bow's contact: a low string takes longer to speak (as on the Bow
    // exciter); the swell after it shapes the note's rise.
    double attackSeconds() const { return 0.0025 * juce::jlimit (1.0, 2.5, 220.0 / frequency); }

    void updateBody()
    {
        bodyDirty = false;
        using B = ModalBowedBodies;
        // Which instruments' gains, and how far between them (log size).
        const auto size = (double) bodySize;
        const float* lower = B::violin;
        const float* upper = B::violin;
        auto blend = 0.0;
        if (size > B::cello)
            lower = upper = B::cellos;
        else if (size > B::viola)
        {
            lower = B::violas;
            upper = B::cellos;
            blend = std::log (size / B::viola) / std::log (B::cello / B::viola);
        }
        else if (size > 1.0)
        {
            upper = B::violas;
            blend = std::log (size) / std::log (B::viola);
        }
        const auto count = ecoOn ? B::count * 2 / 3 : B::count;
        bodyCount = 0;
        for (int j = 0; j < count; ++j)
        {
            const auto hz = (double) B::hz[j] / size;
            const auto width = (double) B::bandwidth[j] / size;
            if (hz >= sampleRate * 0.45)
                continue;
            const auto r = std::exp (-juce::MathConstants<double>::pi * width / sampleRate);
            const auto i = bodyCount++;
            bodyB0[i] = (float) ((1.0 - r * r) * 0.5);
            bodyA1[i] = (float) (-2.0 * r * std::cos (juce::MathConstants<double>::twoPi * hz / sampleRate));
            bodyA2[i] = (float) (r * r);
            const auto db = (double) lower[j] + blend * ((double) upper[j] - (double) lower[j]);
            bodyGain[i] = (float) std::pow (10.0, db / 20.0);
        }
    }

    double sampleRate = 44100.0, frequency = 220.0;
    int modes = 1, bodyCount = 0, untilBlock = 0;
    long long elapsed = 0, releasedAt = -1;
    bool silent = true, modesDirty = true, bodyDirty = true, plucked = false, ecoOn = false;
    float loss0 = 0.8f, loss1 = 3.0e-5f, inharmonic = 1.0e-5f, bowAt = 0.11f;
    float grip = 0.5f, bowSpeed = 0.2f, damperAmount = 0.0f, velocityLevel = 0.5f;
    float bodySize = 1.0f, stringScale = 1.0f;
    float envelope = 0.0f, attackCoefficient = 0.001f, releaseCoefficient = 0.001f;
    float biteEnvelope = 1.0f, biteFall = 0.999f, level = 1.0f;
    float halfRate = 22050.0f, forceScale = 1.0f, pluckScale = 1.0f, peakBridge = 0.0f, peakBody = 0.0f;
    float swell = 0.0f, swellCoefficient = 0.0001f, hairNoise = 0.0f, hissState = 0.0f, hissLast = 0.0f, hissPole = 0.35f;
    float vibratoCents = 9.0f, vibratoPhase = 0.0f;
    double bentCents = 0.0;
    std::uint32_t seed = 1;
    // Fitted to the Spitfire long notes (2026-11-08, see the top).
    static constexpr float swellDepth = 0.97f, rosinJitter = 0.0905f, hissLevel = 0.0052f;
    static constexpr double vibratoRate = 5.2, vibratoDelay = 0.25, vibratoRise = 0.4;
    // The output's level: the string's units are tiny (metres of string,
    // narrow resonators); this brings a violin D4 near a plucked string's.
    static constexpr float outputGain = 30000.0f;

    alignas (32) float q[maxModes] {}, qp[maxModes] {}, turn[maxModes] {}, fall[maxModes] {};
    alignas (32) float shape[maxModes] {}, push[maxModes] {}, out[maxModes] {}, decayOf[maxModes] {}, modeHz[maxModes] {};
    alignas (32) float bodyB0[maxBody] {}, bodyA1[maxBody] {}, bodyA2[maxBody] {}, bodyGain[maxBody] {};
    alignas (32) float bx1[maxBody] {}, bx2[maxBody] {}, by1[maxBody] {}, by2[maxBody] {};
};
