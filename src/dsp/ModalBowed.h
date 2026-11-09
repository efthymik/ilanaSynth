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
//   DAMPER       how fast the string stops once the note is let go (at 0 a
//                bowed note fades as Spitfire's do, a pluck rings on)
// Velocity is the bite: a harder start, a faster bow at the attack.
//
// Fitted over time to ilana's Spitfire String Quartet renders (2026-11-08),
// scored on spectrograms partial by partial (tools/modal_bowed_refit/):
// the bow lands almost still and swells to full speed over about half a
// second (the rise of a long note); the rosin makes the bow's speed jitter
// and the hair's scrape reaches the body (the noise between the partials);
// a little hiss off the hair reaches past the body's top; on release the
// bow lifts off (its pressure eases, its speed keeps on, so it doesn't
// brake the string), the upper partials die away over half a second and
// the note fades at about 37 dB a second. The vibrato is
// never quite even (its rate and depth wander), and it sweeps each partial
// across the body's fine ripple, so each one's level rises and falls on its
// own (less so low down, where the body's resonances thin out); each
// partial's level and the bow's speed also wander slowly, as a player's bow
// does; the hair's grip on each upper partial wavers quickly (its loss: the
// partial's own ring flutters, a grainy line, while the low partials hold
// solid); the box radiates little below its lowest resonance (no boom under
// a low note); and the pitch drifts
// a few cents. The upper register is brighter (the higher strings
// lose less up high), and a gentle roll-off sets the top of the spectrum.
// Round 6 (2026-11-09): the upper partials start about 15 dB darker and open
// up over the first tenth of a second (a bow biting in); the box's own top
// is rolled off with SIZE (no rosin fizz on a cello); and a small room (four
// delay lines per voice, darker as it decays, longer for a bigger box) holds
// the note's tail after the bow lifts, as Spitfire's hall does. Hear the
// room with an amp release of 2.5 s or more. The pizzicato stays dry.
// Eco keeps half the modes and the body's lower half.

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

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
        updateRoom();
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
        hx1 = hx2 = hy1 = hy2 = 0.0f;
        lx1 = lx2 = ly1 = ly2 = 0.0f;
        tilt = hissState = hissLast = 0.0f;
        std::fill (roomBuffer.begin(), roomBuffer.end(), 0.0f);
        std::fill (std::begin (roomLow), std::end (roomLow), 0.0f);
        std::fill (std::begin (roomAt), std::end (roomAt), 0);
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
        // (The upper modes' loss: bowed, fitted to Spitfire's long notes, the
        // register's brightness on top in updateModes; plucked, as fitted to
        // the Iowa violin.)
        const auto dampScale = std::exp2 ((juce::jlimit (0.0f, 1.0f, damping) - 0.35f) * 5.0f);
        const auto lossHigh = (pizz ? 3.0e-5f : 3.947e-06f) * dampScale;
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
        swell = liftSpeed = 0.0f;
        hissState = hissLast = tilt = 0.0f;
        releasedAt = -1;   // (before updateModes: the release's losses are off)
        // The mode count is set by the note's pitch at the start (a bend
        // keeps it, so no mode comes or goes mid-note).
        const auto top = juce::jmin (plucked ? 9000.0 : (double) topHz, sampleRate * 0.45);
        modes = juce::jlimit (1, ecoOn ? maxModes / 2 : maxModes, (int) (top / frequency));
        driftSeed = seed ^ 0x9E3779B9u;
        flutterSeed = seed ^ 0x85EBCA6Bu;
        std::fill (std::begin (flutter), std::end (flutter), 0.0f);
        std::fill (std::begin (flutterIn), std::end (flutterIn), 0.0f);
        pitchDrift = pitchDriftIn = 0.0f;
        std::fill (std::begin (wander), std::end (wander), 0.0f);
        std::fill (std::begin (wanderIn), std::end (wanderIn), 0.0f);
        bowDrift = rateDrift = depthDrift = bowDriftIn = rateDriftIn = depthDriftIn = 0.0f;
        bowWander = 1.0f;
        modesDirty = bodyDirty = true;
        updateModes();
        updateBody();
        for (int n = 0; n < modes; ++n)
        {
            // (bowed: the onset's darkening from the first sample)
            out[n] = outBase[n] * (plucked ? 1.0f : std::exp (-decibel * onsetDark[n]));
            outStep[n] = 0.0f;
        }
        std::fill (std::begin (q), std::end (q), 0.0f);
        std::fill (std::begin (qp), std::end (qp), 0.0f);
        envelope = 0.0f;
        elapsed = 0;
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
            // The envelope: the bow sets on over attackSeconds, eases off on release.
            envelope += ((noteHeld ? 1.0f : 0.0f) - envelope) * (noteHeld ? attackCoefficient : releaseCoefficient);
            swell += ((noteHeld ? 1.0f : 0.0f) - swell) * swellCoefficient;
            if (envelope > 1.0e-5f)
            {
                seed = seed * 1664525u + 1013904223u;
                const auto rosin = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f);
                seed = seed * 1664525u + 1013904223u;
                // On release the bow lifts off: its pressure eases (the
                // envelope) but it keeps the speed it had, so it doesn't
                // brake the string to a stop.
                const auto stroke = envelope * (1.0f - swellDepth + swellDepth * swell);
                if (noteHeld)
                    liftSpeed = stroke;
                const auto speed = bowSpeed * bowWander * liftSpeed;
                hairNoise = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f) * bowSpeed * bowWander * stroke;
                const auto bite = 1.0f + 0.6f * velocityLevel * biteEnvelope;
                const auto bow = speed * bite * (1.0f + rosin * rosinJitter);
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
            out[n] += outStep[n];
            bridge += out[n] * value;
        }
        // The hair's scrape reaches the bridge too, coloured by the body.
        bridge = (bridge + bodyNoise * hairNoise) * level;
        peakBridge = juce::jmax (peakBridge, std::abs (bridge));

        // The body: every resonator driven by the bridge, summed (and a
        // little of the bridge straight through: the box's broad radiation
        // between its resonances, which falls away below its lowest
        // resonance: the bowed string's push there is a boom, not wood).
        const auto radiated = hpB0 * bridge + hpB1 * hx1 + hpB2 * hx2 - hpA1 * hy1 - hpA2 * hy2;
        hx2 = hx1;
        hx1 = bridge;
        hy2 = hy1;
        hy1 = radiated;
        // (and falls away up high: a big box's broad radiation is darker)
        const auto darker = lpB0 * radiated + lpB1 * lx1 + lpB2 * lx2 - lpA1 * ly1 - lpA2 * ly2;
        lx2 = lx1;
        lx1 = radiated;
        ly2 = ly1;
        ly1 = darker;
        auto sound = bodyDirect * (plucked ? bridge : darker);
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
        // The room: four delay lines mixed by a Householder matrix, each
        // through a one-pole low-pass (the low end rings longer than the
        // top). It hears the whole note and rings on once it is let go.
        // (Bowed only: the pizzicato was fitted dry.)
        if (! plucked)
        {
            float* line = roomBuffer.data();
            auto sum = 0.0f;
            for (int r = 0; r < roomLines; ++r)
            {
                roomLow[r] = roomFeed[r] * line[roomStart[r] + roomAt[r]] + roomPole[r] * roomLow[r];
                sum += roomLow[r];
            }
            sum *= 2.0f / (float) roomLines;
            for (int r = 0; r < roomLines; ++r)
            {
                line[roomStart[r] + roomAt[r]] = roomLow[r] - sum + ((r & 1) != 0 ? -sound : sound);
                if (++roomAt[r] == roomLength[r])
                    roomAt[r] = 0;
            }
            sound += roomReturn * (roomLow[0] - roomLow[1] + roomLow[2] - roomLow[3]);
        }
        // A gentle roll-off (tiltHz) sets the top of the spectrum.
        tilt = sound + tiltPole * (tilt - sound);
        sound = tilt;
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
        // Let go (bowed): the upper partials' losses rise to releaseRing
        // times over releaseRingSeconds (the lifting hair stops feeding them).
        if (releasedAt >= 0 && ! plucked)
        {
            const auto gone = juce::jmin (1.0, (double) (elapsed - releasedAt) / (juce::jmax (1.0e-3, releaseRingSeconds) * sampleRate));
            const auto upper = 1.0 + ((double) releaseRing - 1.0) * gone;
            if (upper != ringUpper || gone < 1.0)
            {
                ringUpper = upper;
                const auto low = (double) loss0 / (double) stringScale;
                for (int n = 0; n < modes; ++n)
                {
                    lossOf[n] = (float) juce::jmin (low + (double) lossUpper[n] * upper + gone * (double) releaseLoss[n], sampleRate); // (fastExp's range)
                    const auto decay = std::exp (-(double) lossOf[n] / sampleRate);
                    fall[n] = (float) (decay * decay);
                }
            }
        }
        updateVibrato();
        // Released: the bowed note fades as Spitfire's do, and DAMPER takes
        // the string's level down faster (a pluck at 0 rings on).
        if (releasedAt >= 0 && (damperAmount > 0.0f || ! plucked))
        {
            const auto seconds = (float) (blockSize / sampleRate);
            level *= std::exp2 (-(damperAmount * 60.0f + (plucked ? 0.0f : releaseFadeDb / 6.0206f)) * seconds);
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
            auto room = 0.0f;
            for (int r = 0; r < roomLines; ++r)
                room += std::abs (roomLow[r]);
            if (((energy * level < 3.0e-5f * peakBridge && body < 3.0e-5f * peakBody) || level < 1.0e-5f)
                && room * roomReturn < 3.0e-5f * peakBody * outputGain)
                silent = true;
        }
    }

    // Once a block: the slow wander (each partial's level, the bow's speed,
    // the vibrato's rate and depth: low-passed noise), then the
    // vibrato bends every mode by the same ratio (the losses stay at the
    // note's own pitch) and sweeps each partial across the body's fine
    // ripple, so its level rises and falls with the vibrato as through a
    // real body's many narrow resonances.
    void updateVibrato()
    {
        if (plucked)
            return;
        const auto draw = [this]
        {
            driftSeed = driftSeed * 1664525u + 1013904223u;
            return (float) (driftSeed >> 8) * (2.0f / 16777216.0f) - 1.0f;
        };
        for (int n = 0; n < modes; ++n)
        {
            wanderIn[n] += wanderPole * (wanderGain * draw() - wanderIn[n]);
            wander[n] += wanderPole * (wanderIn[n] - wander[n]);
        }
        bowDriftIn += wanderPole * (bowDriftGain * draw() - bowDriftIn);
        bowDrift += wanderPole * (bowDriftIn - bowDrift);
        rateDriftIn += vibratoPole * (rateDriftGain * draw() - rateDriftIn);
        rateDrift += vibratoPole * (rateDriftIn - rateDrift);
        depthDriftIn += vibratoPole * (depthDriftGain * draw() - depthDriftIn);
        depthDrift += vibratoPole * (depthDriftIn - depthDrift);
        bowWander = std::exp (decibel * bowDrift);
        // The bow's fast randomness: the hair's grip on each partial wavers
        // (its loss: a driven partial's level goes as one over it, so only
        // the partial's own ring moves, and no faster than it can; Spitfire's
        // upper partials are grainy lines, its low ones hold steady), and the
        // pitch drifts slowly (the player's intonation).
        const auto drawFast = [this]
        {
            flutterSeed = flutterSeed * 1664525u + 1013904223u;
            return (float) (flutterSeed >> 8) * (2.0f / 16777216.0f) - 1.0f;
        };
        for (int n = 0; n < modes; ++n)
        {
            flutterIn[n] += flutterPole * (flutterGain * flutterShape[n] * drawFast() - flutterIn[n]);
            // (bounded far past any real swing, for fastExp)
            flutter[n] = juce::jlimit (-600.0f, 600.0f, flutter[n] + flutterPole * (flutterIn[n] - flutter[n]));
        }
        pitchDriftIn += pitchPole * (pitchGain * drawFast() - pitchDriftIn);
        pitchDrift += pitchPole * (pitchDriftIn - pitchDrift);

        const auto seconds = (double) elapsed / sampleRate;
        vibratoPhase += (float) (juce::MathConstants<double>::twoPi * vibratoRate * (1.0 + (double) rateDrift) * blockSize / sampleRate);
        if (vibratoPhase > juce::MathConstants<float>::twoPi)
            vibratoPhase -= juce::MathConstants<float>::twoPi;
        const auto onset = juce::jlimit (0.0, 1.0, (seconds - vibratoDelay) / vibratoRise);
        const auto cents = (double) vibratoCents * (1.0 + (double) depthDrift) * onset * std::sin ((double) vibratoPhase) + (double) pitchDrift;
        const auto ratio = std::exp2 (cents / 1200.0);
        const auto k = juce::MathConstants<double>::twoPi / sampleRate;
        // (The ripple's two sines at each mode's place plus the swing, by
        // the angle sum: the swing is the same for every mode.)
        const auto swing = juce::MathConstants<double>::twoPi * rippleDensity * cents / 1200.0;
        const auto c1 = (float) std::cos (swing) - 1.0f, s1 = (float) std::sin (swing);
        const auto c2 = (float) std::cos (goldenRatio * swing) - 1.0f, s2 = (float) std::sin (goldenRatio * swing);
        const auto half = 0.5f * rippleDb;
        const auto inverseRate = (float) (1.0 / sampleRate);
        // The onset's brightness: each upper partial's share of the bridge
        // force starts onsetDarkDb down (onsetDark[n]) and comes back over
        // onsetDarkSeconds (Spitfire's upper partials build up after the
        // fundamental: a gentle stroke's rounded Helmholtz corner).
        const auto dark = (float) std::exp (-seconds / onsetDarkSeconds);
        for (int n = 0; n < modes; ++n)
        {
            // (with the hair on the string: the grip eases off as the bow
            // lifts; held within about flutterClip dB either way, as an
            // unbounded swing would choke a partial for a moment)
            const auto grip = flutterClip * (1.0f - 2.0f / (fastExp (2.0f * envelope * flutter[n] * (1.0f / flutterClip)) + 1.0f)); // (tanh)
            decayNow[n] = fastExp (-lossOf[n] * fastExp (-decibel * grip) * inverseRate);
            fall[n] = decayNow[n] * decayNow[n];
        }
        for (int n = 0; n < modes; ++n)
        {
            const auto decay = decayNow[n];
            turn[n] = (float) (2.0 * (double) decay * std::cos (juce::jmin ((double) modeHz[n] * ratio, sampleRate * 0.49) * k));
            const auto ripple = half * (rippleSin[n] * c1 + rippleCos[n] * s1 + rippleSin2[n] * c2 + rippleCos2[n] * s2);
            // (glided to over the block: a step would zip)
            outStep[n] = (outBase[n] * std::exp (decibel * (ripple + wander[n] - dark * onsetDark[n])) - out[n]) * (1.0f / (float) blockSize);
        }
    }

    // The body's fine ripple: two constant-Q sines in log frequency, scaled
    // with the body's size; where each mode sits on it.
    void updateRipple()
    {
        for (int n = 0; n < modes; ++n)
        {
            const auto octave = std::log2 ((double) modeHz[n] * (double) bodySize / 1000.0);
            const auto phase = juce::MathConstants<double>::twoPi * rippleDensity * octave;
            const auto phase2 = juce::MathConstants<double>::twoPi * rippleDensity * goldenRatio * octave + 1.3;
            // (The body's resonances thin out low down: a low partial pulses less.)
            const auto depth = juce::jmin (1.0, std::pow ((double) modeHz[n] * (double) bodySize / (double) rippleKnee, (double) rippleFreqSlope));
            rippleSin[n] = (float) (depth * std::sin (phase));
            rippleCos[n] = (float) (depth * std::cos (phase));
            rippleSin2[n] = (float) (depth * std::sin (phase2));
            rippleCos2[n] = (float) (depth * std::cos (phase2));
        }
    }

    // e^x to about 1e-4, for |x| under 87 (the grip's flutter runs per mode
    // per block: the library's exp and tanh there cost a third more CPU per
    // voice; no branches, so the loop over the modes goes in vectors).
    static float fastExp (float x)
    {
        const auto t = x * 1.442695041f;
        const auto whole = (int) (t + 128.0f) - 128; // (floor: t + 128 is positive)
        const auto f = t - (float) whole;
        const auto fraction = 1.0f + f * (0.6960656f + f * (0.2244943f + f * 0.0794402f));
        const auto bits = (std::int32_t) ((whole + 127) << 23);
        float scale;
        std::memcpy (&scale, &bits, sizeof (scale));
        return fraction * scale;
    }

    // Uniform noise at the block rate through two one-poles in a row
    // (smooth: no fast jitter to blur the partials): the pole for a corner
    // of hz, and the gain for a standard deviation of spread.
    void wanderFilter (double hz, float spread, float& pole, float& gain) const
    {
        const auto a = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * hz * blockSize / sampleRate);
        const auto p = 1.0 - a;
        pole = (float) a;
        gain = (float) ((double) spread / std::sqrt (a * a * a * a * (1.0 + p * p) / std::pow (1.0 - p * p, 3.0) / 3.0));
    }

    void updateModes()
    {
        modesDirty = false;
        const auto k = 1.0 / sampleRate;
        halfRate = (float) (0.5 * sampleRate);
        // SCALE: a longer string loses less per second and is less stiff.
        const auto lossScale = 1.0 / (double) stringScale;
        // The register's brightness: the upper strings lose less up high.
        const auto lossHigh = plucked ? (double) loss1 : (double) loss1 * std::pow (frequency / 440.0, -(double) brightness);
        const auto b = (double) inharmonic / ((double) stringScale * stringScale * stringScale);
        auto shapeSum = 0.0;
        for (int n = 0; n < modes; ++n)
        {
            const auto index = (double) (n + 1);
            const auto hz = juce::jmin (index * frequency * std::sqrt (1.0 + b * index * index), sampleRate * 0.49);
            lossUpper[n] = (float) (lossHigh * hz * hz * lossScale);
            const auto sigma = (double) loss0 * lossScale + (double) lossUpper[n];
            lossOf[n] = (float) juce::jmin (sigma, sampleRate); // (fastExp's range)
            const auto decay = std::exp (-sigma * k);
            turn[n] = (float) (2.0 * decay * std::cos (juce::MathConstants<double>::twoPi * hz * k));
            modeHz[n] = (float) hz;
            // (once let go, the mode loses releaseLossHz more a second at 1 kHz, by (hz / 1 kHz) ^ releaseLossSlope)
            releaseLoss[n] = (float) ((double) releaseLossHz * std::pow (hz / 1000.0, (double) releaseLossSlope));
            flutterShape[n] = (float) juce::jmin (1.0, std::pow (hz / (double) flutterKnee, (double) flutterFreqSlope));
            onsetDark[n] = (float) (onsetDarkDb * juce::jmin (1.0, std::pow (hz / (double) onsetDarkKnee, (double) onsetDarkSlope)));
            fall[n] = (float) (decay * decay);
            shape[n] = (float) std::sin (index * juce::MathConstants<double>::pi * (double) bowAt);
            shapeSum += (double) shape[n] * shape[n];
            // The bridge force's share: the mode's slope at the bridge (n pi / L).
            outBase[n] = (float) (index / (double) modes);
        }
        // With the string's mass normalised away (it cancels between the
        // bow's admittance and the force's push), the force on mode n is
        // 2 k phi_n (v_bow - v_string) table / sum phi^2.
        const auto admittance = juce::jmax (1.0e-6, shapeSum);
        for (int n = 0; n < modes; ++n)
            push[n] = (float) (2.0 * k * (double) shape[n] / admittance);
        forceScale = 1.0f;
        ringUpper = 1.0;
        float unused = 0.0f;
        wanderFilter (wanderHz, wanderDb, wanderPole, wanderGain);
        wanderFilter (wanderHz, bowDriftDb, unused, bowDriftGain);
        wanderFilter (vibratoWanderHz, rateWander, vibratoPole, rateDriftGain);
        wanderFilter (vibratoWanderHz, depthWander, unused, depthDriftGain);
        // (The flutter is deeper and quicker up high: the violin's against the cello's.)
        const auto pitchRatio = frequency / 440.0;
        wanderFilter (juce::jmin (100.0, flutterHz * std::pow (pitchRatio, (double) flutterHzSlope)), flutterDb * (float) std::pow (pitchRatio, (double) flutterSlope),
                      flutterPole, flutterGain);
        wanderFilter (pitchDriftHz, pitchDriftCents, pitchPole, pitchGain);
        updateRipple();
        attackCoefficient = (float) (1.0 - std::exp (-1.0 / (attackSeconds () * sampleRate)));
        releaseCoefficient = (float) (1.0 - std::exp (-1.0 / (releaseSeconds * sampleRate)));
        swellCoefficient = (float) (1.0 - std::exp (-1.0 / (swellSeconds * sampleRate)));
        hissPole = (float) std::exp (-juce::MathConstants<double>::twoPi * 8000.0 / sampleRate);
        tiltPole = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin ((double) tiltHz, sampleRate * 0.45) / sampleRate);
        biteFall = (float) std::exp (-1.0 / (0.08 * sampleRate));
        if (elapsed == 0)
            biteEnvelope = 1.0f;
        // The plucked string's amplitudes, in the bowed string's units (a
        // bow at speed v moves the string about v / (2 pi f); the factor
        // sets a pizzicato level with an arco note, measured).
        pluckScale = (float) (5.0 * (double) bowSpeed / (juce::MathConstants<double>::twoPi * juce::jmax (20.0, frequency)));
    }

    // The room's lines (prime lengths near roomMs times the ratios), each
    // line's gain for roomLowDecay dB a second at DC and its low-pass's pole
    // for roomHighDecay dB a second at 4 kHz, and the return for a wet level
    // of roomDb (energy, against the dry note, for a broad noise).
    void updateRoom()
    {
        static constexpr double ratios[roomLines] { 0.8013, 0.9137, 1.0798, 1.2052 };
        const auto isPrime = [] (int n) { for (int k = 3; k * k <= n; k += 2) if (n % k == 0) return false; return true; };
        auto meanSquare = 0.0;
        auto total = 0;
        for (int r = 0; r < roomLines; ++r)
        {
            auto length = juce::jmax (3, (int) std::lround (roomMs * 1.0e-3 * sampleRate * ratios[r])) | 1;
            while (! isPrime (length))
                length += 2;
            roomLength[r] = length;
            roomStart[r] = total;
            total += length;
            const auto seconds = (double) length / sampleRate;
            const auto feed = std::pow (10.0, -roomLowDecay * seconds / 20.0);
            const auto extra = juce::jmax (0.0, roomHighDecay - roomLowDecay) * seconds; // (dB more per pass at 4 kHz)
            const auto c = (std::pow (10.0, extra / 10.0) - 1.0) / (2.0 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * 4000.0 / sampleRate)));
            const auto pole = c > 1.0e-12 ? ((2.0 * c + 1.0) - std::sqrt (4.0 * c + 1.0)) / (2.0 * c) : 0.0;
            roomPole[r] = (float) pole;
            roomFeed[r] = (float) (feed * (1.0 - pole));
            meanSquare += feed * feed / roomLines;
        }
        roomBuffer.assign ((size_t) total, 0.0f);   // (prepare only: about 14 KB at 48 kHz)
        roomScale = std::sqrt ((1.0 - meanSquare) / roomLines);
        updateRoomReturn();
    }

    // The room's return: roomDb for a violin, roomSizeDb more per doubling of the body's size.
    void updateRoomReturn() { roomReturn = (float) (std::pow (10.0, (roomDb + roomSizeDb * std::log2 ((double) bodySize)) / 20.0) * roomScale); }

    // The bow's contact: a low string takes longer to speak (as on the Bow
    // exciter); the swell after it shapes the note's rise.
    double attackSeconds() const { return 0.0025 * juce::jlimit (1.0, 2.5, 220.0 / frequency); }

    void updateBody()
    {
        bodyDirty = false;
        updateRoomReturn();
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
        auto loudest = -200.0;
        for (int j = 0; j < B::count; ++j)
            loudest = juce::jmax (loudest, (double) lower[j] + blend * ((double) upper[j] - (double) lower[j]));
        bodyDirect = (float) std::pow (10.0, ((double) directDb + loudest) / 20.0);
        {
            // (Two poles, Butterworth, at directHp for a violin, lower for a bigger box.)
            const auto w0 = juce::MathConstants<double>::twoPi * juce::jmin ((double) directHp / size, sampleRate * 0.45) / sampleRate;
            const auto alpha = std::sin (w0) / juce::MathConstants<double>::sqrt2; // (Q = 1 / sqrt 2)
            const auto cw = std::cos (w0);
            const auto a0 = 1.0 + alpha;
            hpB0 = hpB2 = (float) ((1.0 + cw) * 0.5 / a0);
            hpB1 = (float) (-(1.0 + cw) / a0);
            hpA1 = (float) (-2.0 * cw / a0);
            hpA2 = (float) ((1.0 - alpha) / a0);
        }
        {
            // (and low-passed, two poles, at directLp for a violin, lower for a bigger box)
            const auto hz = (double) directLp / size;
            if (hz < sampleRate * 0.45)
            {
                const auto w0 = juce::MathConstants<double>::twoPi * hz / sampleRate;
                const auto alpha = std::sin (w0) / juce::MathConstants<double>::sqrt2;
                const auto cw = std::cos (w0);
                const auto a0 = 1.0 + alpha;
                lpB0 = lpB2 = (float) ((1.0 - cw) * 0.5 / a0);
                lpB1 = (float) ((1.0 - cw) / a0);
                lpA1 = (float) (-2.0 * cw / a0);
                lpA2 = (float) ((1.0 - alpha) / a0);
            }
            else
            {
                lpB0 = 1.0f;
                lpB1 = lpB2 = lpA1 = lpA2 = 0.0f;
            }
        }
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
        updateRipple();
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
    float swell = 0.0f, swellCoefficient = 0.0001f, hairNoise = 0.0f, hissState = 0.0f, hissLast = 0.0f, hissPole = 0.35f, tilt = 0.0f, tiltPole = 0.6f;
    float vibratoCents = 9.0f, vibratoPhase = 0.0f;
    float wanderPole = 0.01f, wanderGain = 0.0f, bowDriftGain = 0.0f, vibratoPole = 0.01f, rateDriftGain = 0.0f, depthDriftGain = 0.0f;
    float bowDrift = 0.0f, rateDrift = 0.0f, depthDrift = 0.0f, bowDriftIn = 0.0f, rateDriftIn = 0.0f, depthDriftIn = 0.0f;
    float bowWander = 1.0f, bodyDirect = 0.0f, liftSpeed = 0.0f;
    float flutterPole = 0.1f, flutterGain = 0.0f, pitchPole = 0.01f, pitchGain = 0.0f, pitchDrift = 0.0f, pitchDriftIn = 0.0f;
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f, hx1 = 0.0f, hx2 = 0.0f, hy1 = 0.0f, hy2 = 0.0f;
    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f, lx1 = 0.0f, lx2 = 0.0f, ly1 = 0.0f, ly2 = 0.0f;
    double ringUpper = 1.0;   // (the release's loss factor the modes have now)
    std::uint32_t seed = 1, driftSeed = 1, flutterSeed = 1;
    // Fitted to the Spitfire long notes (2026-11-08, see the top).
    // The bow's onset (round 6): the stroke starts at 1 - swellDepth of its
    // speed and swells over swellSeconds (a bow landing slower stuck to the
    // string for ~30 ms before it spoke); the upper partials come in darker
    // (onsetDarkDb at onsetDarkKnee and over, onsetDarkSlope below it) and
    // brighten over onsetDarkSeconds.
    static constexpr double swellSeconds = 0.3544;
    static constexpr float onsetDarkDb = 15.04f, onsetDarkKnee = 1990.0f, onsetDarkSlope = 2.82f;
    static constexpr double onsetDarkSeconds = 0.1429;
    static constexpr float swellDepth = 0.8208f, rosinJitter = 0.08536f, hissLevel = 3.336e-04f, bodyNoise = 1.695e-06f;
    static constexpr float brightness = 2.144f, topHz = 12000.0f, tiltHz = 5590.0f, directDb = -10.33f;
    // The body's fine ripple (dB, cycles per octave) the vibrato sweeps the
    // partials across.
    static constexpr float rippleDb = 3.326f, rippleDensity = 19.46f;
    // The slow wander (dB standard deviation and corner): each partial's
    // level on its own, the bow's speed; the vibrato's rate and depth
    // (fractions; Spitfire's vibrato is never quite even).
    static constexpr float wanderDb = 2.052f, bowDriftDb = 1.742f, rateWander = 0.09024f, depthWander = 0.4561f;
    static constexpr double wanderHz = 0.6, vibratoWanderHz = 0.8;
    // The bow's fast randomness: each partial's flutter (dB standard
    // deviation and corner at A4, and how both grow with the register), the
    // pitch's slow drift (cents).
    static constexpr float flutterDb = 21.35f, flutterSlope = 0.0f, flutterHzSlope = 0.7853f, pitchDriftCents = 1.441f;
    static constexpr double flutterHz = 16.51, pitchDriftHz = 0.5;
    static constexpr float flutterKnee = 818.2f, flutterFreqSlope = 1.285f, rippleKnee = 963.9f, rippleFreqSlope = 2.074f, directHp = 96.61f, directLp = 20000.0f;
    static constexpr float flutterClip = 30.0f;
    static constexpr double vibratoRate = 5.2, vibratoDelay = 0.25, vibratoRise = 0.4;
    // The bow's release: its pressure eases off over releaseSeconds as it
    // lifts, the upper partials come to lose releaseRing times more over
    // releaseRingSeconds, and the note fades at releaseFadeDb a second
    // (Spitfire's notes once let go).
    static constexpr double releaseSeconds = 1.0;
    static constexpr float releaseFadeDb = 35.79f, releaseRing = 17.84f;
    static constexpr double releaseRingSeconds = 0.4991;
    // Once let go, each mode also loses releaseLossHz more a second at
    // 1 kHz, by (hz / 1 kHz) ^ releaseLossSlope (ramped in with the ring).
    static constexpr float releaseLossHz = 23.94f, releaseLossSlope = 3.224f;
    // The room (fitted to the tails of Spitfire's long notes).
    static constexpr int roomLines = 4;
    static constexpr double roomDb = -2.5, roomSizeDb = -19.28, roomMs = 18.44, roomLowDecay = 38.88, roomHighDecay = 159.5;
    static constexpr float decibel = 0.1151292546f, goldenRatio = 1.618034f;
    // The output's level: the string's units are tiny (metres of string,
    // narrow resonators); this brings a violin D4 near a plucked string's.
    static constexpr float outputGain = 30000.0f;

    alignas (32) float q[maxModes] {}, qp[maxModes] {}, turn[maxModes] {}, fall[maxModes] {};
    alignas (32) float shape[maxModes] {}, push[maxModes] {}, out[maxModes] {}, outBase[maxModes] {}, modeHz[maxModes] {}, lossUpper[maxModes] {}, lossOf[maxModes] {}, decayNow[maxModes] {};
    alignas (32) float flutter[maxModes] {}, flutterIn[maxModes] {}, flutterShape[maxModes] {}, onsetDark[maxModes] {};
    alignas (32) float wander[maxModes] {}, wanderIn[maxModes] {}, outStep[maxModes] {}, rippleSin[maxModes] {}, rippleCos[maxModes] {}, rippleSin2[maxModes] {}, rippleCos2[maxModes] {};
    alignas (32) float bodyB0[maxBody] {}, bodyA1[maxBody] {}, bodyA2[maxBody] {}, bodyGain[maxBody] {};
    alignas (32) float bx1[maxBody] {}, bx2[maxBody] {}, by1[maxBody] {}, by2[maxBody] {};
    alignas (32) float releaseLoss[maxModes] {};
    std::vector<float> roomBuffer;   // (the four lines end to end)
    alignas (16) float roomLow[roomLines] {}, roomFeed[roomLines] {}, roomPole[roomLines] {};
    int roomLength[roomLines] {}, roomStart[roomLines] {}, roomAt[roomLines] {};
    float roomReturn = 0.0f;
    double roomScale = 0.0;
};
