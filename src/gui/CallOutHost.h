#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Parent call-outs inside the GPU-rendered subtree.
namespace CallOutHost
{
inline juce::Component* parentFor (juce::Component& target)
{
    for (auto* host = &target; host != nullptr; host = host->getParentComponent())
        if (host->getCachedComponentImage() != nullptr)
            return host;

    return target.getTopLevelComponent();
}
} // namespace CallOutHost
