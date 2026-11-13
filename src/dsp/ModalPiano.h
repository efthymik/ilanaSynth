#pragma once

// The Modal Piano exciter: a grand piano rebuilt from its partials.
//
// Where the Piano exciter (PianoString) models the hammer and string, this
// one plays back what a reference grand measured: for each recorded note and
// velocity, every partial's frequency and its level over time
// (ModalPianoTable, from tools/build_modal_piano.py). A note between two
// recorded ones blends their two nearest notes and velocity layers. Each
// partial is a sine (a complex rotator; eight at a time with AVX, else
// four with SSE) whose level follows the measured curve at a block rate.
// Partials that have gone silent, or sit 90 dB under the loudest one, stop
// running in groups of eight. The first few tens of
// milliseconds are the recording's own attack (hammer thump and all),
// resampled to the note's pitch, crossfading into the partials (the
// piano's; the guitar's pluck is modelled, below).
//
// In stereo, as the reference's microphones heard it: each partial and board
// resonance has its own balance and phase between left and right (steady
// over a note, so measured once per note), the attack is the recording's two
// channels, and the noise is two uncorrelated draws (the guitar's is one,
// round 4: its reference is dual mono). process() returns the middle,
// (left + right) / 2, and side() the rest, (left - right) / 2.
//
// The knobs move away from the recording, which they reproduce at their
// middle (DECAY, STIFF, HAMMER) or at 0 (DAMP):
//   DECAY   stretches or shortens the whole decay (x0.35 .. x2.8)
//   DAMP    makes the upper partials die away faster
//   STIFF   0 is a harmonic string, the middle the measured stretch, 1 twice it
//   HAMMER  plays the timbre of a softer or harder strike at the same loudness
//   DAMPER  how fast the dampers stop the note on release (0 rings on)
// Eco keeps the first half of the partials.
//
// The same engine plays the Modal Guitar (a nylon-string guitar's table,
// setTable), which plays no recording: its pluck is modelled (2026-11-12,
// table version 5, tools/build_modal_guitar.py). Its table starts with four
// early times (0 to 35 ms) whose levels were fitted to the pluck, so the
// upper partials and the body's boom start loud and fall fast; each partial
// and body mode starts in the phase it had in the nearest recorded note at
// the nearest velocity (not blended: two phases blended would cancel);
// the note swells along one raised cosine from its start to the top of the
// pluck (the pluck's measured start, a few ms in as the times count, and
// its rise, 0.5 to 4 ms; the one thing it adds per sample); and the noise
// bands above 500 Hz get a burst (the pluck's own noise, measured over the
// steady noise) that falls block by block.
//
// Two shape knobs move either instrument away from the recording, which
// they reproduce at 1 (setShape):
//   SIZE    the body (soundboard, guitar box), physically (2026-11-13):
//           real near x1, a smooth "what if" out to x0.25 and x4, soft
//           limits, nothing new per sample (setBodyLaws, moveMode,
//           bodyShapeDb). Each table mode is tagged at parse time
//           (ModalPianoTable::ModeKind). Guitar: air (A0 102.5 Hz) and top
//           (T(1,1)2 240 Hz) move as a coupled pair (Helmholtz SIZE^-1.25,
//           top SIZE^-1.5), the rest of the plate SIZE^-1.5; the partials
//           follow the moved resonances and the bass corner under A0 (and
//           the averaged tone, as the plate, above 1.5 kHz), drain into
//           moved modes, and the pluck rises slower and clicks softer on a
//           bigger box. Grand: board modes move SIZE^-p (p 2 under 200 Hz
//           to 0.5 from 1.2 kHz; the bass strings' longitudinal modes stay),
//           the averaged tone moves with them, the bass radiation corner
//           (55 Hz) as SIZE^-2, and the board's thump noise on the board's
//           time. Both: modes keep constant Q (moved by r, they ring 1/r as
//           long, after the dampers too), fade out under 30 Hz, and past
//           x1.4 free slots fill with infill modes (density ~ area).
//           Exactly the recording at x1 (every term skipped). Past x2 it
//           leaves physics (the giant body, addGiant; x1-x2 bit for bit
//           as before): up to 24 extra slots crossfade in by x4, body
//           modes through the strings' range fed by the note's spectrum
//           near each, and a halo of the note's lowest partials a few
//           cents off; each blooms, rings long (T60 rising with SIZE) and
//           rings on after key-up, the low end cut, the dry note ducked
//           so the level holds. The guitar is a giant hollow box, nearly
//           a gong (one set of modes for every note, upper ones swelling
//           later); the grand a piano on a huge plate (each voice its own
//           plate modes to 6 kHz, a fast bloom, a stronger halo).
//   SCALE   the string's length at the same pitch: longer strings are less
//           stiff (the stretch falls as SCALE^-3) and ring longer (x SCALE)

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <complex>
#include <cmath>

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_MODAL_SSE 1
 #include <xmmintrin.h>
 #include <immintrin.h>
 // The AVX path is compiled for AVX on its own and picked at run time (the
 // build targets SSE2), as the WEST folder's is.
 #if defined(__GNUC__) || defined(__clang__)
  #define ILANA_MODAL_AVX_TARGET __attribute__ ((target ("avx")))
 #else
  #define ILANA_MODAL_AVX_TARGET
 #endif
#else
 #define ILANA_MODAL_SSE 0
#endif

#include "ModalPianoTable.h"

class ModalPiano
{
public:
    static constexpr int maxPartials = 100;
    static constexpr int maxModes = 16;     // the board's resonances
    static constexpr int maxGiant = 32;     // SIZE past x2: the giant body's 24 modes and up to 7 silent ones (addGiant)
    static constexpr int group = 8;         // slots run and skipped together (one AVX register, two SSE)
    static constexpr int maxSlots = (maxPartials + maxModes + maxGiant + group - 1) / group * group;
    static constexpr int blockSize = 32;
    // Tests and benches turn it off to compare with every partial running
    // (CPU round 2026-10-31): skipping groups of silent partials, and
    // partials 90 dB under the loudest one (relativeCut).
    inline static bool skipSilentGroups = true;
    inline static float relativeCut = 90.0f;
    // ILANA_NO_AVX (or a CPU without it) keeps the SSE path.
    static bool& useAvx()
    {
       #if ILANA_MODAL_SSE
        static bool use = juce::SystemStats::hasAVX()
                          && juce::SystemStats::getEnvironmentVariable ("ILANA_NO_AVX", "").isEmpty();
       #else
        static bool use = false;
       #endif
        return use;
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        useAvx(); // (its first call reads the environment: here, not on the audio thread)
        ModalPianoTable::guitar(); // (parsed here too, not on the audio thread)
        designNoise();
        reset();
    }

    void reset()
    {
        count = slots = modeSlots = giantStart = 0;
        attackLeft = 0;
        pluckAt = pluckLength = 0;
        burstLive = false;
        silent = true;
    }

    void setNote (double hz, int midiNote)
    {
        if (hz == frequency && midiNote == note)
            return;
        frequency = juce::jlimit (8.0, sampleRate * 0.45, hz);
        note = midiNote;
        pitchDirty = true;
    }

    // Which instrument's measurements play (the grand by default).
    void setTable (const ModalPianoTable& newTable)
    {
        if (&newTable == tableData)
            return;
        tableData = &newTable;
        if (sized)
            setBodyLaws();
        reset();
    }

    // SIZE and SCALE as factors (1: the recording).
    void setShape (float newSize, float newScale)
    {
        newSize = juce::jlimit (0.1f, 10.0f, newSize);
        newScale = juce::jlimit (0.1f, 10.0f, newScale);
        if (newSize == bodySize && newScale == stringScale)
            return;
        bodySize = newSize;
        stringScale = newScale;
        shapeDbDirty = true;
        sized = newSize != 1.0f;
        if (sized)
            setBodyLaws();
        else
            giant = 0.0f;
        stretchScale = 1.0f / (newScale * newScale * newScale);
        pitchDirty = true;
    }

    void setParams (float newDecay, float newDamp, float newStiffness, float newHardness, float newDamper, bool newEco)
    {
        timeScale = std::exp2 ((juce::jlimit (0.0f, 1.0f, newDecay) - 0.5f) * 3.0f);
        const auto damp = juce::jlimit (0.0f, 1.0f, newDamp);
        dampRate = 40.0f * damp * damp;
        if (const auto stretch = 2.0f * juce::jlimit (0.0f, 1.0f, newStiffness); stretch != stiffness)
        {
            stiffness = stretch;
            pitchDirty = true;
        }
        hardness = juce::jlimit (0.0f, 1.0f, newHardness);
        damper = juce::jlimit (0.0f, 1.0f, newDamper);
        eco = newEco;
    }

    void trigger (float velocity, float randomUnit)
    {
        const auto& table = *tableData;
        count = slots = giantStart = 0;
        attackLeft = 0;
        pluckAt = pluckLength = 0;
        burstLive = pluckNoise = false;
        silent = true;
        if (! table.valid())
            return;

        const auto played = 127.0f * juce::jlimit (0.0f, 1.0f, velocity);
        // HAMMER: the timbre of a softer or harder strike (up to 50 velocity
        // steps either way), brought back to this velocity's loudness.
        const auto timbre = played + (hardness - 0.5f) * 100.0f;
        Corners at, loudness;
        findCorners (table, note, timbre, at);
        findCorners (table, note, played, loudness);
        auto gainDb = energyDb (table, loudness) - energyDb (table, at);
        // Below the softest recorded layer, quieter still.
        const auto softest = (float) table.velocities.front();
        if (played < softest)
            gainDb += 30.0f * std::log10 (juce::jmax (1.0f, played) / softest);

        // The partials: frequency ratios blended in log, levels in dB.
        auto partials = 0;
        for (const auto& c : at.entries)
            partials = juce::jmax (partials, c->partials);
        partials = juce::jmin (partials, eco ? maxPartials / 2 : maxPartials);
        const auto top = juce::jmin (16000.0, sampleRate * 0.45);
        auto seed = (std::uint32_t) (randomUnit * 4294967040.0f) | 1u;

        for (int k = 0; k < partials; ++k)
        {
            auto logRatio = 0.0f;
            for (int c = 0; c < 4; ++c)
                logRatio += at.weights[c] * std::log (ratioOf (*at.entries[c], k));
            const auto ratio = std::exp (logRatio);
            if (ratio * frequency > top)
                break;
            measured[k] = ratio;
            harmonic[k] = (float) (k + 1);
            // Random start phases: a real strike's partials don't line up.
            seed = seed * 1664525u + 22695477u;
            const auto phase = (float) (seed >> 8) * (juce::MathConstants<float>::twoPi / 16777216.0f);
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = 0.0f;
            count = k + 1;
        }
        corners = at;
        levelOffset = gainDb;

        // The nearest recorded note gives the attack and the board's
        // resonances (fixed in Hz, they don't follow the key), at the two
        // velocity layers round the timbre.
        const auto nearest = at.noteWeight < 0.5f ? at.noteIndex : at.noteIndex + (at.noteWeight > 0.0f ? 1 : 0);
        modeA = &table.at (nearest, at.velocityIndex);
        modeB = &table.at (nearest, juce::jmin (at.velocityIndex + 1, (int) table.velocities.size() - 1));
        slots = count;
        const auto modes = juce::jmin ((int) modeA->modes.size(), (int) modeB->modes.size(), maxModes);
        for (int m = 0; m < modes; ++m)
        {
            const auto k = slots++;
            modeHz[m] = modeA->modes[(size_t) m] / bodySize;
            seed = seed * 1664525u + 22695477u;
            const auto phase = (float) (seed >> 8) * (juce::MathConstants<float>::twoPi / 16777216.0f);
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = 0.0f;
        }
        modeSlots = slots;
        if (sized)
            addInfill (modes, seed);
        giantStart = slots;
        if (sized && logSize > 1.0f)
            addGiant (seed);
        for (int k = slots; k < roundedCount(); ++k)
        {
            measured[k] = harmonic[k] = re[k] = im[k] = amp[k] = 0.0f;
            rotCos[k] = 1.0f;
            rotSin[k] = 0.0f;
            turnCos[k] = turnSin[k] = 0.0f;
            blockTurn[k] = 0.0;
        }
        updatePitch();
        if (slots > giantStart)
            coupleGiant();

        // The modelled pluck (the guitar): each partial and mode starts in
        // the phase it had in the nearest recorded note at the nearest
        // velocity (phases are not blended: between two they would cancel),
        // a sample early, as process() turns it before it sounds.
        const auto* plucked = table.modelledPluck
                                  ? &table.at (nearest, at.velocityWeight < 0.5f ? at.velocityIndex
                                                                                 : juce::jmin (at.velocityIndex + 1, (int) table.velocities.size() - 1))
                                  : nullptr;
        if (plucked != nullptr)
            for (int k = 0; k < slots; ++k)
            {
                if (k >= modeSlots)
                    break; // (infill modes: random)
                const auto& phases = k < count ? plucked->startPhase : plucked->modeStartPhase;
                const auto i = (size_t) (k < count ? k : k - count);
                if (i >= phases.size() || phases[i] == ModalPianoTable::noPhase)
                    continue;
                const auto phase = phases[i] - std::atan2 (rotSin[k], rotCos[k]);
                re[k] = std::cos (phase);
                im[k] = std::sin (phase);
            }

        // The stereo picture: the nearest note's, per partial and mode.
        for (int k = 0; k < roundedCount(); ++k)
        {
            auto balance = 0.0f, phase = 0.0f;
            if (k < count && k < (int) modeA->balance.size())
            {
                balance = modeA->balance[(size_t) k];
                phase = modeA->phase[(size_t) k];
            }
            else if (k >= count && k < modeSlots)
            {
                balance = modeA->modeBalance[(size_t) (k - count)];
                phase = modeA->modePhase[(size_t) (k - count)];
            }
            // Each channel's gain, at the two channels' power average of 1.
            if (k >= modeSlots && k < giantStart)
            {
                // (An infill mode: its lower neighbour's.)
                const auto parent = (size_t) (infillFrom[k - modeSlots] - count);
                balance = modeA->modeBalance[parent];
                phase = modeA->modePhase[parent];
            }
            if (k >= giantStart && k < slots)
            {
                // (A giant body mode: its own place, wide.)
                balance = 10.0f * (hash01 (k - giantStart, 3) - 0.5f);
                phase = 2.0f * (hash01 (k - giantStart, 4) - 0.5f);
            }
            const auto ratio = std::pow (10.0f, balance / 20.0f);
            const auto right = std::sqrt (2.0f / (1.0f + ratio * ratio));
            gainLeft[k] = ratio * right;
            rightSin[k] = right * std::cos (phase);
            rightCos[k] = right * std::sin (phase);
        }

        // The attack, resampled to this note's pitch.
        pluckNoise = plucked != nullptr;
        if (plucked != nullptr)
        {
            // None (the guitar): one raised cosine from the note's start to
            // the top of its pluck (the pluck's start, as the times count,
            // and its rise), and its noise burst over the steady noise.
            attackLeft = 0;
            attackLength = 1;
            fadeStart = 0;
            partialRamp = 1.0f; // (nextBlock keeps it there: the crossfade is over)
            pluckAt = 0;
            // (SIZE: a bigger top gives way slower, the rise x SIZE^0.5.)
            const auto riseMs = sized ? plucked->riseMs * std::sqrt (bodySize) : plucked->riseMs;
            pluckLength = juce::jmax (1, juce::roundToInt (juce::jmax (0.0f, plucked->pluckMs + riseMs) * 0.001 * sampleRate));
            const auto toVariance = (float) ModalPianoTable::noiseFftSize / 6.0f * (float) (sampleRate / table.rate) / 0.6931472f;
            for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
            {
                auto db = 0.0f, fall = 0.0f;
                for (int i = 0; i < 4; ++i)
                {
                    const auto& e = *at.entries[(size_t) i];
                    const auto has = (size_t) b < e.burstDb.size();
                    db += at.weights[(size_t) i] * (has ? e.burstDb[(size_t) b] : -160.0f);
                    fall += at.weights[(size_t) i] * (has ? e.burstFall[(size_t) b] : 0.0f);
                }
                db += levelOffset;
                if (sized)
                    db -= 2.0f * logSize; // (SIZE: a heavier top, a softer click)
                burstVariance[b] = db < -175.0f ? 0.0f : std::pow (10.0f, db / 10.0f) * toVariance;
                burstFactor[b] = std::pow (10.0f, -juce::jmax (0.0f, fall) * (float) (blockSize / sampleRate) / 10.0f);
                burstLive = burstLive || burstVariance[b] > 0.0f;
            }
        }
        else
        {
            attackA = &modeA->attack;
            attackB = &modeB->attack;
            attackRightA = &modeA->attackRight;
            attackRightB = &modeB->attackRight;
            attackMix = at.velocityWeight;
            attackGain = std::pow (10.0f, gainDb / 20.0f);
            if (table.nearestAttack && modeA != modeB)
            {
                const auto blendedDb = modeA->attackDb + attackMix * (modeB->attackDb - modeA->attackDb);
                const auto* use = attackMix < 0.5f ? modeA : modeB;
                attackA = attackB = &use->attack;
                attackRightA = attackRightB = &use->attackRight;
                attackGain *= std::pow (10.0f, juce::jlimit (-24.0f, 24.0f, blendedDb - use->attackDb) / 20.0f);
                attackMix = 0.0f;
            }
            const auto anchorHz = 440.0 * std::exp2 ((table.notes[(size_t) nearest] - 69) / 12.0);
            attackStep = frequency / anchorHz * table.rate / sampleRate;
            const auto usable = juce::jmin ((double) std::min ({ attackA->size(), attackB->size(), attackRightA->size(), attackRightB->size() }) - 2.0,
                                            0.08 * table.rate);
            attackLength = juce::jmax (1, (int) (usable / attackStep));
            fadeStart = juce::jmin ((int) (0.025 * sampleRate), attackLength / 3);
            attackLeft = attackLength;
            attackPosition = 0.0;
            attackFade = 1.0f;
            partialRamp = 0.0f;
        }
        hasNoise = ! modeA->noiseLevels.empty();
        for (auto& band : noise)
        {
            band.left = band.right = {};
            band.gain = band.step = 0.0f;
        }
        noisePower.fill (0.0f);
        noiseSeed = seed;
        noiseSeedRight = seed * 2654435761u + 12345u;
        lastSide = 0.0f;

        elapsed = 0;
        released = -1;
        segment = -1;
        loudestPartial = -200.0f;
        std::fill (std::begin (groupLive), std::end (groupLive), false);
        std::fill (std::begin (missedTurn), std::end (missedTurn), 0.0);
        untilBlock = 0;
        silent = slots == 0 && attackLeft == 0;
    }

    // The middle of the two channels; side() is the rest.
    float process (bool noteHeld)
    {
        if (silent)
        {
            lastSide = 0.0f;
            return 0.0f;
        }
        if (! noteHeld && released < 0)
            released = elapsed;
        if (untilBlock == 0)
            nextBlock();
        --untilBlock;
        ++elapsed;

        // Each partial: sin on the left; on the right, the same sine shifted
        // by its phase (sin (x + p) = sin x cos p + cos x sin p). The
        // rotator's length is the partial's level: its turn (turnCos/Sin)
        // also grows or shrinks it to the next block's level, so no
        // separate level ramp runs per sample (Mathews & Smith's phasor
        // filter, SMAC 2003).
        auto left = 0.0f, right = 0.0f;
       #if ILANA_MODAL_SSE
        if (useAvx())
            sumPartialsAvx (left, right);
        else
        {
            auto sumLeft = _mm_setzero_ps(), sumRight = _mm_setzero_ps();
            for (int g = 0; g < liveGroups; ++g)
                for (int k = liveGroup[g]; k < liveGroup[g] + group; k += 4)
                {
                    const auto r = _mm_load_ps (re + k), i = _mm_load_ps (im + k);
                    const auto c = _mm_load_ps (turnCos + k), s = _mm_load_ps (turnSin + k);
                    const auto nextRe = _mm_sub_ps (_mm_mul_ps (r, c), _mm_mul_ps (i, s));
                    const auto nextIm = _mm_add_ps (_mm_mul_ps (r, s), _mm_mul_ps (i, c));
                    _mm_store_ps (re + k, nextRe);
                    _mm_store_ps (im + k, nextIm);
                    sumLeft = _mm_add_ps (sumLeft, _mm_mul_ps (nextIm, _mm_load_ps (gainLeft + k)));
                    sumRight = _mm_add_ps (sumRight, _mm_add_ps (_mm_mul_ps (nextIm, _mm_load_ps (rightSin + k)),
                                                                 _mm_mul_ps (nextRe, _mm_load_ps (rightCos + k))));
                }
            alignas (16) float lanes[4];
            _mm_store_ps (lanes, sumLeft);
            left = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
            _mm_store_ps (lanes, sumRight);
            right = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
        }
       #else
        for (int g = 0; g < liveGroups; ++g)
            for (int k = liveGroup[g]; k < liveGroup[g] + group; ++k)
            {
                const auto r = re[k];
                re[k] = r * turnCos[k] - im[k] * turnSin[k];
                im[k] = r * turnSin[k] + im[k] * turnCos[k];
                left += im[k] * gainLeft[k];
                right += im[k] * rightSin[k] + re[k] * rightCos[k];
            }
       #endif
        if (hasNoise && pluckNoise)
        {
            // The modelled pluck's noise is one draw in both channels (the
            // reference render is dual mono: two draws lost 3 dB in the
            // middle).
            noiseSeed = noiseSeed * 1664525u + 1013904223u;
            const auto white = ((float) (noiseSeed >> 8) * (2.0f / 16777216.0f) - 1.0f) * 1.7320508f;
            auto sum = 0.0f;
            for (auto& band : noise)
            {
                sum += band.gain * band.filter (band.left, white);
                band.gain += band.step;
            }
            left += sum;
            right += sum;
        }
        else if (hasNoise)
        {
            // Uniform, unit variance; a draw for each channel.
            noiseSeed = noiseSeed * 1664525u + 1013904223u;
            noiseSeedRight = noiseSeedRight * 1664525u + 1013904223u;
            const auto whiteLeft = ((float) (noiseSeed >> 8) * (2.0f / 16777216.0f) - 1.0f) * 1.7320508f;
            const auto whiteRight = ((float) (noiseSeedRight >> 8) * (2.0f / 16777216.0f) - 1.0f) * 1.7320508f;
            for (auto& band : noise)
            {
                left += band.gain * band.filter (band.left, whiteLeft);
                right += band.gain * band.filter (band.right, whiteRight);
                band.gain += band.step;
            }
        }
        left *= partialRamp;
        right *= partialRamp;
        partialRamp += partialRampStep;
        if (pluckAt < pluckLength)
        {
            // The modelled pluck's swell (raised cosine) to its top.
            const auto rise = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) pluckAt / (float) pluckLength);
            left *= rise;
            right *= rise;
            ++pluckAt;
        }

        if (attackLeft > 0)
        {
            const auto i = (int) attackPosition;
            const auto f = (float) (attackPosition - i);
            const auto at = [i, f] (const std::vector<float>& x) { return x[(size_t) i] + f * (x[(size_t) i + 1] - x[(size_t) i]); };
            const auto gain = attackGain * attackFade;
            const auto fromLeft = at (*attackA) + attackMix * (at (*attackB) - at (*attackA));
            const auto fromRight = at (*attackRightA) + attackMix * (at (*attackRightB) - at (*attackRightA));
            left += fromLeft * gain;
            right += fromRight * gain;
            attackFade += attackFadeStep;
            attackPosition += attackStep;
            --attackLeft;
        }
        lastSide = 0.5f * (left - right) * outputGain;
        return 0.5f * (left + right) * outputGain;
    }

    // (left - right) / 2 of the sample process() last returned.
    float side() const { return lastSide; }

   #if ILANA_MODAL_SSE
    // The partials, eight to an AVX register.
    ILANA_MODAL_AVX_TARGET void sumPartialsAvx (float& left, float& right)
    {
        auto sumLeft = _mm256_setzero_ps(), sumRight = _mm256_setzero_ps();
        for (int g = 0; g < liveGroups; ++g)
        {
            const auto k = liveGroup[g];
            const auto r = _mm256_loadu_ps (re + k), i = _mm256_loadu_ps (im + k);
            const auto c = _mm256_loadu_ps (turnCos + k), s = _mm256_loadu_ps (turnSin + k);
            const auto nextRe = _mm256_sub_ps (_mm256_mul_ps (r, c), _mm256_mul_ps (i, s));
            const auto nextIm = _mm256_add_ps (_mm256_mul_ps (r, s), _mm256_mul_ps (i, c));
            _mm256_storeu_ps (re + k, nextRe);
            _mm256_storeu_ps (im + k, nextIm);
            sumLeft = _mm256_add_ps (sumLeft, _mm256_mul_ps (nextIm, _mm256_loadu_ps (gainLeft + k)));
            sumRight = _mm256_add_ps (sumRight, _mm256_add_ps (_mm256_mul_ps (nextIm, _mm256_loadu_ps (rightSin + k)),
                                                               _mm256_mul_ps (nextRe, _mm256_loadu_ps (rightCos + k))));
        }
        left = sumOf (sumLeft);
        right = sumOf (sumRight);
    }
    static ILANA_MODAL_AVX_TARGET float sumOf (__m256 x)
    {
        const auto sum = _mm_add_ps (_mm256_castps256_ps128 (x), _mm256_extractf128_ps (x, 1));
        alignas (16) float lanes[4];
        _mm_store_ps (lanes, sum);
        return (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
    }
   #endif

private:
    struct Corners
    {
        std::array<const ModalPianoTable::Entry*, 4> entries {}; // lo/hi note x lo/hi velocity
        std::array<float, 4> weights {};
        int noteIndex = 0, velocityIndex = 0;
        float noteWeight = 0.0f, velocityWeight = 0.0f;
    };

    static void bracket (const std::vector<int>& values, float x, int& index, float& weight)
    {
        index = 0;
        weight = 0.0f;
        const auto last = (int) values.size() - 1;
        if (last <= 0 || x <= (float) values.front())
            return;
        if (x >= (float) values.back())
        {
            index = last;
            return;
        }
        while (index < last - 1 && (float) values[(size_t) index + 1] <= x)
            ++index;
        weight = (x - (float) values[(size_t) index]) / (float) (values[(size_t) index + 1] - values[(size_t) index]);
    }

    static void findCorners (const ModalPianoTable& table, int midiNote, float velocity, Corners& c)
    {
        bracket (table.notes, (float) midiNote, c.noteIndex, c.noteWeight);
        bracket (table.velocities, velocity, c.velocityIndex, c.velocityWeight);
        const auto nextNote = juce::jmin (c.noteIndex + 1, (int) table.notes.size() - 1);
        const auto nextVelocity = juce::jmin (c.velocityIndex + 1, (int) table.velocities.size() - 1);
        c.entries = { &table.at (c.noteIndex, c.velocityIndex), &table.at (nextNote, c.velocityIndex),
                      &table.at (c.noteIndex, nextVelocity), &table.at (nextNote, nextVelocity) };
        c.weights = { (1.0f - c.noteWeight) * (1.0f - c.velocityWeight), c.noteWeight * (1.0f - c.velocityWeight),
                      (1.0f - c.noteWeight) * c.velocityWeight, c.noteWeight * c.velocityWeight };
    }

    // A partial past an entry's last one: the stretch carried on from it.
    static float ratioOf (const ModalPianoTable::Entry& e, int k)
    {
        if (k < e.partials)
            return juce::jmax (0.05f, e.ratios[(size_t) k]);
        if (e.partials < 2)
            return (float) (k + 1);
        const auto last = (float) e.partials;
        const auto b = juce::jmax (1.0e-7f, ((e.ratios.back() / last) * (e.ratios.back() / last) - 1.0f) / (last * last));
        const auto n = (float) (k + 1);
        return n * std::sqrt (1.0f + b * n * n);
    }

    static float levelOf (const ModalPianoTable::Entry& e, int k, int t, int times)
    {
        return k < e.partials ? e.levels[(size_t) (k * times + t)] : -160.0f;
    }

    // The strike's loudness: all partials' power at 0.05 s (the first time,
    // or the first after the guitar's early ones).
    static float energyDb (const ModalPianoTable& table, const Corners& c)
    {
        const auto times = (int) table.times.size();
        auto power = 1.0e-20;
        for (int k = 0; k < maxPartials; ++k)
        {
            auto level = 0.0f;
            for (int i = 0; i < 4; ++i)
                level += c.weights[(size_t) i] * levelOf (*c.entries[(size_t) i], k, table.lateTime, times);
            power += std::pow (10.0, level / 10.0);
        }
        return (float) (10.0 * std::log10 (power));
    }

    // The rotator's length is the partial's level. Each block it is set
    // back to exactly the level it should be at (no drift), then its turn
    // is scaled so the length goes there and on to the next block's level
    // along an exponential, which is how a level that falls in dB falls.
    // A silent partial keeps a tiny length (-160 dB) so its phase stays.
    // (span: samples to the target; the giant body's slots go 8 blocks at a time.)
    void setLevel (int k, float target, int span = blockSize)
    {
        static constexpr float floorLevel = 1.0e-8f;
        const auto from = amp[k] > 0.0f ? amp[k] : floorLevel;
        const auto length2 = re[k] * re[k] + im[k] * im[k];
        auto scale = length2 > 0.0f ? from / std::sqrt (length2) : 1.0f;
        auto start = from;
        if (amp[k] == 0.0f && target > 0.0f)
        {
            // Starting (the strike, or back from silence): at its level at once.
            scale *= target / floorLevel;
            start = target;
        }
        re[k] *= scale;
        im[k] *= scale;
        const auto to = target > 0.0f ? target : floorLevel;
        const auto growth = start == to ? 1.0f : std::exp2 (std::log2 (to / start) * (1.0f / (float) span));
        turnCos[k] = rotCos[k] * growth;
        turnSin[k] = rotSin[k] * growth;
        amp[k] = target;
    }

    int roundedCount() const { return (slots + group - 1) / group * group; }

    // SIZE and SCALE on a partial's level (2026-11-11; ilana: "size and
    // scale are not doin anything": they only moved the faint body modes
    // and the decay). SIZE: the instrument's average tone moved down by the
    // body's growth (a bigger box rings lower: boomier, darker; a smaller
    // one thinner, brighter), at 0.6 of its depth, within 9 dB. SCALE: a
    // longer string at the same pitch is tighter and brighter, 3 dB per
    // octave of partial number per doubling, within 12 dB. 0 at x1 and x1.
    float shapeGainDb (int k, float partialHz) const
    {
        auto db = 0.0f;
        if (bodySize != 1.0f && tableData != nullptr)
            db += bodyShapeDb (partialHz);
        if (stringScale != 1.0f)
            db += juce::jlimit (-12.0f, 12.0f, 3.0f * std::log2 (stringScale) * std::log2 ((float) (k + 1)));
        if (slots > giantStart)
            db -= giantDuckDb();
        return db;
    }

    // ---- SIZE, physically (2026-11-13; ilana: "make it more realistic",
    // "push all modal instruments in this manner ... to create novel
    // timbres"). Exact at x1 (every term below is skipped there), real
    // near it, and carried on smoothly as a "what if" out to x0.25 and x4,
    // with soft limits (tanh) instead of clamps. All of it is worked out
    // here, per note or when SIZE or the pitch moves: nothing per sample.
    static float softLimit (float x, float limit) { return limit * std::tanh (x / limit); }
    static float smoothstep (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    // The guitar's air (A0) and top (T(1,1)2) modes are coupled
    // (Christensen & Vistisen 1980): decoupled once into the Helmholtz
    // resonance and the top's own (coupling c 0.5), then the air moves as
    // SIZE^-1.25 (a scaled box with a scaled hole is SIZE^-1, a fixed hole
    // SIZE^-1.5), the top and the rest of the plate as SIZE^-1.5 (thickness
    // fixed would be SIZE^-2, scaled SIZE^-1; the bracing stiffens), and
    // the pair recoupled. A0 102.5 Hz at x1: 197 Hz at x0.6 (a tenor uke),
    // 56 at x1.6, 32 at x2.5 (under E2: a guitar half drum).
    void setBodyLaws()
    {
        const auto& table = *tableData;
        logSize = std::log2 (bodySize);
        if (table.guitarBody)
        {
            const auto fm2 = (double) table.airHz * table.airHz, fp2 = (double) table.topHz * table.topHz;
            constexpr double c2 = 0.25;
            const auto sum = fm2 + fp2, product = fm2 * fp2 / (1.0 - c2);
            const auto d = std::sqrt (juce::jmax (0.0, sum * sum - 4.0 * product));
            const auto h2 = 0.5 * (sum - d) * std::pow ((double) bodySize, -2.5);
            const auto p2 = 0.5 * (sum + d) * std::pow ((double) bodySize, -3.0);
            const auto mid = 0.5 * (h2 + p2), half = 0.5 * std::sqrt ((p2 - h2) * (p2 - h2) + 4.0 * c2 * h2 * p2);
            airRatio = (float) (std::sqrt (juce::jmax (0.0, mid - half)) / table.airHz);
            topRatio = (float) (std::sqrt (mid + half) / table.topHz);
            plateRatio = std::exp2 (-1.5f * logSize);
        }
        // The grand's board noise (0-500 Hz, the thump) on the board's time.
        noiseTime = std::exp2 (2.0f * std::tanh (-logSize));
        // Past x2, the giant body (giantBody): 0 at x2, 1 at x4.
        giant = logSize > 1.0f ? juce::jmin (1.0f, logSize - 1.0f) : 0.0f;
    }

    // The grand's board: its thickness hardly changes with a piano's size,
    // so its low modes go as h/L^2, SIZE^-2 (the lowest strong one, 58 Hz in
    // this table, would be ~100 Hz on an upright); above ~1 kHz the ribs
    // hold the waves and it hardly moves (SIZE^-0.5).
    static float boardPower (float hz)
    {
        if (hz <= 200.0f)
            return 2.0f;
        if (hz >= 1200.0f)
            return 0.5f;
        return 2.0f - 1.5f * std::log2 (hz / 200.0f) / 2.5849625f;
    }

    // How far a body mode at hz (of this kind) moves.
    float modeRatio (unsigned char kind, float hz) const
    {
        if (kind == ModalPianoTable::stringMode)
            return 1.0f;
        if (! tableData->guitarBody)
            return std::exp2 (-boardPower (hz) * logSize);
        return kind == ModalPianoTable::airMode ? airRatio : kind == ModalPianoTable::topMode ? topRatio : plateRatio;
    }

    // A mode moved under 30 Hz or over 0.45 of the rate fades out over an
    // octave (and none moved toward the middle is raised).
    float edgeDb (float hz) const
    {
        const auto x = juce::jmax (std::log2 (30.0f / hz), std::log2 (hz / (0.45f * (float) sampleRate)));
        const auto g = 1.0f - smoothstep (x);
        return g > 1.0e-7f ? 20.0f * std::log10 (g) : -140.0f;
    }

    // A body mode slot's Hz, decay (ringScale) and level (modeDb) at this SIZE.
    void moveMode (int k)
    {
        if (k >= giantStart)
        {
            moveGiant (k);
            return;
        }
        if (k < modeSlots)
        {
            const auto m = (size_t) (k - count);
            const auto from = modeA->modes[m];
            const auto kind = m < modeA->modeKind.size() ? modeA->modeKind[m] : ModalPianoTable::boardMode;
            const auto r = modeRatio (kind, from);
            hz[k] = from * r;
            // Constant Q: a mode moved by r rings 1/r as long (to ~4x).
            ringScale[k] = std::exp2 (2.0f * std::tanh (std::log2 (r) * 0.5f));
            // A mode's level hardly changes with size (radiation ~ area x
            // f x velocity, the velocity ~ 1 / mass); the air mode's a
            // little (2 dB per doubling).
            modeDb[k] = juce::jmin (0.0f, edgeDb (hz[k]) - edgeDb (from))
                        + (kind == ModalPianoTable::airMode ? softLimit (2.0f * logSize, 6.0f) : 0.0f);
            if (slots > giantStart)
                modeDb[k] -= giantDuckDb();
            return;
        }
        const auto j = k - modeSlots;
        const auto a = infillFrom[j], b = infillTo[j];
        hz[k] = std::exp2 (std::log2 (hz[a]) + infillU[j] * (std::log2 (hz[b]) - std::log2 (hz[a])));
        ringScale[k] = std::sqrt (ringScale[a] * ringScale[b]);
        const auto fade = smoothstep (2.0f * std::log2 (bodySize / infillAt[j]));
        modeDb[k] = (fade > 1.0e-7f ? 20.0f * std::log10 (fade) : -140.0f) + juce::jmin (0.0f, edgeDb (hz[k]));
        if (slots > giantStart)
            modeDb[k] -= giantDuckDb();
    }

    // A bigger plate has more modes per Hz (~ area, SIZE^2 at a fixed
    // thickness): past x1.4 the free slots fill with modes between moved
    // neighbours (the widest gaps first, at a random point between them),
    // each fading in over half an octave of SIZE from where the density
    // asks for it.
    void addInfill (int modes, std::uint32_t& seed)
    {
        if (bodySize <= 1.4f || modes < 2)
            return;
        float moved[maxModes];
        int order[maxModes], gaps[maxModes];
        for (int m = 0; m < modes; ++m)
        {
            const auto kind = (size_t) m < modeA->modeKind.size() ? modeA->modeKind[(size_t) m] : ModalPianoTable::boardMode;
            moved[m] = modeA->modes[(size_t) m] * modeRatio (kind, modeA->modes[(size_t) m]);
            order[m] = m;
        }
        std::sort (order, order + modes, [&moved] (int x, int y) { return moved[x] < moved[y]; });
        const auto pairs = modes - 1;
        for (int i = 0; i < pairs; ++i)
            gaps[i] = i;
        std::stable_sort (gaps, gaps + pairs, [&] (int x, int y)
                          { return moved[order[x + 1]] / moved[order[x]] > moved[order[y + 1]] / moved[order[y]]; });
        for (int i = 0; i < maxModes - modes; ++i)
        {
            const auto at = 1.4f * std::sqrt (1.0f + (float) i / (float) modes);
            if (bodySize <= at)
                break;
            const auto pair = gaps[i % pairs];
            seed = seed * 1664525u + 22695477u;
            infillFrom[i] = count + order[pair];
            infillTo[i] = count + order[pair + 1];
            infillU[i] = 0.3f + 0.4f * (float) (seed >> 8) / 16777216.0f;
            infillAt[i] = at;
            const auto k = slots++;
            seed = seed * 1664525u + 22695477u;
            const auto phase = (float) (seed >> 8) * (juce::MathConstants<float>::twoPi / 16777216.0f);
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = 0.0f;
        }
    }

    // ---- SIZE past x2: the giant body (2026-11-13; ilana: "past x2 it
    // leaves physics: dense ringing body modes stay in the strings' range,
    // a reverb-like bloom, longer ring-on; x1-x2 stays as is"). Physics
    // alone sends every body mode under the strings past ~x2 (and fades
    // it under 30 Hz), so x2.5 and x4 sounded alike above 100 Hz. Past x2
    // (giant: 0 at x2, smoothstep in log2 SIZE to 1 at x4) a layer of
    // extra mode slots crossfades in, filling the strings' range:
    //   body modes  a ladder through the strings' range (jittered per
    //               slot, sinking slowly as SIZE^-0.5 past x2), fed by the
    //               note's spectrum near each one (a broad kernel, plus a
    //               narrow one for partials right on a mode), low end cut;
    //   sympathetic the note's lowest partials again, a few cents off
    //               (each voice its own detune): a halo that beats slowly
    //               against the strings and outlasts them;
    // each slot fading in at its own rank (denser with SIZE), blooming
    // (a rise of tau) and ringing long (T60 rising with SIZE; slower still
    // after key-up), with the strings and the table's body ducked a few dB
    // so the level holds. Two flavours (GiantFlavour): the guitar a giant
    // hollow box, nearly a gong (one fixed set of modes for every note,
    // 55 Hz-1.8 kHz, upper modes swell later); the grand a piano on a huge
    // plate (each voice its own denser plate modes to 6 kHz, a fast bloom,
    // a strong halo). All of it per note and per block, in the existing
    // phasor bank: nothing new per sample.
    struct GiantFlavour
    {
        int sympathetic, body;           // slots of each
        float lowHz, highHz;             // the body ladder at x2
        bool shared;                     // one ladder for every note (else each voice's own)
        float cornerHz;                  // the layer's low cut (12 dB/octave under it)
        float bodyShare;                 // of the layer's power, the body modes' (the rest the halo's)
        float t60From, t60To, t60Tilt, tiltHz; // body T60 (s) at giant 0 and 1, times (Hz / tiltHz)^-tilt
        float haloT60From, haloT60To;
        float bloomFrom, bloomTo, bloomHigh;   // rise tau (s); + bloomHigh x sqrt (Hz / 1 kHz) x giant
        float layerFromDb, layerToDb;    // the layer's power over the note's (at 0.05 s), giant 0 and 1
        float duckDb;                    // the strings and table body down by this x giant
        float haloCents;                 // the halo's detune, up to
        float whiten;                    // a body mode feeds on the note's power to this (under 1: brighter, less boom)
        float tilt;                      // and its feed tilts up as (Hz / 500)^tilt (against mud)
    };
    const GiantFlavour& flavour() const
    {
        static constexpr GiantFlavour box { 4, 20, 55.0f, 4000.0f, true, 180.0f, 0.75f, 3.0f, 10.0f, 0.25f, 200.0f,
                                            2.5f, 8.0f, 0.02f, 0.10f, 0.40f, -9.0f, 1.0f, 2.0f, 6.0f, 0.6f, 0.7f };
        static constexpr GiantFlavour plate { 6, 18, 110.0f, 6000.0f, false, 100.0f, 0.6f, 3.0f, 12.0f, 0.35f, 500.0f,
                                              3.0f, 14.0f, 0.01f, 0.10f, 0.0f, -9.0f, 1.0f, 1.5f, 5.0f, 0.8f, 0.0f };
        return tableData->guitarBody ? box : plate;
    }
    // A fixed number in [0, 1) for slot j (the guitar's one ladder, stereo places).
    static float hash01 (int j, int salt)
    {
        auto h = (std::uint32_t) j * 2654435761u + (std::uint32_t) salt * 40503u + 0x9e3779b9u;
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;
        h *= 3266489917u;
        h ^= h >> 16;
        return (float) (h >> 8) / 16777216.0f;
    }
    float giantDuckDb() const { return flavour().duckDb * giant; }

    // The giant body's slots, after the infill (SIZE over x2 at the note's start).
    void addGiant (std::uint32_t& seed)
    {
        const auto& f = flavour();
        const auto next = [&seed]
        {
            seed = seed * 1664525u + 22695477u;
            return (float) (seed >> 8) / 16777216.0f;
        };
        const auto add = [&] (int partial, float hz, float detuneCents, float rank)
        {
            const auto j = slots - giantStart;
            giantPartial[j] = partial;
            giantBaseHz[j] = hz;
            giantDetune[j] = std::exp2 (detuneCents / 1200.0f);
            giantRank[j] = rank;
            giantCouple[j] = -140.0f;
            const auto k = slots++;
            const auto phase = next() * juce::MathConstants<float>::twoPi;
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = 0.0f;
        };
        // (Silent slots up to a group's edge first: the giant body's groups,
        // always sounding, then don't keep the partials' last one running.)
        while (slots % group != 0)
            add (-2, 1000.0f, 0.0f, 2.0f);
        for (int i = 0; i < f.sympathetic && i < count; ++i)
        {
            const auto cents = f.haloCents * (0.4f + 0.6f * next()) * (next() < 0.5f ? -1.0f : 1.0f);
            add (i, 0.0f, cents, 0.5f * (float) i / (float) f.sympathetic);
        }
        for (int b = 0; b < f.body; ++b)
        {
            const auto u = f.shared ? hash01 (b, 1) : next();
            const auto at = ((float) b + 0.5f + 0.8f * (u - 0.5f)) / (float) f.body;
            add (-1, f.lowHz * std::pow (f.highHz / f.lowHz, at), 1.5f * (2.0f * next() - 1.0f),
                 (float) ((b * 7) % f.body) / (float) f.body);
        }
    }

    // A giant slot's Hz, level (but for its bloom and fall), fall and bloom at this SIZE.
    void moveGiant (int k)
    {
        const auto& f = flavour();
        const auto j = k - giantStart;
        const auto halo = giantPartial[j] >= 0;
        hz[k] = (halo ? hz[giantPartial[j]] : giantBaseHz[j] * std::exp2 (-0.5f * (logSize - 1.0f))) * giantDetune[j];
        ringScale[k] = 1.0f;
        modeDb[k] = 0.0f;
        const auto on = smoothstep ((1.5f * giant - giantRank[j]) * 4.0f);
        giantDb[j] = giantCouple[j] + f.layerFromDb + giant * (f.layerToDb - f.layerFromDb)
                     + (on > 1.0e-7f ? 20.0f * std::log10 (on) : -140.0f) + juce::jmin (0.0f, edgeDb (hz[k]));
        const auto t60 = halo ? f.haloT60From * std::pow (f.haloT60To / f.haloT60From, giant)
                              : f.t60From * std::pow (f.t60To / f.t60From, giant) * std::pow (juce::jmax (20.0f, hz[k]) / f.tiltHz, -f.t60Tilt);
        giantFall[j] = 60.0f / juce::jmax (0.05f, t60);
        giantBloom[j] = f.bloomFrom + giant * (f.bloomTo - f.bloomFrom + f.bloomHigh * std::sqrt (hz[k] / 1000.0f));
    }

    // What each giant slot gets from this note (once, at its start): the
    // halo its partial's power, a body mode the note's power near it; the
    // body and the halo each scaled to their share of the note's power.
    void coupleGiant()
    {
        const auto& f = flavour();
        const auto& table = *tableData;
        const auto times = (int) table.times.size();
        float power[maxPartials], fed[maxPartials];
        auto total = 1.0e-30f;
        for (int k = 0; k < count; ++k)
        {
            auto level = levelOffset + shapeDb[k];
            for (int i = 0; i < 4; ++i)
                level += corners.weights[(size_t) i] * levelOf (*corners.entries[(size_t) i], k, table.lateTime, times);
            power[k] = level < -140.0f ? 0.0f : std::pow (10.0f, level / 10.0f);
            fed[k] = level < -140.0f ? 0.0f : std::pow (10.0f, f.whiten * level / 10.0f);
            total += power[k];
        }
        float feed[maxGiant];
        auto bodySum = 1.0e-30f, haloSum = 1.0e-30f;
        for (int k = giantStart; k < slots; ++k)
        {
            const auto j = k - giantStart;
            auto c = 0.0f;
            if (giantPartial[j] == -2)
                c = 0.0f;
            else if (giantPartial[j] >= 0)
            {
                c = power[giantPartial[j]];
                haloSum += c;
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const auto r = hz[i] / hz[k], x = r - 1.0f / r;
                    c += fed[i] * (1.0f / (1.0f + 2.0f * x * x) + 4.0f / (1.0f + 1600.0f * x * x));
                }
                const auto low = f.cornerHz / hz[k];
                c *= std::pow (hz[k] / 500.0f, f.tilt) / (1.0f + low * low * low * low);
                bodySum += c;
            }
            feed[j] = c;
        }
        for (int k = giantStart; k < slots; ++k)
        {
            const auto j = k - giantStart;
            const auto share = giantPartial[j] == -2 ? 0.0f : giantPartial[j] >= 0 ? (1.0f - f.bodyShare) / haloSum : f.bodyShare / bodySum;
            const auto p = total * share * feed[j];
            giantCouple[j] = p > 1.0e-14f ? 10.0f * std::log10 (p) : -140.0f;
            moveGiant (k);
        }
    }

    // SIZE on a partial's level.
    // The guitar, below ~1 kHz: the box's two strong resonances (air and
    // top, Q 12, peaks ~9 and ~7 dB as the table's A0 bump), moved, over
    // where they were; and its bass corner (under the air mode the hole
    // and the top cancel, 12 dB/octave from 0.8 A0) moved with A0: a small
    // box loses its bass, a big one keeps it. Above ~1.5 kHz (crossfaded
    // from 700 Hz) the average tone moved as the plate (SIZE^1.5), as before.
    // The grand: the average tone moved as its board (SIZE^p(f), 2 in the
    // bass to 0.5 from 1.2 kHz), and its bass radiation corner (55 Hz in
    // this table, where the board modes under it are ~12 dB weaker) moved
    // as SIZE^-2: an upright, a spinet, a toy piano below, the sub-bass
    // opening up above.
    float bodyShapeDb (float hz) const
    {
        const auto& table = *tableData;
        const auto corner = [hz] (float from, float to)
        {
            const auto a = from / hz, b = to / hz;
            return (1.0f + a * a * a * a) / (1.0f + b * b * b * b);
        };
        if (table.guitarBody)
        {
            const auto w = smoothstep (std::log2 (hz / 700.0f) * 0.9086f); // (1 over log2 (1500 / 700))
            auto low = 0.0f, high = 0.0f;
            if (w < 1.0f)
            {
                const auto resonances = [hz] (float air, float top)
                {
                    constexpr float q = 12.0f, airGain = (2.5f / q) * (2.5f / q), topGain = (2.0f / q) * (2.0f / q);
                    const auto lorentz = [hz, q] (float f)
                    {
                        const auto x = hz / f, y = 1.0f - x * x;
                        return 1.0f / (y * y + x * x / (q * q));
                    };
                    return 1.0f + airGain * lorentz (air) + topGain * lorentz (top);
                };
                const auto air = table.airHz * airRatio, top = table.topHz * topRatio;
                low = softLimit (10.0f * std::log10 (resonances (air, top) / resonances (table.airHz, table.topHz)
                                                     * corner (0.8f * table.airHz, 0.8f * air)), 20.0f);
            }
            if (w > 0.0f)
                high = softLimit (0.6f * (table.curveAt (hz * std::exp2 (1.5f * logSize)) - table.curveAt (hz)), 9.0f);
            return low + w * (high - low);
        }
        const auto average = softLimit (0.6f * (table.curveAt (hz * std::exp2 (boardPower (hz) * logSize)) - table.curveAt (hz)), 9.0f);
        constexpr float bassCorner = 55.0f;
        return average + softLimit (10.0f * std::log10 (corner (bassCorner, bassCorner * std::exp2 (-2.0f * logSize))), 24.0f);
    }

    // The guitar: a partial near a body mode drains into it (Gough 1981):
    // ~6 dB/s more at a moved mode's peak (Q 12), and up to 1.5 dB/s less
    // where one has moved away.
    float drainOf (float hz) const
    {
        const auto& table = *tableData;
        if (! table.guitarBody || hz > 2000.0f)
            return 0.0f;
        const auto closeness = [hz] (float f)
        {
            const auto x = 12.0f * (hz / f - f / hz);
            return 1.0f / (1.0f + x * x);
        };
        const auto d = 6.0f * (closeness (table.airHz * airRatio) + closeness (table.topHz * topRatio) - closeness (table.airHz) - closeness (table.topHz));
        return d > 0.0f ? d : softLimit (d, 1.5f);
    }

    // The grand's lowest noise band (0-500 Hz: the board's thump) read at
    // the board's time: a big board booms on, a small one stops dead.
    float boardNoiseDb (double t) const
    {
        const auto& table = *tableData;
        const auto times = (int) table.times.size();
        const auto tn = t * (double) noiseTime;
        auto s = 0;
        while (s < times - 2 && tn >= table.times[(size_t) s + 1])
            ++s;
        auto a = 0.0f, c = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const auto& e = *corners.entries[(size_t) i];
            a += corners.weights[(size_t) i] * e.noiseLevels[(size_t) s];
            c += corners.weights[(size_t) i] * e.noiseLevels[(size_t) s + 1];
        }
        const auto t0 = table.times[(size_t) s], t1 = table.times[(size_t) s + 1];
        const auto u = (float) juce::jlimit (0.0, 1.0, (std::log (tn + 0.005) - std::log (t0 + 0.005))
                                                         / (std::log (t1 + 0.005) - std::log (t0 + 0.005)));
        auto db = a + u * (c - a) + levelOffset;
        if (const auto beyond = (float) juce::jmax (0.0, tn - table.times.back()); beyond > 0.0f)
            db += juce::jmin (-1.0f, (c - a) / (t1 - t0)) * beyond;
        return db;
    }

    void updatePitch()
    {
        pitchDirty = false;
        if (shapeDbDirty)
        {
            shapeDbDirty = false;
            segment = -1; // (the levels below are read again with the new shape)
        }
        for (int k = 0; k < count; ++k)
        {
            const auto ratio = harmonic[k] + (measured[k] - harmonic[k]) * (stiffness * stretchScale);
            hz[k] = (float) (ratio * frequency);
            shapeDb[k] = shapeGainDb (k, hz[k]);
            if (sized)
                drainRate[k] = drainOf (hz[k]);
            const auto w = juce::MathConstants<double>::twoPi * juce::jmin ((double) hz[k], sampleRate * 0.49) / sampleRate;
            rotCos[k] = (float) std::cos (w);
            rotSin[k] = (float) std::sin (w);
            blockTurn[k] = blockSize * (double) std::atan2 (rotSin[k], rotCos[k]);
        }
        for (int k = count; k < slots; ++k)
        {
            if (! sized && k < modeSlots)
                hz[k] = modeA != nullptr ? modeA->modes[(size_t) (k - count)] / bodySize : modeHz[k - count];
            else
                moveMode (k);
            const auto w = juce::MathConstants<double>::twoPi * (sized ? juce::jmin ((double) hz[k], sampleRate * 0.49) : (double) hz[k]) / sampleRate;
            rotCos[k] = (float) std::cos (w);
            rotSin[k] = (float) std::sin (w);
            blockTurn[k] = blockSize * (double) std::atan2 (rotSin[k], rotCos[k]);
        }
    }

    // Every block: each partial's level on the measured curve (log time
    // between the table's times), the knobs' extra losses, and a ramp to it.
    void nextBlock()
    {
        untilBlock = blockSize;
        if (pitchDirty)
            updatePitch();

        const auto& table = *tableData;
        const auto times = (int) table.times.size();
        const auto now = (double) (elapsed + blockSize) / sampleRate;
        const auto t = now / (timeScale * stringScale);

        auto s = 0;
        while (s < times - 2 && t >= table.times[(size_t) s + 1])
            ++s;
        if (s != segment)
        {
            segment = s;
            for (int k = 0; k < count; ++k)
            {
                auto a = 0.0f, b = 0.0f;
                for (int i = 0; i < 4; ++i)
                {
                    a += corners.weights[(size_t) i] * levelOf (*corners.entries[(size_t) i], k, s, times);
                    b += corners.weights[(size_t) i] * levelOf (*corners.entries[(size_t) i], k, s + 1, times);
                }
                levelA[k] = a + levelOffset + shapeDb[k];
                levelB[k] = b + levelOffset + shapeDb[k];
            }
            if (hasNoise)
                for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
                {
                    auto a = 0.0f, c = 0.0f;
                    for (int i = 0; i < 4; ++i)
                    {
                        const auto& e = *corners.entries[(size_t) i];
                        a += corners.weights[(size_t) i] * e.noiseLevels[(size_t) (b * times + s)];
                        c += corners.weights[(size_t) i] * e.noiseLevels[(size_t) (b * times + s + 1)];
                    }
                    noiseA[b] = a + levelOffset;
                    noiseB[b] = c + levelOffset;
                }
            const auto w = corners.velocityWeight;
            for (int k = count; k < modeSlots; ++k)
            {
                const auto m = (size_t) (k - count);
                const auto at = [times] (const ModalPianoTable::Entry& e, size_t mode, int t) { return e.modeLevels[mode * (size_t) times + (size_t) t]; };
                levelA[k] = at (*modeA, m, s) + w * (at (*modeB, m, s) - at (*modeA, m, s)) + levelOffset;
                levelB[k] = at (*modeA, m, s + 1) + w * (at (*modeB, m, s + 1) - at (*modeA, m, s + 1)) + levelOffset;
                if (sized)
                {
                    // SIZE: at constant Q a mode moved by r falls r times
                    // as fast (from its level at 0.05 s; the guitar's early
                    // pluck times as measured), and its level change.
                    const auto late = table.lateTime;
                    const auto ref = at (*modeA, m, late) + w * (at (*modeB, m, late) - at (*modeA, m, late)) + levelOffset;
                    if (s > late)
                        levelA[k] = ref + (levelA[k] - ref) * ringScale[k];
                    if (s + 1 > late)
                        levelB[k] = ref + (levelB[k] - ref) * ringScale[k];
                    levelA[k] += modeDb[k];
                    levelB[k] += modeDb[k];
                }
            }
            // SIZE's infill modes: between their two neighbours, 3 dB under
            // their mean, faded in.
            for (int k = modeSlots; k < giantStart; ++k)
            {
                const auto a = infillFrom[k - modeSlots], b = infillTo[k - modeSlots];
                levelA[k] = 0.5f * (levelA[a] + levelA[b]) - 3.0f + modeDb[k];
                levelB[k] = 0.5f * (levelB[a] + levelB[b]) - 3.0f + modeDb[k];
            }
        }

        const auto t0 = table.times[(size_t) s], t1 = table.times[(size_t) s + 1];
        const auto lastTime = table.times.back();
        // Before the first time: held. Past the last: the last segment's
        // fall per second carries on (at least 1 dB/s).
        const auto u = (float) juce::jlimit (0.0, 1.0, (std::log (t + 0.005) - std::log (t0 + 0.005))
                                                         / (std::log (t1 + 0.005) - std::log (t0 + 0.005)));
        const auto beyond = (float) juce::jmax (0.0, t - lastTime);
        const auto releasedFor = released < 0 ? 0.0f : (float) ((elapsed - released) / sampleRate);
        const auto releaseRate = damper * 250.0f * juce::jlimit (0.4f, 1.5f, std::sqrt ((float) frequency / 262.0f));
        const auto seconds = (float) now;

        // A partial this far under the loudest one (last block's) is not
        // heard next to it: it goes quiet as if under -140 dB.
        const auto cut = skipSilentGroups ? loudestPartial - relativeCut : -1000.0f;
        auto loudest = -200.0f, loudestNow = -200.0f;
        bool slotLive[maxSlots];
        const auto giantRelease = 0.3f * std::exp2 (-3.3f * giant);
        const auto giantTurn = (int) ((elapsed / blockSize) & 7);
        for (int k = 0; k < slots; ++k)
        {
            if (k >= giantStart)
            {
                // The giant body: blooming, then its own long fall (and
                // a slower one after key-up). Slow: each slot is set 8
                // blocks ahead, an eighth of them each block.
                const auto j = k - giantStart;
                if (((j + giantTurn) & 7) != 0 && elapsed > 0)
                {
                    loudest = juce::jmax (loudest, giantLastDb[j]);
                    loudestNow = juce::jmax (loudestNow, giantLastDb[j]);
                    slotLive[k] = amp[k] > 0.0f;
                    continue;
                }
                const auto span = 8 - ((j + giantTurn) & 7);
                const auto at = (float) ((double) (elapsed + span * blockSize) / sampleRate);
                const auto r = at / giantBloom[j];
                const auto bloom = r > 12.0f ? 0.0f : 20.0f * std::log10 (juce::jmax (1.0e-7f, 1.0f - std::exp (-r)));
                const auto releasedThen = released < 0 ? 0.0f : (float) ((elapsed + span * blockSize - released) / sampleRate);
                const auto db = giantDb[j] + bloom - giantFall[j] * at / timeScale
                                - dampRate * juce::jmin (8.0f, hz[k] / 1000.0f) * at
                                - releaseRate * releasedThen * giantRelease;
                giantLastDb[j] = db;
                loudest = juce::jmax (loudest, db);
                loudestNow = juce::jmax (loudestNow, db);
                const auto target = db < -140.0f || db < cut ? 0.0f : std::pow (10.0f, db / 20.0f);
                slotLive[k] = target > 0.0f || amp[k] > 0.0f;
                setLevel (k, target, span * blockSize);
                continue;
            }
            auto db = levelA[k] + u * (levelB[k] - levelA[k]);
            if (beyond > 0.0f)
                db += juce::jmin (-1.0f, (levelB[k] - levelA[k]) / (t1 - t0)) * beyond;
            if (! sized)
                db -= (dampRate * juce::jmin (8.0f, hz[k] / 1000.0f)) * seconds
                      + releaseRate * releasedFor;
            else // (SIZE: partials near the moved body modes drain into them; a big body rings on after the dampers)
                db -= (dampRate * juce::jmin (8.0f, hz[k] / 1000.0f) + (k < count ? drainRate[k] : 0.0f)) * seconds
                      + releaseRate * releasedFor * (k < count ? 1.0f : ringScale[k]);
            loudest = juce::jmax (loudest, db);
            loudestNow = juce::jmax (loudestNow, db);
            const auto target = db < -140.0f || db < cut ? 0.0f : std::pow (10.0f, db / 20.0f);
            slotLive[k] = target > 0.0f || amp[k] > 0.0f;
            setLevel (k, target);
        }
        loudestPartial = loudestNow;
        for (int k = slots; k < roundedCount(); ++k)
            slotLive[k] = false;

        // Only groups with a sounding partial run this block. A silent
        // group's rotators stop too (CPU round 2026-10-31: most of a note's
        // upper partials are silent within a second, many from the strike).
        liveGroups = 0;
        for (int k = 0; k < roundedCount(); k += group)
        {
            auto live = ! skipSilentGroups;
            for (int j = k; j < k + group; ++j)
                live = live || slotLive[j];
            auto& wasLive = groupLive[k / group];
            if (live)
            {
                liveGroup[liveGroups++] = k;
                // Back from silence (the measured curve rises again, or a
                // DECAY move): catch its rotators up with the turns they
                // missed, so it comes back in the phase it would have had.
                if (! wasLive)
                    for (int j = k; j < k + group; ++j)
                    {
                        const auto c = std::cos (missedTurn[j]), sn = std::sin (missedTurn[j]);
                        const auto r = (double) re[j], i = (double) im[j];
                        re[j] = (float) (r * c - i * sn);
                        im[j] = (float) (r * sn + i * c);
                        missedTurn[j] = 0.0;
                    }
            }
            else
                for (int j = k; j < k + group; ++j)
                {
                    missedTurn[j] += blockTurn[j];
                    if (missedTurn[j] > juce::MathConstants<double>::twoPi)
                        missedTurn[j] -= juce::MathConstants<double>::twoPi;
                }
            wasLive = live;
        }

        if (hasNoise)
        {
            // The table's noise is the median FFT bin: back to power per bin,
            // then to the variance each band's filter needs at this rate
            // (the filters overlap: a few Gauss-Seidel passes find the
            // non-negative powers that come closest).
            std::array<float, ModalPianoTable::noiseBandCount> variance {};
            const auto toVariance = (float) ModalPianoTable::noiseFftSize / 6.0f * (float) (sampleRate / table.rate) / 0.6931472f;
            for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
            {
                auto db = noiseA[b] + u * (noiseB[b] - noiseA[b]);
                if (beyond > 0.0f)
                    db += juce::jmin (-1.0f, (noiseB[b] - noiseA[b]) / (t1 - t0)) * beyond;
                if (b == 0 && sized && ! table.guitarBody)
                    db = boardNoiseDb (t); // (SIZE: the board's thump on its own time)
                db -= releaseRate * releasedFor;
                loudest = juce::jmax (loudest, db);
                // (The guitar's top band sits at -138..-150 dB: its floor is lower.)
                variance[(size_t) b] = db < (pluckNoise ? -175.0f : -140.0f) ? 0.0f : std::pow (10.0f, db / 10.0f) * toVariance;
            }
            // (The steady noise only: the burst, scaled too, made the
            // pluck's first 5 ms louder still against the reference.)
            if (pluckNoise)
                for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
                    variance[(size_t) b] *= noiseMedian[b];
            if (burstLive)
            {
                // The pluck's noise, falling.
                burstLive = false;
                for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
                {
                    variance[(size_t) b] += burstVariance[b];
                    burstVariance[b] = burstVariance[b] * burstFactor[b] < 1.0e-12f ? 0.0f : burstVariance[b] * burstFactor[b];
                    burstLive = burstLive || burstVariance[b] > 0.0f;
                }
            }
            auto& power = noisePower;
            for (int pass = 0; pass < 6; ++pass)
                for (int b = 0; b < ModalPianoTable::noiseBandCount; ++b)
                {
                    auto others = 0.0f;
                    for (int j = 0; j < ModalPianoTable::noiseBandCount; ++j)
                        if (j != b)
                            others += noiseLeak[b][j] * power[(size_t) j];
                    power[(size_t) b] = juce::jmax (0.0f, (variance[(size_t) b] - others) / noiseLeak[b][b]);
                }
            for (int j = 0; j < ModalPianoTable::noiseBandCount; ++j)
            {
                auto& band = noise[(size_t) j];
                band.step = (std::sqrt (power[(size_t) j]) - band.gain) / (float) blockSize;
            }
        }

        // The crossfade from the recording's attack into the partials, as
        // per-sample ramps to where it is at the block's end (a gain stepped
        // once a block buzzes: steps 1.5 kHz apart, heard as a hiss on top).
        const auto atEnd = (int) (attackLength - attackLeft) + blockSize;
        auto fadeTarget = 1.0f, rampTarget = 0.0f;
        if (atEnd > fadeStart)
        {
            const auto x = juce::jlimit (0.0f, 1.0f, (float) (atEnd - fadeStart) / (float) juce::jmax (1, attackLength - fadeStart));
            fadeTarget = std::cos (x * juce::MathConstants<float>::halfPi);
            rampTarget = std::sin (x * juce::MathConstants<float>::halfPi);
        }
        attackFadeStep = (fadeTarget - attackFade) / (float) blockSize;
        partialRampStep = (rampTarget - partialRamp) / (float) blockSize;

        if (loudest < -130.0f && attackLeft == 0)
            silent = true;
    }

    double sampleRate = 44100.0, frequency = 261.63;
    int note = 60;
    float timeScale = 1.0f, dampRate = 0.0f, stiffness = 1.0f, hardness = 0.5f, damper = 0.5f;
    bool eco = false, pitchDirty = true, silent = true;
    const ModalPianoTable* tableData = &ModalPianoTable::get();
    float bodySize = 1.0f, stringScale = 1.0f, stretchScale = 1.0f;
    bool shapeDbDirty = false;
    // SIZE's physics (setBodyLaws; sized: SIZE isn't 1, so it's on).
    bool sized = false;
    float logSize = 0.0f, airRatio = 1.0f, topRatio = 1.0f, plateRatio = 1.0f, noiseTime = 1.0f;
    int modeSlots = 0; // the table's modes end here; SIZE's infill modes follow
    float ringScale[maxSlots] {}, modeDb[maxSlots] {}, drainRate[maxSlots] {};
    int infillFrom[maxModes] {}, infillTo[maxModes] {};
    float infillU[maxModes] {}, infillAt[maxModes] {};
    // The giant body (addGiant, past x2): its slots start at giantStart.
    float giant = 0.0f;
    int giantStart = 0;
    int giantPartial[maxGiant] {}; // the halo's partial, or -1: a body mode (-2: silent, to a group's edge)
    float giantBaseHz[maxGiant] {}, giantDetune[maxGiant] {}, giantRank[maxGiant] {}, giantCouple[maxGiant] {};
    float giantDb[maxGiant] {}, giantFall[maxGiant] {}, giantBloom[maxGiant] {}, giantLastDb[maxGiant] {};
    float shapeDb[maxSlots] {}; // each partial's SIZE / SCALE level change (shapeGainDb)
    // The table's sinusoid levels are the reference's in dBFS (about 20 dB
    // quiet); Voice::exciterTrim brings the Modal Piano up to the other
    // oscillators' level, so this stays 1 (Modal Grand, saved without the
    // trim, keeps its level).
    static constexpr float outputGain = 1.0f;

    Corners corners;
    float levelOffset = 0.0f;
    int count = 0, slots = 0, segment = -1, untilBlock = 0; // partials, then partials and modes
    long long elapsed = 0, released = -1;

    alignas (32) float re[maxSlots] {}, im[maxSlots] {}, rotCos[maxSlots] {}, rotSin[maxSlots] {};
    alignas (32) float turnCos[maxSlots] {}, turnSin[maxSlots] {}; // this block's turn, with the level's change
    float amp[maxSlots] {}; // each rotator's length at the block's start (0: silent)
    float loudestPartial = -200.0f;
    int liveGroup[maxSlots / group] {}; // the first slot of each group that sounds this block
    int liveGroups = 0;
    bool groupLive[maxSlots / group] {};
    double missedTurn[maxSlots] {}; // radians a silent slot's rotator has not turned
    double blockTurn[maxSlots] {};  // radians its rotator turns in a block
    // The stereo picture: the left's gain, and the right's as the parts of
    // the sine and cosine that make its gain and phase.
    alignas (32) float gainLeft[maxSlots] {}, rightSin[maxSlots] {}, rightCos[maxSlots] {};
    float lastSide = 0.0f;
    float measured[maxSlots] {}, harmonic[maxSlots] {}, hz[maxSlots] {};
    float levelA[maxSlots] {}, levelB[maxSlots] {};

    float modeHz[maxModes] {};

    // The noise between the partials: white noise through one filter per
    // band of the table's (a low-pass, then octave band-passes), each two
    // biquads deep: the bass's noise is ~50 dB above the treble's, and a
    // second-order skirt would leak more than that into the top bands.
    struct NoiseBand
    {
        struct State { float x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, z1 = 0, z2 = 0; };
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        State left, right;
        float gain = 0, step = 0;

        float filter (State& st, float white)
        {
            const auto y = b0 * white + b1 * st.x1 + b2 * st.x2 - a1 * st.y1 - a2 * st.y2;
            st.x2 = st.x1; st.x1 = white;
            st.y2 = st.y1; st.y1 = y;
            const auto z = b0 * y + b1 * st.u1 + b2 * st.u2 - a1 * st.z1 - a2 * st.z2;
            st.u2 = st.u1; st.u1 = y;
            st.z2 = st.z1; st.z1 = z;
            return z;
        }
    };
    std::array<NoiseBand, ModalPianoTable::noiseBandCount> noise {};
    float noiseA[ModalPianoTable::noiseBandCount] {}, noiseB[ModalPianoTable::noiseBandCount] {};
    float noiseLeak[ModalPianoTable::noiseBandCount][ModalPianoTable::noiseBandCount] {};
    std::array<float, ModalPianoTable::noiseBandCount> noisePower {};
    std::uint32_t noiseSeed = 1, noiseSeedRight = 2;
    bool hasNoise = false;
    // The guitar's noise (the modelled pluck's table): one draw for both
    // channels, a lower floor, and each band's variance times noiseMedian
    // (the table's measure is the band's median bin, which noiseLeak's mean
    // over log-spaced points misses by 3 to 7 dB).
    bool pluckNoise = false;
    float noiseMedian[ModalPianoTable::noiseBandCount] {};

    void designNoise()
    {
        constexpr int bands = ModalPianoTable::noiseBandCount;
        const auto nyquist = 0.49 * sampleRate;
        for (int j = 0; j < bands; ++j)
        {
            const auto lo = (double) ModalPianoTable::noiseBands[j][0], hi = juce::jmin ((double) ModalPianoTable::noiseBands[j][1], nyquist);
            auto& f = noise[(size_t) j];
            if (j == 0)
            {
                // Below the band's top: the bass's noise falls ~30 dB by the
                // next band, and the skirt must be well down by then.
                const auto w = juce::MathConstants<double>::twoPi * 0.6 * hi / sampleRate, alpha = std::sin (w) / (2.0 * 0.7071), c = std::cos (w);
                const auto a0 = 1.0 + alpha;
                f.b0 = f.b2 = (float) ((1.0 - c) / 2.0 / a0);
                f.b1 = (float) ((1.0 - c) / a0);
                f.a1 = (float) (-2.0 * c / a0);
                f.a2 = (float) ((1.0 - alpha) / a0);
            }
            else
            {
                const auto centre = std::sqrt (lo * juce::jmax (lo * 1.01, hi)), q = 4.0; // (narrower than the band, for the same reason)
                const auto w = juce::MathConstants<double>::twoPi * centre / sampleRate, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
                const auto a0 = 1.0 + alpha;
                f.b0 = (float) (alpha / a0);
                f.b1 = 0.0f;
                f.b2 = (float) (-alpha / a0);
                f.a1 = (float) (-2.0 * c / a0);
                f.a2 = (float) ((1.0 - alpha) / a0);
            }
        }

        // noiseLeak[b][j]: band j's filter's mean power gain over band b
        // (log-spaced: the table's median sits near the band's middle).
        for (int b = 0; b < bands; ++b)
        {
            const auto lo = juce::jmax (20.0, (double) ModalPianoTable::noiseBands[b][0]);
            const auto hi = juce::jmin ((double) ModalPianoTable::noiseBands[b][1], nyquist);
            for (int j = 0; j < bands; ++j)
            {
                const auto& f = noise[(size_t) j];
                auto sum = 0.0;
                constexpr int points = 48;
                for (int i = 0; i < points; ++i)
                {
                    const auto hz = lo * std::pow (hi / lo, (i + 0.5) / points);
                    const auto z = std::polar (1.0, -juce::MathConstants<double>::twoPi * hz / sampleRate);
                    const auto h = ((double) f.b0 + (double) f.b1 * z + (double) f.b2 * z * z)
                                   / (1.0 + (double) f.a1 * z + (double) f.a2 * z * z);
                    sum += std::norm (h) * std::norm (h); // (two stages)
                }
                noiseLeak[b][j] = (float) (hi > lo ? sum / points : (b == j ? 1.0 : 0.0));
            }
            noiseLeak[b][b] = juce::jmax (1.0e-6f, noiseLeak[b][b]);
        }

        // noiseMedian[b]: 1 over the median, over evenly spaced Hz in band
        // b (as the table's FFT bins), of the filters' power when the
        // Gauss-Seidel passes ask every band for a variance of 1.
        double power[bands] {};
        for (int pass = 0; pass < 60; ++pass)
            for (int b = 0; b < bands; ++b)
            {
                auto others = 0.0;
                for (int j = 0; j < bands; ++j)
                    if (j != b)
                        others += noiseLeak[b][j] * power[j];
                power[b] = juce::jmax (0.0, (1.0 - others) / noiseLeak[b][b]);
            }
        for (int b = 0; b < bands; ++b)
        {
            const auto lo = juce::jmax (20.0, (double) ModalPianoTable::noiseBands[b][0]);
            const auto hi = juce::jmin ((double) ModalPianoTable::noiseBands[b][1], nyquist);
            constexpr int points = 255;
            std::array<double, points> psd {};
            for (int i = 0; i < points; ++i)
            {
                const auto hz = lo + (hi - lo) * (i + 0.5) / points;
                const auto z = std::polar (1.0, -juce::MathConstants<double>::twoPi * hz / sampleRate);
                for (int j = 0; j < bands; ++j)
                {
                    const auto& f = noise[(size_t) j];
                    const auto h = ((double) f.b0 + (double) f.b1 * z + (double) f.b2 * z * z)
                                   / (1.0 + (double) f.a1 * z + (double) f.a2 * z * z);
                    psd[(size_t) i] += power[j] * std::norm (h) * std::norm (h);
                }
            }
            std::nth_element (psd.begin(), psd.begin() + points / 2, psd.end());
            noiseMedian[b] = hi > lo && psd[points / 2] > 0.0 ? (float) (1.0 / psd[points / 2]) : 1.0f;
        }
    }
    const ModalPianoTable::Entry* modeA = nullptr;
    const ModalPianoTable::Entry* modeB = nullptr;
    const std::vector<float>* attackA = nullptr;
    const std::vector<float>* attackB = nullptr;
    const std::vector<float>* attackRightA = nullptr;
    const std::vector<float>* attackRightB = nullptr;
    float attackMix = 0.0f, attackGain = 1.0f, attackFade = 1.0f, partialRamp = 0.0f;
    float attackFadeStep = 0.0f, partialRampStep = 0.0f;
    double attackPosition = 0.0, attackStep = 1.0;
    int attackLength = 1, attackLeft = 0, fadeStart = 0;
    // The modelled pluck: samples into its swell and the swell's length;
    // its noise per band (noise variance, as nextBlock's) and how much of
    // it each block leaves.
    int pluckAt = 0, pluckLength = 0;
    float burstVariance[ModalPianoTable::noiseBandCount] {}, burstFactor[ModalPianoTable::noiseBandCount] {};
    bool burstLive = false;
};
