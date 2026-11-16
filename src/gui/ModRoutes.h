#pragma once

#include <algorithm>
#include <initializer_list>
#include <vector>

#include "../PluginProcessor.h"

// The mod slots a source takes part in (as the source or the VIA aux), and
// a one-step removal of those routes: a slot the source drives is cleared,
// one it only scales (VIA) loses its aux.
inline std::vector<int> modSlotsUsing (const IlanaSynthAudioProcessor& processor, std::initializer_list<Mod::Source> sources)
{
    std::vector<int> slots;

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.destination == 0)
            continue;

        for (const auto source : sources)
            if (routing.source == source || routing.aux == source)
            {
                slots.push_back (slot);
                break;
            }
    }

    return slots;
}

inline void removeModRoutes (IlanaSynthAudioProcessor& processor, std::initializer_list<Mod::Source> sources)
{
    for (const auto slot : modSlotsUsing (processor, sources))
    {
        const auto routing = processor.readModSlot (slot);
        const auto drives = std::find (sources.begin(), sources.end(), routing.source) != sources.end();

        if (drives)
            processor.clearModSlot (slot);
        else
            processor.setModSlotValue (slot, "aux", 0.0f);
    }
}
