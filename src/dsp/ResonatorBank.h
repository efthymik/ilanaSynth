#pragma once

#include <juce_core/juce_core.h>

#include "RatePole.h"

#include <cmath>
#include <vector>

class ResonatorBank
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const auto size = juce::nextPowerOfTwo ((int) (sampleRate / 30.0) + 1);

        for (auto& buffer : buffers)
        {
            buffer.assign ((size_t) size, 0.0f);
        }

        reset();
    }

    void reset()
    {
        for (auto& buffer : buffers)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (auto& state : lowpassState)
            state = 0.0f;

        writePosition = 0;
    }

    void setTuning (double baseFrequency, float offsetSemitones, float keytrack)
    {
        if (buffers[0].empty())
            return;

        const auto trackedRoot = baseFrequency * std::exp2 ((double) offsetSemitones / 12.0);
        const auto fixedRoot = 220.0 * std::exp2 ((double) offsetSemitones / 12.0);
        const auto blendedRoot = trackedRoot * (double) juce::jlimit (0.0f, 1.0f, keytrack)
                                 + fixedRoot * (double) (1.0f - juce::jlimit (0.0f, 1.0f, keytrack));

        for (int i = 0; i < 3; ++i)
        {
            const auto frequency = juce::jlimit (25.0, sampleRate * 0.45, blendedRoot * ratios[i]);
            periods[i] = sampleRate / frequency;
        }
        updateDelays();
    }

    void setParams (float newAmount, float newDecay, float newDamping)
    {
        const auto clampedAmount = juce::jlimit (0.0f, 1.0f, newAmount);

        if (clampedAmount < 0.001f && amount >= 0.001f)
            reset();

        amount = clampedAmount;
        feedback = 0.5f + juce::jlimit (0.0f, 1.0f, newDecay) * 0.48f;
        // Once a sample: kept at its 48 kHz sound at any rate (the feedback
        // is once a trip round the loop, so it already is).
        lowpassCoefficient = onePoleFrom48k (1.0f - juce::jlimit (0.0f, 1.0f, newDamping) * 0.9f, sampleRate);
        updateDelays();
    }

    // excite rings the delay lines without joining the dry mix (the live
    // input in ilanaSynth FX); at 0 the output is exactly as before.
    float process (float input, float excite = 0.0f)
    {
        if (buffers[0].empty() || amount < 0.001f)
            return input;

        const auto size = (int) buffers[0].size();
        auto sum = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            auto readPosition = (double) writePosition - delays[i];

            while (readPosition < 0.0)
                readPosition += (double) size;

            const auto index = (int) readPosition;
            const auto next = (index + 1) % size;
            const auto fraction = (float) (readPosition - (double) index);
            const auto delayed = buffers[(size_t) i][(size_t) index]
                                 + (buffers[(size_t) i][(size_t) next] - buffers[(size_t) i][(size_t) index]) * fraction;

            lowpassState[i] += (delayed - lowpassState[i]) * lowpassCoefficient;
            buffers[(size_t) i][(size_t) writePosition] = input + excite + lowpassState[i] * feedback;
            sum += delayed;
        }

        writePosition = (writePosition + 1) % size;

        return input * (1.0f - amount) + (sum / 3.0f) * amount;
    }

private:
    // The low-pass in each loop delays its note too (by its phase delay
    // there): taken off the line, or the resonator sat flat, about 4 cents
    // at 220 Hz and 15 at 880 at 48 kHz (as the strings did before Debug 5).
    // Cached: the pitch moves at most once a sub-block.
    void updateDelays()
    {
        for (int i = 0; i < 3; ++i)
        {
            if (periods[i] <= 0.0 || (periods[i] == delayPeriods[i] && lowpassCoefficient == delayCoefficients[i]))
                continue;
            delayPeriods[i] = periods[i];
            delayCoefficients[i] = lowpassCoefficient;
            const auto omega = juce::MathConstants<double>::twoPi / periods[i];
            const auto pole = 1.0 - (double) lowpassCoefficient;
            const auto lowpassDelay = std::atan2 (pole * std::sin (omega), 1.0 - pole * std::cos (omega)) / omega;
            delays[i] = juce::jmax (1.25, periods[i] - lowpassDelay);
        }
    }

    static constexpr double ratios[3] = { 1.0, 2.0, 3.0 };

    std::vector<float> buffers[3];
    float lowpassState[3] {};
    double delays[3] {};
    double periods[3] {};
    double delayPeriods[3] {};
    float delayCoefficients[3] {};
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float feedback = 0.9f;
    float lowpassCoefficient = 0.5f;
    int writePosition = 0;
};
