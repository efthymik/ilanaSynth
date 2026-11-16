#pragma once

#include <functional>
#include <string>
#include <vector>

// The editor's list of everything that can sit open over the page, in the order Esc
// closes them: the first entry is the top. Each entry is a closer that returns true
// when it found its overlay open and closed it, false when it was not open (so a new
// overlay is one registration here, not an edit to a chain). Entries that can be open
// in several places at once (held mod cards, the remap editor, tuning popovers) look
// for themselves in their closer and close what they find.
class PopupStack
{
public:
    // Registered top first. `name` is for tests and logs.
    void add (const char* name, std::function<bool()> tryClose)
    {
        entries.push_back ({ name, std::move (tryClose) });
    }

    // Closes the top-most open entry only. False when nothing was open.
    bool closeTop()
    {
        for (auto& e : entries)
        {
            if (e.tryClose != nullptr && e.tryClose())
            {
                lastClosedName = e.name;
                return true;
            }
        }

        return false;
    }

    // The entry the last closeTop() closed (empty before any).
    const std::string& lastClosed() const { return lastClosedName; }

    std::vector<std::string> names() const
    {
        std::vector<std::string> out;
        for (auto& e : entries)
            out.push_back (e.name);
        return out;
    }

private:
    struct Entry { std::string name; std::function<bool()> tryClose; };
    std::vector<Entry> entries;
    std::string lastClosedName;
};
