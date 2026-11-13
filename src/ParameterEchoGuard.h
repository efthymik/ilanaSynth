#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// Keeps a VST3 host's late echo of the editor's own edits off the parameters.
//
// Ableton Live sends every edit the editor makes back to the plugin through
// the audio callback a block or more later. JUCE's wrapper sets the parameter
// to that echoed value, which by then is older than the knob: mid-drag the
// parameter (and the sound, the filter graph, every display that reads it)
// stepped back a frame, then forward on the next mouse move ("stuttery when
// moving the knob"). HeldSliderAttachment (#254) only kept the knob itself
// still; this keeps the parameter.
//
// The rule, per parameter: the editor's edits (made on the message thread)
// are remembered for the burst they belong to. While the user is editing it
// (a gesture is open, or the last edit is under holdMs old), a value that
// arrives from another thread and equals one of the burst's earlier values
// is an echo: the parameter goes back to the user's latest value. Anything
// else (automation with new values, a preset, an undo, any change once the
// user has let go) passes as before.
class ParameterEchoGuard : private juce::AudioProcessorParameter::Listener
{
public:
    // Longer than a host takes to echo (a few blocks), as HeldSliderAttachment's hold.
    static constexpr double holdMs = 250.0;
    static constexpr int burstSize = 64; // the user's values remembered per burst

    ParameterEchoGuard() = default;

    ~ParameterEchoGuard() override
    {
        for (auto& state : states)
            if (state->parameter != nullptr)
                state->parameter->removeListener (this);
    }

    void attach (const juce::Array<juce::AudioProcessorParameter*>& parameters)
    {
        states.clear();
        states.reserve ((size_t) parameters.size());
        for (auto* parameter : parameters)
        {
            states.push_back (std::make_unique<State>());
            states.back()->parameter = parameter;
            parameter->addListener (this);
        }
    }

    // Echoes put back so far (the UI test reads it).
    int getEchoesBlocked() const { return echoesBlocked.load(); }

private:
    struct State
    {
        juce::AudioProcessorParameter* parameter = nullptr;
        std::atomic<bool> gesture { false };
        std::atomic<double> lastUserMs { -1.0e9 };
        std::atomic<float> latest { 0.0f };
        std::array<std::atomic<float>, burstSize> burst {};
        std::atomic<int> burstCount { 0 };
    };

    static double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    static bool isMessageThread()
    {
        return juce::MessageManager::existsAndIsCurrentThread();
    }

    State* stateFor (int index) const
    {
        return index >= 0 && index < (int) states.size() ? states[(size_t) index].get() : nullptr;
    }

    bool editing (const State& state, double now) const
    {
        return state.gesture.load() || now - state.lastUserMs.load() < holdMs;
    }

    void parameterValueChanged (int index, float value) override
    {
        if (reverting)
            return;

        auto* state = stateFor (index);
        if (state == nullptr)
            return;

        const auto now = nowMs();

        if (isMessageThread())
        {
            // The user's edit (or a preset, undo, a learned CC: all of them
            // become the value to keep). A new burst starts once the last one lapsed.
            if (! editing (*state, now))
                state->burstCount = 0;
            const auto count = state->burstCount.load();
            state->burst[(size_t) (count % burstSize)] = value;
            state->burstCount = count + 1;
            state->latest = value;
            state->lastUserMs = now;
            return;
        }

        if (! editing (*state, now))
            return;

        const auto latest = state->latest.load();
        if (value == latest || ! isEarlierUserValue (*state, value))
            return;

        // An echo: put the user's latest value back. (Inside the host's own
        // callback JUCE's wrapper doesn't send this back to the host.)
        const juce::ScopedValueSetter<bool> guard (reverting, true);
        state->parameter->setValueNotifyingHost (latest);
        ++echoesBlocked;
    }

    void parameterGestureChanged (int index, bool starting) override
    {
        if (auto* state = stateFor (index))
        {
            if (starting && ! editing (*state, nowMs()))
                state->burstCount = 0;
            state->gesture = starting;
            if (! starting)
                state->lastUserMs = nowMs();
        }
    }

    static bool isEarlierUserValue (const State& state, float value)
    {
        const auto count = juce::jmin (state.burstCount.load(), burstSize);
        for (int i = 0; i < count; ++i)
            if (std::abs (state.burst[(size_t) i].load() - value) < 1.0e-7f)
                return true;
        return false;
    }

    std::vector<std::unique_ptr<State>> states;
    std::atomic<int> echoesBlocked { 0 };
    static inline thread_local bool reverting = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterEchoGuard)
};
