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
//   WOLF         (the BUZZ knob) lets the body's strongest low resonance push
//                back on the string: a second resonator at that resonance
//                (from the fitted body, scaled by SIZE) is driven by the
//                bridge force, and its motion moves the bridge, a force on
//                each mode within an octave of it by the mode's slope there.
//                A partial near the resonance and the body then trade energy
//                back and forth, and the resonance sounds too: low, a little
//                life; by halfway (its damping halved) the note on it beats,
//                a cello's wolf (near G#3 at the cello's size, C4 on the
//                violin). Past halfway it howls: the resonance shakes the
//                bow off the string a few times a second, so the whole note
//                cuts in and out and the box rings louder; at the top the
//                notes within a few semitones and an octave below join in. At 0 the string
//                plays exactly as fitted.
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
// Round 8 (2026-11-10), scored on whole held-out samples and on each
// partial's level over time. The violin: nothing sounds under the note (the
// whole bridge is high-passed a third of the way up to the fundamental: the
// rosin's jitter hummed there); the stroke gathers speed from rest (two
// stages, an S) while the hair's noise is there from the start; and the
// let-go is Spitfire's two stages: a quick drop as the bow leaves, then a
// fall to a quiet tail of the room. The cello (all of it by SIZE, nothing at
// the violin's): more wood, less brass. Its spectrum was too smooth (too
// much of the bridge straight through, too little of the body's
// resonances), so the direct path drops 12 dB per doubling of SIZE under a
// refitted body, and its vibrato is shallower; its low partials ring on
// after the let-go while the upper ones die late and steeply. Between the
// two (a viola) the terms blend by the log of SIZE.
// Round 9 (2026-11-10): a table per note for each fitted instrument
// (ModalBowedNotes.h: each partial's level, and the violin's floor, let-go,
// onset, vibrato and wander, the cello's onset darkening, release losses
// and drops partial by partial), read at note-on by the nearest note and
// there only near the size it was fitted at. The violin: a quicker
// release, the hiss growing with the register, and the hall's own floor (a
// pink noise about 40 dB under the note that rises with it and fades
// once let go). The cello: shallower flutter and wander, more wood, more
// hiss, and the bow's noise kept up under the tail. The violin's retuned
// terms blend back to round 8's at the cello's size and the cello's in from
// round 8's at the violin's, by the log of SIZE. The pizzicato keeps round
// 8's box (only the cello's body gains, 1.36 dB down, reach it).
// Eco keeps half the modes and the body's lower half.

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "ModalBowedBodies.h"
#include "ModalBowedNotes.h"

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
        updateRoom (true);
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
        pink0 = pink1 = pink2 = floorLow = floorHigh = floorLast = floorEnvelope = floorStep = 0.0f;
        wolfMove = wolfMoveLast = wolfRing = wolfFed = 0.0f; wolfGrip = 1.0f; wolfLost = false; wolfClock = 0;
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
                    float damper, bool pizz, bool eco, float vibrato = 0.5f, float wolf = 0.0f)
    {
        // At the Physical card's defaults (DECAY 0.75, DAMP 0.35, STIFF 0)
        // the losses and stretch of the sketch fitted to the Iowa violin.
        const auto lossAll = 0.8f * std::exp2 ((0.75f - juce::jlimit (0.0f, 1.0f, decay)) * 5.0f);
        // (The upper modes' loss: bowed, fitted to Spitfire's long notes, the
        // register's brightness on top in updateModes; plucked, as fitted to
        // the Iowa violin. Bowed, this is DAMP's factor: stringLossHigh by SIZE in updateModes.)
        const auto dampScale = std::exp2 ((juce::jlimit (0.0f, 1.0f, damping) - 0.35f) * 5.0f);
        const auto lossHigh = (pizz ? 3.0e-5f : 1.0f) * dampScale;
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
        const auto w = juce::jlimit (0.0f, 1.0f, wolf);
        if (w != wolfAmount)
        {
            wolfAmount = w;
            updateWolf();
        }
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
            modesDirty = true;   // (round 9: the string's terms blend by SIZE too)
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
        swell = swellSecond = liftSpeed = 0.0f;
        jitterRatio = letGo = tailNoiseGain = 1.0f;
        letGoStep = 0.0f;
        hissState = hissLast = tilt = 0.0f;
        // (Round 9) The per-note tables: the rows for this note (its pitch now,
        // to the nearest semitone), read here only. Bowed only.
        noteHz = frequency;
        const auto midi = (int) std::floor (69.0 + 12.0 * std::log2 (frequency / 440.0) + 0.5);
        for (int i = 0; i < ModalBowedNotes::tableCount; ++i)
        {
            const auto& table = ModalBowedNotes::tables[i];
            auto note = midi;
            auto weight = 1.0;
            if (note < table.first)
            {
                note = table.first;
                weight = table.holdBelow ? 1.0 : 0.0;
            }
            else if (note > table.last)
            {
                weight = table.fadeAbove > 0 ? juce::jmax (0.0, 1.0 - (double) (note - table.last) / table.fadeAbove) : 0.0;
                note = table.last;
            }
            noteRow[i] = plucked || weight <= 0.0 ? nullptr : table.rows + (note - table.first);
            noteWeight[i] = weight;
        }
        floorSeed = seed ^ 0x27D4EB2Fu;
        pink0 = pink1 = pink2 = floorLow = floorHigh = floorLast = floorEnvelope = floorStep = 0.0f;
        noiseHeld = noiseFloorNow = dropNow = 0.0f;
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
        wolfMove = wolfMoveLast = wolfRing = wolfFed = 0.0f; wolfGrip = 1.0f; wolfLost = false; wolfClock = 0;
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
        // (WOLF's path only when it is up: set once a block, so at 0 the
        // string runs exactly as it did, at no cost.)
        return wolfActive ? step<true> (expression, noteHeld) : step<false> (expression, noteHeld);
    }

private:
    template <bool wolf>
    float step (float expression, bool noteHeld)
    {
        // The modes' next state without the bow (with WOLF, the bridge's
        // motion pushes each mode by its slope there).
        float next[maxModes];
        auto bowVelocity = 0.0f;
        for (int n = 0; n < modes; ++n)
        {
            next[n] = turn[n] * q[n] - fall[n] * qp[n];
            bowVelocity += shape[n] * (next[n] - qp[n]);
        }
        if constexpr (wolf)
        {
            for (int n = wolfFirst; n < wolfEnd; ++n)
                next[n] += wolfPush[n] * wolfMove;
            bowVelocity += wolfShape * wolfMove;
        }
        bowVelocity *= halfRate;

        auto force = 0.0f;
        if (! plucked)
        {
            // The envelope: the bow sets on over attackSeconds, eases off on release.
            envelope += ((noteHeld ? 1.0f : 0.0f) - envelope) * (noteHeld ? attackCoefficient : releaseCoefficient);
            const auto swellStep = noteHeld ? swellOnCoefficient : swellCoefficient;
            swell += ((noteHeld ? 1.0f : 0.0f) - swell) * swellStep;
            // (Round 8: a second such stage after the first, part of the way:
            // an S, the bow gathering speed from rest.)
            swellSecond += (swell - swellSecond) * swellStep;
            if (envelope > 1.0e-5f)
            {
                seed = seed * 1664525u + 1013904223u;
                const auto rosin = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f);
                seed = seed * 1664525u + 1013904223u;
                // On release the bow lifts off: its pressure eases (the
                // envelope) but it keeps the speed it had, so it doesn't
                // brake the string to a stop.
                const auto built = swell + swellSecondAtSize * (swellSecond - swell);
                const auto stroke = envelope * (1.0f - swellDepthAtSize + swellDepthAtSize * built);
                if (noteHeld)
                    liftSpeed = stroke;
                const auto speed = bowSpeed * bowWander * liftSpeed;
                // (Round 8: the hair's noise is there from the start, on a
                // shallower swell of its own: Spitfire's floor between the
                // partials is up within 50 ms, the partials take half a
                // second. The rosin's jitter too, by jitterRatio.)
                auto noiseStroke = envelope * (1.0f - noiseSwellAtSize + noiseSwellAtSize * built);
                // (Round 9, the cello's: once let go, no lower than the
                // release floor, noiseFloorNow, set by nextBlock.)
                if (noteHeld)
                    noiseHeld = noiseStroke;
                else
                    noiseStroke = juce::jmax (noiseStroke, noiseFloorNow);
                hairNoise = ((float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f) * bowSpeed * bowWander * noiseStroke;
                const auto bite = 1.0f + 0.6f * velocityLevel * biteEnvelope;
                auto bow = speed * bite * (1.0f + rosin * rosinJitter * jitterRatio);
                if constexpr (wolf)
                    bow *= wolfGrip;
                auto pressure = juce::jlimit (0.0f, 1.0f, grip + expression * 0.75f);
                if constexpr (wolf)
                    pressure = juce::jlimit (0.0f, 1.0f, pressure + wolfPress * (1.0f - wolfGrip));   // (the lost bow digs in)
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
        if constexpr (wolf)
        {
            // The body's main resonance, driven by the bridge force; its
            // motion is the bridge's, which the modes feel next sample.
            // (Bounded, as a safety.)
            const auto moved = wolfTurn * wolfMove - wolfFall * wolfMoveLast + wolfDrive * bridge;
            wolfMoveLast = wolfMove;
            wolfMove = juce::jlimit (-wolfLimit, wolfLimit, moved);
            const auto driven = wolfDrive * bridge;
            wolfRing += (wolfMove * wolfMove - wolfRing) * wolfRingStep;
            wolfFed += (driven * driven - wolfFed) * wolfRingStep;
            // The howl: the resonance ringing far over what feeds it shakes
            // the bow off the string for a moment (a real wolf's stutter).
            if (wolfClock > 0)
            {
                if (--wolfClock == wolfRest)
                    wolfLost = false;
            }
            else if (wolfFloor < 1.0f && wolfRing > wolfTrip * wolfFed)
            {
                wolfLost = true;
                wolfClock = wolfHold + wolfRest;
            }
            wolfGrip += ((wolfLost ? wolfFloor : 1.0f) - wolfGrip) * wolfGripStep;
        }
        // The hair's scrape reaches the bridge too, coloured by the body.
        bridge = (bridge + bodyNoiseNow * tailNoiseGain * hairNoise) * level;
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
        // (Round 8: bowed, the whole bridge goes through it, the resonators
        // too: no hum under the note.)
        if (! plucked)
            bridge = radiated;
        // (and falls away up high: a big box's broad radiation is darker)
        const auto darker = lpB0 * radiated + lpB1 * lx1 + lpB2 * lx2 - lpA1 * ly1 - lpA2 * ly2;
        lx2 = lx1;
        lx1 = radiated;
        ly2 = ly1;
        ly1 = darker;
        auto sound = bodyDirect * (plucked ? bridge : darker);
        if constexpr (wolf)
            sound += wolfRadiate * level * (wolfMove - wolfMoveLast);   // (the resonance's own sound: its speed, so no DC)
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
        sound += hissNow * hiss;
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
            // (Round 8: the let-go's gain, after the room; see nextBlock.)
            letGo += letGoStep;
            sound *= letGo;
            // (Round 9, the violin's) The hall's floor: a pink noise (Kellet's
            // three poles), one-pole low- and high-passed, after the let-go's
            // gain (it does not drop with the note); its envelope from nextBlock.
            if (floorGain > 0.0f)
            {
                floorSeed = floorSeed * 1664525u + 1013904223u;
                const auto white = (float) (floorSeed >> 8) * (2.0f / 16777216.0f) - 1.0f;
                pink0 = 0.99765f * pink0 + white * 0.0990460f;
                pink1 = 0.96300f * pink1 + white * 0.2965164f;
                pink2 = 0.57000f * pink2 + white * 1.0526913f;
                const auto pink = pink0 + pink1 + pink2 + white * 0.1848f;
                floorLow = pink + floorLowPole * (floorLow - pink);
                floorHigh = floorHighPole * (floorHigh + floorLow - floorLast);
                floorLast = floorLow;
                floorEnvelope += floorStep;
                sound += floorGain * floorEnvelope * (floorHigh + (float) hallFloorWhite * white);
            }
        }
        // A gentle roll-off (tiltHz) sets the top of the spectrum.
        tilt = sound + tiltPole * (tilt - sound);
        sound = tilt;
        return std::isfinite (sound) ? juce::jlimit (-8.0f, 8.0f, sound) : 0.0f;
    }

    void nextBlock (bool noteHeld)
    {
        untilBlock = blockSize;
        if (modesDirty)
            updateModes();
        if (bodyDirty)
            updateBody();
        wolfActive = wolfOn;
        if (wolfActive && ! std::isfinite (wolfMove + wolfMoveLast))
            wolfMove = wolfMoveLast = 0.0f;
        // Let go (bowed): the upper partials' losses rise to releaseRing
        // times over releaseRingSeconds (the lifting hair stops feeding them).
        if (releasedAt >= 0 && ! plucked)
        {
            const auto gone = juce::jmin (1.0, (double) (elapsed - releasedAt) / (juce::jmax (1.0e-3, releaseRingSeconds) * sampleRate));
            const auto upper = 1.0 + (releaseRingAtSize - 1.0) * gone;
            // (Round 8: on a bigger body the released partials' extra loss
            // comes in late, as gone ^ releaseRampPower; 1 on a violin.)
            const auto late = releaseRampPower == 1.0 ? gone : std::pow (gone, releaseRampPower);
            if (upper != ringUpper || gone < 1.0)
            {
                ringUpper = upper;
                const auto low = (double) loss0 / (double) stringScale;
                for (int n = 0; n < modes; ++n)
                {
                    lossOf[n] = (float) juce::jmin (low + (double) lossUpper[n] * upper + late * (double) releaseLoss[n], sampleRate); // (fastExp's range)
                    const auto decay = std::exp (-(double) lossOf[n] / sampleRate);
                    fall[n] = (float) (decay * decay);
                }
            }
        }
        // The let-go in two stages (round 8), as Spitfire's release samples:
        // a quick drop (letGoDropDb over letGoDropSeconds: the bow off the
        // string), a short hold, then a fall of tailDb more (a smooth step
        // from tailDelay over tailSeconds) to a quiet tail; the room's own
        // ring goes on under it, that much quieter. A gain after the room,
        // glided over the block. The hair's noise is kept out of most of
        // that fall (tailNoise of it, in dB): the lifted bow's breath
        // carries on under the tail, fading with the bow's pressure.
        if (! plucked)
        {
            if (releasedAt >= 0)
            {
                const auto since = (double) (elapsed + blockSize - releasedAt) / sampleRate;
                const auto x = juce::jlimit (0.0, 1.0, (since - tailDelay) / tailSeconds);
                const auto down = letGoDropAtSize * (1.0 - std::exp (-since / letGoDropSeconds)) + tailDbAtSize * x * x * (3.0 - 2.0 * x);
                const auto target = (float) std::exp (-(double) decibel * down);
                letGoStep = (target - letGo) * (1.0f / (float) blockSize);
                tailNoiseGain = (float) std::exp ((double) decibel * down * tailNoiseAtSize);
                // (Round 9, the cello's: the bow's noise falls no lower than
                // noiseFloorAtSize of its level at the let-go, that floor
                // fading noiseFloorFade dB a second; each partial drops its
                // table's dropDb over partialDropSeconds, in updateVibrato.)
                noiseFloorNow = noiseFloorAtSize > 0.0f ? (float) (noiseFloorAtSize * noiseHeld * std::exp (-(double) decibel * noiseFloorFade * since)) : 0.0f;
                dropNow = (float) (1.0 - std::exp (-since / partialDropSeconds));
                // (Round 9, the violin's: the hall's floor steps up
                // hallFloorLetGoDb over hallFloorLetGoSeconds as the bow
                // leaves and fades hallFloorFade dB a second.)
                if (floorGain > 0.0f)
                {
                    const auto stepUp = 1.0 + (std::pow (10.0, hallFloorLetGoDb / 20.0) - 1.0) * juce::jmin (1.0, since / hallFloorLetGoSeconds);
                    floorStep = ((float) (stepUp * std::pow (10.0, -hallFloorFade * since / 20.0)) - floorEnvelope) * (1.0f / (float) blockSize);
                }
            }
            else
            {
                if (noteHeld)
                {
                    // (The rosin's jitter is as strong against the full stroke
                    // from the start; held at its last on the let-go.)
                    const auto built = swell + swellSecondAtSize * (swellSecond - swell);
                    jitterRatio = (1.0f - noiseSwellAtSize + noiseSwellAtSize * built) / (1.0f - swellDepthAtSize + swellDepthAtSize * built);
                }
                // (the hall's floor rises over hallFloorRise from the note-on)
                floorStep = floorRise * (1.0f - floorEnvelope) * (1.0f / (float) blockSize);
            }
        }
        updateVibrato();
        // Released: the bowed note fades as Spitfire's do, and DAMPER takes
        // the string's level down faster (a pluck at 0 rings on).
        if (releasedAt >= 0 && (damperAmount > 0.0f || ! plucked))
        {
            const auto seconds = (float) (blockSize / sampleRate);
            level *= std::exp2 (-(damperAmount * 60.0f + (plucked ? 0.0f : releaseFadeAtSize / 6.0206f)) * seconds);
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
            // (letGo: the let-go's gain on all of it, 1 for a pluck)
            // (and the hall's floor gone as far under the note's loudest)
            if (((energy * level * letGo < 3.0e-5f * peakBridge && body * letGo < 3.0e-5f * peakBody) || level < 1.0e-5f)
                && room * roomReturn * letGo < 3.0e-5f * peakBody * outputGain
                && floorGain * floorEnvelope < 3.0e-5f * peakBody * outputGain)
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
        const auto cents = (double) vibratoCents * vibratoAtSize * vibratoNote * (1.0 + (double) depthDrift) * onset * std::sin ((double) vibratoPhase) + (double) pitchDrift;
        const auto ratio = std::exp2 (cents / 1200.0);
        const auto k = juce::MathConstants<double>::twoPi / sampleRate;
        // (The ripple's two sines at each mode's place plus the swing, by
        // the angle sum: the swing is the same for every mode.)
        const auto swing = juce::MathConstants<double>::twoPi * rippleDensity * cents / 1200.0;
        const auto c1 = (float) std::cos (swing) - 1.0f, s1 = (float) std::sin (swing);
        const auto c2 = (float) std::cos (goldenRatio * swing) - 1.0f, s2 = (float) std::sin (goldenRatio * swing);
        const auto half = 0.5f * rippleDbNow;
        const auto inverseRate = (float) (1.0 / sampleRate);
        // The onset's brightness: each upper partial's share of the bridge
        // force starts onsetDarkDb down (onsetDark[n]) and comes back over
        // onsetDarkSeconds (Spitfire's upper partials build up after the
        // fundamental: a gentle stroke's rounded Helmholtz corner).
        const auto dark = (float) std::exp (-seconds / onsetDarkSecondsAtSize);
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
            // (round 9: less each partial's own drop at the let-go, the cello's table's)
            outStep[n] = (outBase[n] * std::exp (decibel * (ripple + wander[n] - dark * onsetDark[n] - releaseDrop[n] * dropNow)) - out[n]) * (1.0f / (float) blockSize);
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
        updateFit();
        const auto k = 1.0 / sampleRate;
        halfRate = (float) (0.5 * sampleRate);
        // SCALE: a longer string loses less per second and is less stiff.
        const auto lossScale = 1.0 / (double) stringScale;
        // The register's brightness: the upper strings lose less up high.
        const auto lossHigh = plucked ? (double) loss1 : (double) loss1 * stringLossNow * std::pow (frequency / 440.0, -brightnessNow);
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
            flutterShape[n] = (float) juce::jmin (1.0, std::pow (hz / (double) flutterKnee, (double) flutterFreqSlope));
            fall[n] = (float) (decay * decay);
            shape[n] = (float) std::sin (index * juce::MathConstants<double>::pi * (double) bowAt);
            shapeSum += (double) shape[n] * shape[n];
            // The bridge force's share: the mode's slope at the bridge (n pi / L).
            // (Round 9: the per-note tables' dB on it, by their weights.)
            auto gainDb = 0.0;
            for (int i = 0; i < ModalBowedNotes::tableCount; ++i)
                if (tableWeight[i] > 0.0)
                    gainDb += tableWeight[i] * (double) noteRow[i]->gainDb[n];
            outBase[n] = (float) (index / (double) modes * (gainDb != 0.0 ? std::exp ((double) decibel * gainDb) : 1.0));
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
        wanderFilter (wanderHz, wanderDbNow, wanderPole, wanderGain);
        wanderFilter (wanderHz, bowDriftDbNow, unused, bowDriftGain);
        wanderFilter (vibratoWanderHz, rateWander, vibratoPole, rateDriftGain);
        wanderFilter (vibratoWanderHz, depthWander, unused, depthDriftGain);
        // (The flutter is deeper and quicker up high: the violin's against the cello's.)
        const auto pitchRatio = frequency / 440.0;
        wanderFilter (juce::jmin (100.0, flutterHz * std::pow (pitchRatio, (double) flutterHzSlope)), flutterDbNow * (float) std::pow (pitchRatio, (double) flutterSlope),
                      flutterPole, flutterGain);
        wanderFilter (pitchDriftHz, pitchDriftCents, pitchPole, pitchGain);
        updateRipple();
        attackCoefficient = (float) (1.0 - std::exp (-1.0 / (attackSeconds () * sampleRate)));
        updateSizeTerms();
        updateHighPass();
        hissPole = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (hissHpNow, sampleRate * 0.45) / sampleRate);
        tiltPole = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (tiltHzNow, sampleRate * 0.45) / sampleRate);
        floorLowPole = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (hallFloorLowHz, sampleRate * 0.45) / sampleRate);
        floorHighPole = (float) std::exp (-juce::MathConstants<double>::twoPi * hallFloorHighHz / sampleRate);
        floorRise = (float) (1.0 - std::exp (-(double) blockSize / (juce::jmax (1.0e-3, hallFloorRise) * sampleRate)));
        biteFall = (float) std::exp (-1.0 / (0.08 * sampleRate));
        if (elapsed == 0)
            biteEnvelope = 1.0f;
        // The plucked string's amplitudes, in the bowed string's units (a
        // bow at speed v moves the string about v / (2 pi f); the factor
        // sets a pizzicato level with an arco note, measured).
        pluckScale = (float) (5.0 * (double) bowSpeed / (juce::MathConstants<double>::twoPi * juce::jmax (20.0, frequency)));
        updateWolf();
    }

    // The room's lines (prime lengths near roomMs times the ratios), each
    // line's gain for roomLowDecay dB a second at DC and its low-pass's pole
    // for roomHighDecay dB a second at 4 kHz, and the return for a wet level
    // of roomDb (energy, against the dry note, for a broad noise).
    // SIZE for the round 7 terms: the default reads back a hair under 1
    // (0.99999994), taken as 1 so the fitted violin plays as it did.
    // (Round 9: likewise the cello's, so its size plays the cello's terms exactly, the violin's hall floor off.)
    double fittedSize() const
    {
        return std::abs (bodySize - 1.0f) < 1.0e-4f ? 1.0 : std::abs (bodySize - (float) ModalBowedBodies::cello) < 1.0e-4f ? ModalBowedBodies::cello : (double) bodySize;
    }

    // The terms SIZE scales, and (round 9) the note's tables, as values:
    // set with the modes and the body. Round 7: the cello's onset, swell and
    // released partials. Round 8: the violin's new onset and let-go blend
    // back to their round 7 values at the cello's size (by the log of SIZE,
    // held between the two), and the cello's own terms are powers of SIZE on
    // top: all 1 at the violin's, and held at the violin's or the cello's
    // past either end (they were fitted between the two). Round 9: likewise
    // the violin's retuned terms back to round 8's (...Cello) and the
    // cello's new values in from the violin's; the violin's hall floor
    // fades out by 1 - toCello (in level), the cello's release floor in by
    // toCello. A per-note table weighs 1 at the size it was fitted at and
    // nothing a quarter of the way (log size) to the other instrument; its
    // scalars replace the value at this size before the size powers.
    void updateFit()
    {
        const auto size = fittedSize();
        const auto span = juce::jlimit (1.0, ModalBowedBodies::cello, size);
        const auto toCello = juce::jlimit (0.0, 1.0, std::log (size) / std::log (ModalBowedBodies::cello));
        const auto at = [toCello] (double violin, double cello) { return toCello >= 1.0 ? cello : violin + (cello - violin) * toCello; };
        const auto u = std::log (size) / std::log (ModalBowedBodies::cello);
        for (int i = 0; i < ModalBowedNotes::tableCount; ++i)
            tableWeight[i] = noteRow[i] == nullptr ? 0.0
                                                   : noteWeight[i] * juce::jlimit (0.0, 1.0, 1.0 - 4.0 * std::abs (u - std::log (ModalBowedNotes::tables[i].size) / std::log (ModalBowedBodies::cello)));
        const auto note = [this] (int which, double value)
        {
            for (int i = 0; i < ModalBowedNotes::tableCount; ++i)
                if (tableWeight[i] > 0.0 && (ModalBowedNotes::tables[i].scalars >> which & 1u) != 0)
                {
                    const auto v = (double) noteRow[i]->scalar[which];
                    value = tableWeight[i] >= 1.0 ? v : value + (v - value) * tableWeight[i];
                }
            return value;
        };
        using S = ModalBowedNotes;
        const auto swellTime = note (S::swellSeconds, at (swellSeconds, swellSecondsCello));
        swellCoefficient = (float) (1.0 - std::exp (-1.0 / (swellTime * sampleRate)));
        swellOnCoefficient = (float) (1.0 - std::exp (-1.0 / (swellTime * std::pow (size, swellSize) * sampleRate)));
        swellDepthAtSize = (float) at (swellDepth, swellDepthCello);
        swellSecondAtSize = (float) at (swellSecondStage, 0.0);
        noiseSwellAtSize = (float) at (noiseSwellDepth, swellDepthCello);   // (the cello's: the stroke's own)
        releaseCoefficient = (float) (1.0 - std::exp (-1.0 / (note (S::releaseSeconds, at (releaseSeconds, releaseSecondsCello)) * sampleRate)));
        releaseFadeAtSize = (float) (note (S::fadeDb, at (releaseFadeDb, releaseFadeDbCello)) * std::pow (span, releaseFadeSizeSlope));
        releaseRingAtSize = 1.0 + (note (S::releaseRing, at (releaseRing, releaseRingCello)) - 1.0) * std::pow (span, releaseRingSizeSlope);
        releaseRampPower = std::pow (span, releaseRampSizeSlope);
        letGoDropAtSize = note (S::letGoDropDb, at (letGoDropDb, 0.0));
        tailDbAtSize = note (S::tailDb, at (tailDb, 0.0));
        tailNoiseAtSize = at (tailNoise, 0.0);
        onsetDarkSecondsAtSize = at (onsetDarkSeconds, onsetDarkSecondsCello) * std::pow (span, onsetDarkSecondsSizeSlope);
        vibratoAtSize = std::pow (span, vibratoSizeSlope);
        vibratoNote = (float) (note (S::vibratoCents, vibratoFitted) / vibratoFitted);
        releaseLossAtSize = at (releaseLossHz, releaseLossHzCello) * std::pow (size, (double) releaseLossSizeSlope);
        releaseLossSlopeAtSize = (double) releaseLossSlope * std::pow (span, releaseLossSlopeSizeSlope);
        onsetDarkDbAtSize = note (S::onsetDarkDb, at (onsetDarkDb, onsetDarkDbCello));
        onsetDarkSlopeAtSize = at (onsetDarkSlope, onsetDarkSlopeCello);
        onsetDarkKneeAtSize = at (onsetDarkKnee, onsetDarkKneeCello) / std::pow (size, (double) onsetDarkKneeSize);
        // (Round 9: the string, the noise, the wander and the box. The
        // pizzicato keeps round 8's box: its fit was the Iowa violin's.)
        brightnessNow = at (brightness, brightnessCello);
        stringLossNow = at (stringLossHigh, stringLossHighCello);
        attackScaleNow = at (attackScale, attackScaleCello);
        hissHpNow = at (hissHp, hissHpCello);
        hissNow = (float) (at (hissLevel, hissLevelCello) * std::pow (noteHz / 440.0, at (hissSlope, 0.0)));
        bodyNoiseNow = (float) at (bodyNoise, bodyNoiseCello);
        humRatioNow = (float) at (humRatio, humRatioCello);
        flutterDbNow = (float) at (flutterDb, flutterDbCello);
        wanderDbNow = (float) note (S::wanderDb, at (wanderDb, wanderDbCello));
        bowDriftDbNow = (float) at (bowDriftDb, bowDriftDbCello);
        rippleDbNow = (float) note (S::rippleDb, at (rippleDb, rippleDbCello));
        directDbNow = plucked ? directDbCello + directSizeDbPizz * std::log2 (span) : at (directDb, directDbCello) + directSizeDb * std::log2 (span);
        directHpNow = plucked ? directHpCello : at (directHp, directHpCello);
        tiltHzNow = plucked ? tiltHzCello : at (tiltHz, tiltHzCello);
        floorGain = plucked || toCello >= 1.0 ? 0.0f
                                              : (float) ((1.0 - toCello) * std::pow (10.0, note (S::floorDb, hallFloorDb) / 20.0)
                                                         * std::pow (noteHz / 440.0, hallFloorSlope / 6.0206));
        noiseFloorAtSize = plucked ? 0.0f : (float) (toCello * std::pow (10.0, noiseFloorDb / 20.0));
    }

    // The per-mode terms SIZE (and the note's tables) set: with the modes,
    // and redone here when SIZE moves on its own.
    void updateSizeTerms()
    {
        for (int n = 0; n < modes; ++n)
        {
            const auto hz = (double) modeHz[n];
            // (once let go, the mode loses releaseLossHz more a second at 1 kHz, by (hz / 1 kHz) ^ releaseLossSlope)
            // (bounded for the float only: ramped in before lossOf's own bound)
            // (round 9: and the cello's table's lossAdd, no less than a tenth of that; its darkDb on the
            // onset's darkening, its dropDb at the let-go)
            const auto base = juce::jmin (releaseLossAtSize * std::pow (hz / 1000.0, releaseLossSlopeAtSize), 1.0e30);
            auto lossAdd = 0.0, darkAdd = 0.0, drop = 0.0;
            for (int i = 0; i < ModalBowedNotes::tableCount; ++i)
                if (tableWeight[i] > 0.0)
                {
                    lossAdd += tableWeight[i] * (double) noteRow[i]->lossAdd[n];
                    darkAdd += tableWeight[i] * (double) noteRow[i]->darkDb[n];
                    drop += tableWeight[i] * (double) noteRow[i]->dropDb[n];
                }
            releaseLoss[n] = (float) (lossAdd != 0.0 ? juce::jmax (base + lossAdd, 0.1 * base) : base);
            onsetDark[n] = (float) (onsetDarkDbAtSize * juce::jmin (1.0, std::pow (hz / onsetDarkKneeAtSize, onsetDarkSlopeAtSize)) + darkAdd);
            releaseDrop[n] = (float) drop;
        }
        sizeTermsAt = bodySize;
    }

    // The bridge's high-pass (two poles, Butterworth): at directHp for a
    // violin, lower for a bigger box (round 8: by size ^ directHpSizeSlope
    // less than in step with it: the cello's direct path starts higher,
    // its body carries the low end), and (round 8, bowed) no lower than
    // humRatio times the note: a bowed string has nothing under its
    // fundamental, and what the model had there (the rosin's jitter leaning
    // on the modes, out through the direct path and the low resonances'
    // skirts) was a hum Spitfire's notes don't have.
    void updateHighPass()
    {
        const auto span = juce::jlimit (1.0, ModalBowedBodies::cello, fittedSize());
        const auto box = directHpNow * std::pow (span, directHpSizeSlope) / (double) bodySize;
        // (bowed, the whole bridge goes through it, so it stays under the note: at most humTop times it)
        const auto corner = plucked ? box : juce::jmax (juce::jmin (box, (double) humTop * frequency), (double) humRatioNow * frequency);
        const auto w0 = juce::MathConstants<double>::twoPi * juce::jmin (corner, sampleRate * 0.45) / sampleRate;
        const auto alpha = std::sin (w0) / juce::MathConstants<double>::sqrt2; // (Q = 1 / sqrt 2)
        const auto cw = std::cos (w0);
        const auto a0 = 1.0 + alpha;
        hpB0 = hpB2 = (float) ((1.0 + cw) * 0.5 / a0);
        hpB1 = (float) (-(1.0 + cw) / a0);
        hpA1 = (float) (-2.0 * cw / a0);
        hpA2 = (float) ((1.0 - alpha) / a0);
    }

    // (allocate: prepare only; SIZE changes just the decays, not the lines.)
    void updateRoom (bool allocate)
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
            const auto lowDecay = roomLowDecay * std::pow (fittedSize(), roomLowSizeSlope);
            const auto highDecay = roomHighDecay * std::pow (fittedSize(), roomHighSizeSlope);
            const auto feed = std::pow (10.0, -lowDecay * seconds / 20.0);
            const auto extra = juce::jmax (0.0, highDecay - lowDecay) * seconds; // (dB more per pass at 4 kHz)
            const auto c = (std::pow (10.0, extra / 10.0) - 1.0) / (2.0 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * 4000.0 / sampleRate)));
            const auto pole = c > 1.0e-12 ? ((2.0 * c + 1.0) - std::sqrt (4.0 * c + 1.0)) / (2.0 * c) : 0.0;
            roomPole[r] = (float) pole;
            roomFeed[r] = (float) (feed * (1.0 - pole));
            meanSquare += feed * feed / roomLines;
        }
        if (allocate)
            roomBuffer.assign ((size_t) total, 0.0f);   // (prepare only: about 14 KB at 48 kHz)
        roomScale = std::sqrt ((1.0 - meanSquare) / roomLines);
        updateRoomReturn();
    }

    // The room's return: roomDb for a violin, roomSizeDb more per doubling of the body's size.
    void updateRoomReturn() { roomReturn = (float) (std::pow (10.0, (roomDb + roomSizeDb * std::log2 ((double) bodySize) + roomSizeDbMore * std::log2 (fittedSize())) / 20.0) * roomScale); }

    // The bow's contact: a low string takes longer to speak (as on the Bow
    // exciter); the swell after it shapes the note's rise.
    double attackSeconds() const { return 0.05 * attackScaleNow * juce::jlimit (1.0, 2.5, 220.0 / frequency); }

    void updateBody()
    {
        bodyDirty = false;
        updateRoom (false);
        updateFit();
        if (bodySize != sizeTermsAt)
            updateSizeTerms();
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
        // (Round 8: the direct path directSizeDb down per doubling of the
        // body's size, so a cello's resonances carve its spectrum: wood, not brass.)
        bodyDirect = (float) std::pow (10.0, (directDbNow + loudest) / 20.0);   // (round 9: directDbNow has it, updateFit)
        wolfLoud = std::pow (10.0, (loudest + (double) wolfDb) / 20.0);
        updateHighPass();
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
        // WOLF's resonance: the loudest of the body's fitted resonances below
        // wolfTopHz (a violin's), at this SIZE.
        auto wolfLoudest = -1000.0;
        for (int j = 0; j < B::count && B::hz[j] <= wolfTopHz; ++j)
        {
            const auto db = (double) lower[j] + blend * ((double) upper[j] - (double) lower[j]);
            if (db > wolfLoudest)
            {
                wolfLoudest = db;
                wolfHz = (double) B::hz[j] / size;
                wolfWidth = (double) B::bandwidth[j] / size;
            }
        }
        updateRipple();
        updateWolf();
    }

    // WOLF's coupling. Two like oscillators joined by a stiffness kappa
    // split into two, kappa / omega apart (wolfSplitHz at the top), so a
    // partial on the resonance and the body trade their energy at about
    // that rate. The modes' bridge force is their slope there, (n + 1) /
    // modes (outBase); each mode's push is scaled by the same slope, against
    // a partial's at the resonance, so whichever partial lands on it beats
    // alike. Both ways have the same sign, as a spring between them: the
    // coupling only moves energy about and the body's losses take some.
    // Only the modes within an octave of the resonance feel the push
    // (fading out by a raised cosine): far above it the bridge is as good
    // as still, and all the upper modes together, each a stiff spring
    // against the bridge, would add up to a negative stiffness that pulls
    // the resonance down and, at the top, past zero (a runaway). So the loop
    // stays a few weak springs, whatever the note, SIZE or mode count.
    // WOLF also narrows the resonance (wolfNarrow).
    void updateWolf()
    {
        wolfOn = wolfAmount > 0.0f && wolfHz > 0.0 && wolfHz < sampleRate * 0.45;
        if (! wolfOn)
            return;
        // (the knob's lower half brings the coupling in, its upper half the howl)
        const auto coupled = juce::jmin (1.0, 2.0 * (double) wolfAmount);
        const auto howl = juce::jmax (0.0, 2.0 * (double) wolfAmount - 1.0);
        const auto omega = juce::MathConstants<double>::twoPi * wolfHz;
        const auto width = wolfWidth * (1.0 - (double) wolfNarrow * coupled);
        const auto r = std::exp (-juce::MathConstants<double>::pi * width / sampleRate);
        wolfTurn = (float) (2.0 * r * std::cos (omega / sampleRate));
        wolfFall = (float) (r * r);
        const auto split = juce::MathConstants<double>::twoPi * (double) wolfSplitHz * coupled;
        const auto kappa = split * omega / (sampleRate * sampleRate);
        // (a partial's slope at the resonance: wolfHz / (modes x the note))
        const auto perSlope = (double) modes * frequency / wolfHz;
        wolfDrive = (float) (kappa * perSlope);
        wolfFirst = modes;
        wolfEnd = 0;
        auto atBow = 0.0;
        for (int n = 0; n < modes; ++n)
        {
            const auto octaves = std::abs (std::log2 ((double) modeHz[n] / wolfHz));
            const auto nearWolf = octaves < 1.0 ? 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * octaves) : 0.0;
            // (the mode's slope at the bridge, (n + 1) / modes: without the per-note tables' gains)
            wolfPush[n] = (float) (kappa * perSlope * (double) (float) ((double) (n + 1) / (double) modes) * nearWolf);
            if (nearWolf > 0.0)
            {
                wolfFirst = juce::jmin (wolfFirst, n);
                wolfEnd = n + 1;
            }
            atBow += (double) shape[n] * (double) wolfPush[n];
        }
        wolfHold = (int) (wolfHoldSeconds * sampleRate);
        wolfRest = (int) (wolfRestSeconds * sampleRate);
        wolfFloor = 1.0f - (float) howl;
        wolfPress = (float) howl;
        wolfTrip = wolfHowlRatio * (float) std::pow ((double) (wolfHowlRatioTop / wolfHowlRatio), howl);
        wolfGripStep = (float) (1.0 - std::exp (-1.0 / (wolfGripSeconds * sampleRate)));
        wolfRingStep = (float) (1.0 - std::exp (-1.0 / (wolfRingSeconds * sampleRate)));
        wolfShape = (float) atBow;   // (the push as the bow sees it)
        // Its sound (the howl): at its peak, on its own, wolfDb over the
        // body's loudest resonance at the top, less as WOLF comes down.
        wolfRadiate = (float) (coupled * std::pow (10.0, (double) wolfHowlDb * howl / 20.0) * wolfLoud * (1.0 - r * r) * std::sin (omega / sampleRate)
                               / (2.0 * std::sin (0.5 * omega / sampleRate) * juce::jmax (1.0e-12, (double) wolfDrive)));
    }

    double sampleRate = 44100.0, frequency = 220.0;
    int modes = 1, bodyCount = 0, untilBlock = 0;
    long long elapsed = 0, releasedAt = -1;
    bool silent = true, modesDirty = true, bodyDirty = true, plucked = false, ecoOn = false;
    float loss0 = 0.8f, loss1 = 3.0e-5f, inharmonic = 1.0e-5f, bowAt = 0.11f;
    float grip = 0.5f, bowSpeed = 0.2f, damperAmount = 0.0f, velocityLevel = 0.5f;
    float bodySize = 1.0f, stringScale = 1.0f, sizeTermsAt = 1.0f;
    float envelope = 0.0f, attackCoefficient = 0.001f, releaseCoefficient = 0.001f;
    float biteEnvelope = 1.0f, biteFall = 0.999f, level = 1.0f;
    float halfRate = 22050.0f, forceScale = 1.0f, pluckScale = 1.0f, peakBridge = 0.0f, peakBody = 0.0f;
    float swell = 0.0f, swellCoefficient = 0.0001f, swellOnCoefficient = 0.0001f, hairNoise = 0.0f, hissState = 0.0f, hissLast = 0.0f, hissPole = 0.35f, tilt = 0.0f, tiltPole = 0.6f;
    float vibratoCents = 9.0f, vibratoPhase = 0.0f;
    float wanderPole = 0.01f, wanderGain = 0.0f, bowDriftGain = 0.0f, vibratoPole = 0.01f, rateDriftGain = 0.0f, depthDriftGain = 0.0f;
    float bowDrift = 0.0f, rateDrift = 0.0f, depthDrift = 0.0f, bowDriftIn = 0.0f, rateDriftIn = 0.0f, depthDriftIn = 0.0f;
    float bowWander = 1.0f, bodyDirect = 0.0f, liftSpeed = 0.0f;
    // Round 8: the stroke's second stage, the jitter's share against the
    // stroke, the let-go's gain and its step, the noise kept under the tail.
    float swellSecond = 0.0f, jitterRatio = 1.0f, letGo = 1.0f, letGoStep = 0.0f, tailNoiseGain = 1.0f;
    // (the fitted terms at this SIZE: updateSizeTerms)
    float swellDepthAtSize = 0.9163f, swellSecondAtSize = 0.0f, noiseSwellAtSize = 0.265f, releaseFadeAtSize = 7.0f;
    double releaseRingAtSize = 8.6, releaseRampPower = 1.0, letGoDropAtSize = 0.0, tailDbAtSize = 0.0, tailNoiseAtSize = 0.0;
    double onsetDarkSecondsAtSize = 0.158, vibratoAtSize = 1.0;
    double releaseLossAtSize = 0.0, releaseLossSlopeAtSize = 3.2, onsetDarkDbAtSize = 0.0, onsetDarkSlopeAtSize = 2.9, onsetDarkKneeAtSize = 2200.0;
    // Round 9 (updateFit): the string, noise, wander and box at this SIZE and
    // note; the note's tables (a row and its weight per table, picked at
    // note-on; the weight at this SIZE); the violin's hall floor (its level,
    // pink noise, filters and envelope) and the cello's floor under the bow's
    // noise once let go and each partial's drop then.
    double brightnessNow = 2.0, stringLossNow = 3.9e-6, attackScaleNow = 0.05, hissHpNow = 8000.0;
    double directDbNow = -10.0, directHpNow = 96.0, tiltHzNow = 5590.0, noteHz = 220.0;
    float hissNow = 0.0f, bodyNoiseNow = 0.0f, humRatioNow = 0.33f, flutterDbNow = 21.0f, wanderDbNow = 2.0f, bowDriftDbNow = 1.7f, rippleDbNow = 3.3f;
    float vibratoNote = 1.0f;
    const ModalBowedNotes::Row* noteRow[ModalBowedNotes::tableCount] {};
    double noteWeight[ModalBowedNotes::tableCount] {}, tableWeight[ModalBowedNotes::tableCount] {};
    std::uint32_t floorSeed = 1;
    float floorGain = 0.0f, floorLowPole = 0.0f, floorHighPole = 0.0f, floorRise = 0.0f;
    float pink0 = 0.0f, pink1 = 0.0f, pink2 = 0.0f, floorLow = 0.0f, floorHigh = 0.0f, floorLast = 0.0f, floorEnvelope = 0.0f, floorStep = 0.0f;
    float noiseFloorAtSize = 0.0f, noiseHeld = 0.0f, noiseFloorNow = 0.0f, dropNow = 0.0f;
    float flutterPole = 0.1f, flutterGain = 0.0f, pitchPole = 0.01f, pitchGain = 0.0f, pitchDrift = 0.0f, pitchDriftIn = 0.0f;
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f, hx1 = 0.0f, hx2 = 0.0f, hy1 = 0.0f, hy2 = 0.0f;
    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f, lx1 = 0.0f, lx2 = 0.0f, ly1 = 0.0f, ly2 = 0.0f;
    double ringUpper = 1.0;   // (the release's loss factor the modes have now)
    // WOLF: the knob, its resonance (Hz, bandwidth) at this SIZE, the
    // resonator's coefficients and state (the bridge's motion).
    float wolfAmount = 0.0f, wolfTurn = 0.0f, wolfFall = 0.0f, wolfDrive = 0.0f, wolfMove = 0.0f, wolfMoveLast = 0.0f;
    double wolfHz = 0.0, wolfWidth = 1.0, wolfLoud = 0.0;
    float wolfShape = 0.0f, wolfRadiate = 0.0f, wolfRing = 0.0f, wolfFed = 0.0f, wolfGrip = 1.0f, wolfFloor = 1.0f, wolfGripStep = 0.01f, wolfRingStep = 0.001f;
    bool wolfLost = false;
    float wolfPress = 0.0f, wolfTrip = 5.0e7f;
    int wolfClock = 0, wolfHold = 0, wolfRest = 0;
    int wolfFirst = 0, wolfEnd = 0;
    bool wolfOn = false, wolfActive = false;
    std::uint32_t seed = 1, driftSeed = 1, flutterSeed = 1;
    // Fitted to the Spitfire long notes (2026-11-08, see the top).
    // The bow's onset (round 6): the stroke starts at 1 - swellDepth of its
    // speed and swells over swellSeconds (a bow landing slower stuck to the
    // string for ~30 ms before it spoke); the upper partials come in darker
    // (onsetDarkDb at onsetDarkKnee and over, onsetDarkSlope below it) and
    // brighten over onsetDarkSeconds. Round 7, cello only (both do nothing
    // at SIZE 1): on a bigger body the darkening reaches lower (the knee at
    // onsetDarkKnee / size ^ onsetDarkKneeSize) and the bow's swell builds
    // size ^ swellSize slower while the note is held.
    // Round 8: the violin's onset refitted, with a second swell stage
    // (swellSecondStage of the way from one stage to two) and the hair's
    // noise and the rosin's jitter on a shallower swell (noiseSwellDepth).
    // The ...Cello values are round 7's, which the cello keeps (no second
    // stage, the noise on the stroke's own swell): SIZE blends between the
    // two. On a bigger body the darkening also lasts longer (size ^
    // onsetDarkSecondsSizeSlope).
    static constexpr double swellSeconds = 0.20311, swellSecondsCello = 0.3544, swellSize = 0.25;
    static constexpr double swellDepth = 0.91629, swellDepthCello = 0.8208, swellSecondStage = 0.67352, noiseSwellDepth = 0.26398;
    static constexpr float onsetDarkKneeSize = 1.56f;
    static constexpr double onsetDarkDb = 17.49, onsetDarkKnee = 2689.6, onsetDarkSlope = 2.9034;
    static constexpr double onsetDarkDbCello = 15.04, onsetDarkKneeCello = 1990.0, onsetDarkSlopeCello = 2.82;
    static constexpr double onsetDarkSeconds = 0.15823, onsetDarkSecondsCello = 0.1429, onsetDarkSecondsSizeSlope = 0.96;
    // Round 8: the bowed bridge's high-pass is no lower than humRatio times
    // the note. The cello's wood (by SIZE, nothing at 1): the direct path
    // directSizeDb down per doubling, its high-pass size ^
    // directHpSizeSlope higher, the vibrato size ^ vibratoSizeSlope as deep.
    static constexpr float humRatio = 0.37266f, humTop = 0.7f;
    static constexpr double directSizeDb = -7.6977, directHpSizeSlope = 0.87, vibratoSizeSlope = -0.098769;
    static constexpr float rosinJitter = 0.08536f, hissLevel = 1.7936e-04f, bodyNoise = 1.7985e-06f;
    static constexpr float brightness = 1.7841f, topHz = 12000.0f, tiltHz = 5701.8f, directDb = -12.074f;
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
    static constexpr float flutterKnee = 818.2f, flutterFreqSlope = 1.285f, rippleKnee = 963.9f, rippleFreqSlope = 2.074f, directHp = 57.747f, directLp = 20000.0f;
    static constexpr float flutterClip = 30.0f;
    static constexpr double vibratoRate = 5.2, vibratoDelay = 0.25, vibratoRise = 0.4;
    // The bow's release: its pressure eases off over releaseSeconds as it
    // lifts, the upper partials come to lose releaseRing times more over
    // releaseRingSeconds, and the note fades at releaseFadeDb a second
    // (Spitfire's notes once let go). Round 8: the violin's refitted to each
    // partial's fall, with the let-go's two stages (nextBlock: letGoDropDb
    // over letGoDropSeconds, then tailDb more from tailDelay over
    // tailSeconds, tailNoise of it kept off the hair's noise); the
    // ...Cello values are round 7's (no stages), SIZE blending between.
    static constexpr double releaseSeconds = 0.30149, releaseSecondsCello = 1.0;
    static constexpr double releaseFadeDb = 22.334, releaseFadeDbCello = 35.79, releaseRing = 9.3783, releaseRingCello = 17.84;
    static constexpr double releaseRingSeconds = 0.4991;
    static constexpr double letGoDropDb = 3.092, letGoDropSeconds = 0.02, tailDb = 22.875, tailSeconds = 0.50944, tailDelay = 0.07799, tailNoise = 0.34346;
    // Once let go, each mode also loses releaseLossHz more a second at
    // 1 kHz, by (hz / 1 kHz) ^ releaseLossSlope (ramped in with the ring).
    static constexpr double releaseLossHz = 1.2938, releaseLossHzCello = 23.94;
    static constexpr float releaseLossSlope = 3.2245f;
    // Round 9. The violin's retuned terms (brightness, stringLossHigh,
    // directDb, directHp, tiltHz, hissLevel, bodyNoise, humRatio and the
    // ones above) blend to these round 8 values at the cello's size, and the
    // cello's new values (hiss, flutter, wander, bow drift, ripple) in from
    // the violin's. The pizzicato keeps round 8's box (directDbCello,
    // directSizeDbPizz, directHpCello, tiltHzCello). The bow's contact takes
    // attackScale of 50 ms (more low down); the hiss is high-passed at hissHp
    // and grows as (note / 440 Hz) ^ hissSlope (the violin's).
    static constexpr float stringLossHigh = 3.2440e-06f;
    static constexpr double brightnessCello = 2.144, stringLossHighCello = 3.9470e-06, directDbCello = -10.33, directHpCello = 96.61, tiltHzCello = 5590.0;
    static constexpr double bodyNoiseCello = 1.6950e-06, humRatioCello = 0.33, attackScale = 0.050102, attackScaleCello = 0.05;
    static constexpr double hissHp = 9836.2, hissHpCello = 8000.0, hissSlope = 0.77496, hissLevelCello = 0.0022;
    static constexpr double flutterDbCello = 14.316, wanderDbCello = 1.5021, bowDriftDbCello = 0.87783, rippleDbCello = 2.9949;
    static constexpr double directSizeDbPizz = -12.0;
    // The violin's hall floor: a pink noise hallFloorDb against the note at
    // A4 (hallFloorSlope dB more per doubling of the note), low-passed
    // (one pole) at hallFloorLowHz and high-passed at hallFloorHighHz, with
    // hallFloorWhite of its white noise; it rises over hallFloorRise from
    // the note-on, steps up hallFloorLetGoDb over hallFloorLetGoSeconds as
    // the bow leaves and fades hallFloorFade dB a second. Bowed only.
    static constexpr double hallFloorDb = -55.793, hallFloorSlope = 4.9665, hallFloorLowHz = 1874.2, hallFloorHighHz = 226.92, hallFloorRise = 0.0035533;
    static constexpr double hallFloorLetGoDb = 2.2157, hallFloorLetGoSeconds = 0.05, hallFloorFade = 23.941, hallFloorWhite = 0.30072;
    // The cello's: once let go the bow's noise stays at least noiseFloorDb
    // under its level at the let-go, that floor fading noiseFloorFade dB a
    // second; its table's partial drops take partialDropSeconds.
    static constexpr double noiseFloorDb = -11.7, noiseFloorFade = 20.0, partialDropSeconds = 0.03;
    // The vibrato (cents) the per-note tables' vibratoCents are against: the
    // knob's vibrato scales by their ratio.
    static constexpr double vibratoFitted = 14.65;
    // Round 7, cello only (nothing at SIZE 1): a bigger body's released
    // partials lose more (releaseLossHz times size ^ releaseLossSizeSlope), and
    // its room is louder (roomSizeDbMore more per doubling) but shorter, its decays
    // times size ^ roomLowSizeSlope and size ^ roomHighSizeSlope.
    // Round 8, cello only: its low partials ring on once let go (the ring's
    // excess over 1 times size ^ releaseRingSizeSlope) while the upper ones
    // die steeply (the loss's slope times size ^ releaseLossSlopeSizeSlope)
    // and late (ramped in as time ^ (size ^ releaseRampSizeSlope)), and the
    // fade is size ^ releaseFadeSizeSlope quicker.
    static constexpr float releaseLossSizeSlope = 11.925f;
    static constexpr double releaseRingSizeSlope = -2.297, releaseLossSlopeSizeSlope = 1.053, releaseRampSizeSlope = 1.957, releaseFadeSizeSlope = 0.2405;
    static constexpr double roomLowSizeSlope = 1.086, roomHighSizeSlope = 0.19131;
    // The room (fitted to the tails of Spitfire's long notes).
    static constexpr int roomLines = 4;
    static constexpr double roomDb = -2.5, roomSizeDb = -18.864, roomSizeDbMore = 14.736, roomMs = 18.44, roomLowDecay = 38.88, roomHighDecay = 159.5;
    static constexpr float decibel = 0.1151292546f, goldenRatio = 1.618034f;
    // WOLF at the top: the split (Hz) of a partial sitting on the
    // resonance, how much narrower the resonance rings, the highest
    // resonance (a violin's Hz) it picks from, the bridge's bound (far past
    // any bowed swing) and the resonance's sound (dB, see updateWolf).
    static constexpr float wolfSplitHz = 6.0f, wolfNarrow = 0.5f, wolfTopHz = 700.0f, wolfLimit = 0.05f, wolfDb = 3.0f;
    // The howl (WOLF's upper half): the resonance ringing wolfHowlRatio
    // over what the bridge feeds it (down to wolfHowlRatioTop at the top:
    // more notes around the wolf trip it) makes the bow lose the string for
    // wolfHoldSeconds (its speed drops to nothing within wolfGripSeconds and
    // it digs in, braking the string), then it bites again and can't lose
    // it for wolfRestSeconds; the resonance is wolfHowlDb louder at the top.
    static constexpr float wolfHowlRatio = 5.0e7f, wolfHowlRatioTop = 5.0e6f, wolfHowlDb = 8.0f;
    static constexpr double wolfHoldSeconds = 0.12, wolfRestSeconds = 0.08, wolfGripSeconds = 0.005, wolfRingSeconds = 0.03;
    // The output's level: the string's units are tiny (metres of string,
    // narrow resonators); this brings a violin D4 near a plucked string's.
    static constexpr float outputGain = 30000.0f;

    alignas (32) float q[maxModes] {}, qp[maxModes] {}, turn[maxModes] {}, fall[maxModes] {};
    alignas (32) float shape[maxModes] {}, push[maxModes] {}, out[maxModes] {}, outBase[maxModes] {}, modeHz[maxModes] {}, lossUpper[maxModes] {}, lossOf[maxModes] {}, decayNow[maxModes] {};
    alignas (32) float flutter[maxModes] {}, flutterIn[maxModes] {}, flutterShape[maxModes] {}, onsetDark[maxModes] {};
    alignas (32) float wander[maxModes] {}, wanderIn[maxModes] {}, outStep[maxModes] {}, rippleSin[maxModes] {}, rippleCos[maxModes] {}, rippleSin2[maxModes] {}, rippleCos2[maxModes] {};
    alignas (32) float bodyB0[maxBody] {}, bodyA1[maxBody] {}, bodyA2[maxBody] {}, bodyGain[maxBody] {};
    alignas (32) float bx1[maxBody] {}, bx2[maxBody] {}, by1[maxBody] {}, by2[maxBody] {};
    alignas (32) float releaseLoss[maxModes] {}, wolfPush[maxModes] {}, releaseDrop[maxModes] {};
    std::vector<float> roomBuffer;   // (the four lines end to end)
    alignas (16) float roomLow[roomLines] {}, roomFeed[roomLines] {}, roomPole[roomLines] {};
    int roomLength[roomLines] {}, roomStart[roomLines] {}, roomAt[roomLines] {};
    float roomReturn = 0.0f;
    double roomScale = 0.0;
};
