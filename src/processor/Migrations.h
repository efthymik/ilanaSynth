#pragma once

#include "PatchValues.h"

#include <vector>

// The migrations that keep old patches sounding as they were heard: one
// table, run in its order by both loads (applyFullState for a saved state,
// loadFactoryPreset for a factory recipe). To add a migration: one row at the
// end of table() (src/processor/State.cpp) and its stamp's version in
// savedSchemas (State.cpp). See docs/ARCHITECTURE.md, "Migrations".
namespace Migrations
{
    // Where in a saved state's load a row runs.
    enum class SavedStage
    {
        none,           // not on saved states
        beforeDefaults, // on the state as saved, before the patch tables, samples and the DX7 import
        afterDefaults,  // after every parameter the state lacks was filled in from its default
    };

    // Where in a factory preset's load a row runs.
    enum class FactoryStage
    {
        none,          // not on factory recipes
        legacyRecipes, // the original 80 recipes only (no category), before the voicing
        afterTrims,    // every recipe, after its voicing, trims and level
    };

    struct Row
    {
        // The stamp a saved state carries (nullptr: no stamp, the row runs on
        // every load). A state whose stamp (1 if it has none) is below
        // toVersion gets the row and is stamped toVersion. A factory recipe
        // starts at version 1 of every key.
        const char* schemaKey;
        int fromVersion;
        int toVersion;
        // stamp: the key's version before this row (saved: as loaded and
        // stamped by the rows above; factory: 1, or the toVersion of the row
        // above with the same key that runs on recipes).
        void (*run) (PatchValues& values, int stamp);

        SavedStage saved;
        PatchValues::MissingId savedMissing; // what set does with an id the state lacks
        FactoryStage factory;
        PatchValues::MissingId factoryMissing;
        FactoryPatchValues::Duplicates factoryDuplicates; // a recipe that holds an id twice
    };

    const std::vector<Row>& table();

    // The rows of one stage on a saved state, which they stamp.
    void runSaved (juce::ValueTree& state, SavedStage stage);
    // The rows of one stage on a factory recipe's values.
    void runFactory (FactoryPatchValues::Values& values, FactoryStage stage);
}
