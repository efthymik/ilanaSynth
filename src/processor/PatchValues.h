#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

// The values a migration reads and writes: a saved state's PARAM children
// (TreePatchValues) or a factory recipe's (id, value) list
// (FactoryPatchValues). get returns the fallback for an id the patch does
// not hold; what set does with such an id is the adapter's MissingId.
// Both index the ids once, when they are made, and keep the index up to
// date as set adds entries; anything that changes the patch's structure
// some other way needs a new adapter.
class PatchValues
{
public:
    // Kept apart on purpose: each migration was written against one of these,
    // and an id it sets that the patch lacks must stay added or skipped as it was.
    enum class MissingId { add, skip };

    virtual ~PatchValues() = default;
    virtual float get (const juce::String& id, float fallback) const = 0;
    virtual bool contains (const juce::String& id) const = 0;
    virtual void set (const juce::String& id, float value) = 0;

    void setMissing (MissingId mode) { missing = mode; }

    // For the migrations' std::function getter and setter.
    std::function<float (const juce::String&, float)> getter() const
    {
        return [this] (const juce::String& id, float fallback) { return get (id, fallback); };
    }
    std::function<void (const juce::String&, float)> setter()
    {
        return [this] (const juce::String& id, float value) { set (id, value); };
    }

    // The saved state the values are in, for migrations that work on the tree
    // itself (nullptr for a factory recipe).
    virtual juce::ValueTree* tree() { return nullptr; }

protected:
    explicit PatchValues (MissingId mode) : missing (mode) {}
    MissingId missing;
};

// A saved state: the first child with each "id" (of any type, as the old
// find lambdas walked them), its "value" property.
class TreePatchValues final : public PatchValues
{
public:
    explicit TreePatchValues (juce::ValueTree& stateToUse, MissingId mode = MissingId::skip)
        : PatchValues (mode), state (stateToUse)
    {
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            const auto child = state.getChild (i);
            children.emplace (child.getProperty ("id").toString(), child); // the first one wins
        }
    }

    juce::ValueTree find (const juce::String& id) const
    {
        const auto it = children.find (id);
        return it != children.end() ? it->second : juce::ValueTree();
    }

    // The child for id, appended (a PARAM with only its id) if the state has none.
    juce::ValueTree findOrAdd (const juce::String& id)
    {
        auto child = find (id);
        if (! child.isValid())
        {
            child = juce::ValueTree ("PARAM");
            child.setProperty ("id", id, nullptr);
            state.appendChild (child, nullptr);
            children.emplace (id, child);
        }
        return child;
    }

    bool contains (const juce::String& id) const override { return find (id).isValid(); }

    float get (const juce::String& id, float fallback) const override
    {
        const auto child = find (id);
        return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
    }

    void set (const juce::String& id, float value) override
    {
        auto child = missing == MissingId::add ? findOrAdd (id) : find (id);
        if (child.isValid())
            child.setProperty ("value", value, nullptr);
    }

    juce::ValueTree* tree() override { return &state; }

private:
    juce::ValueTree& state;
    std::unordered_map<juce::String, juce::ValueTree> children;
};

// A factory recipe's values. A recipe can hold an id twice (a voicing line
// over the recipe's own): get reads the first; set writes the first, or with
// Duplicates::all every one of them, as the old loops that kept going did.
class FactoryPatchValues final : public PatchValues
{
public:
    using Values = std::vector<std::pair<juce::String, float>>;
    enum class Duplicates { first, all };

    explicit FactoryPatchValues (Values& valuesToUse, MissingId mode = MissingId::add, Duplicates duplicatesToUse = Duplicates::first)
        : PatchValues (mode), values (valuesToUse), duplicates (duplicatesToUse)
    {
        for (size_t i = 0; i < values.size(); ++i)
            entries[values[i].first].push_back (i);
    }

    void setDuplicates (Duplicates mode) { duplicates = mode; }

    bool contains (const juce::String& id) const override { return entries.count (id) > 0; }

    float get (const juce::String& id, float fallback) const override
    {
        const auto it = entries.find (id);
        return it != entries.end() ? values[it->second.front()].second : fallback;
    }

    void set (const juce::String& id, float value) override
    {
        if (const auto it = entries.find (id); it != entries.end())
        {
            if (duplicates == Duplicates::all)
                for (const auto index : it->second)
                    values[index].second = value;
            else
                values[it->second.front()].second = value;
        }
        else if (missing == MissingId::add)
        {
            entries[id].push_back (values.size());
            values.push_back ({ id, value });
        }
    }

private:
    Values& values;
    Duplicates duplicates;
    std::unordered_map<juce::String, std::vector<size_t>> entries;
};
