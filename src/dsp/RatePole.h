#pragma once

#include <cmath>

// Loop filters voiced at 48 kHz (where the factory sounds were made and the
// fingerprints run), kept the same in seconds at any other voice rate
// (44.1-192 kHz hosts, 2x/4x oversampling). Both return their input
// unchanged at exactly 48 kHz.

// A one-pole low-pass step y += (x - y) * c: the same time constant.
inline float onePoleFrom48k (float coefficient, double sampleRate)
{
    if (sampleRate == 48000.0)
        return coefficient;
    return (float) (1.0 - std::pow (1.0 - (double) coefficient, 48000.0 / sampleRate));
}

// A first-order allpass (a + z^-1) / (1 + a z^-1): the same delay at DC.
inline float allpassFrom48k (float coefficient, double sampleRate)
{
    if (sampleRate == 48000.0)
        return coefficient;
    const auto delay = (1.0 - (double) coefficient) / (1.0 + (double) coefficient) * sampleRate / 48000.0;
    return (float) ((1.0 - delay) / (1.0 + delay));
}
