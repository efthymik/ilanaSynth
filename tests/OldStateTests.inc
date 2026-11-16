// Old saved states (tests/old-states): one patch per schema generation, loaded
// through the saved-patch path (loadPresetFromFile -> applyFullState) and
// compared with the parameter state and render the code gave before the
// migration table (ARCH-4). Plus a parameter hash of every factory preset
// (loadFactoryPreset's own migrations), which the fingerprints only hear.
//
// The corpus is made from today's factory presets (ILANA_OLDSTATE_MAKE=1):
// a preset's saved state, cut to what differs from a new patch, its schema
// stamps set to what a build of that generation wrote (from git history and
// the migrations' own dates), the parameters that generation did not have
// yet taken out, and a fixed overlay of old-style values so every migration
// has something to move. They are reconstructions, not files old builds
// wrote: what they check is that loading them stays exactly as it was.
//
// ILANA_OLDSTATE_TEST=1 runs this alone (it also runs in ILANA_PRESET_TEST
// and the full run). ILANA_OLDSTATE_WRITE=1 rewrites the expected values
// (only when a change of the result is intended). The render hashes are
// checked on Linux only (another compiler's floats differ in the last bits);
// elsewhere the parameter values are compared to 1e-5.

#include "dsp/LfoSim.h"

namespace OldStates
{
    struct Generation
    {
        const char* file;
        std::vector<std::pair<const char*, int>> stamps; // what that build's saved states carried
    };

    // In the order the builds were made. Before 2026-11-01 (the repository's
    // import) the stamps follow the migrations' comments; after it, the
    // commits named.
    inline const std::vector<Generation>& generations()
    {
        static const std::vector<Generation> list {
            { "g00-v1.0.xml", {} },
            { "g01-v1.1.xml", { { "osc3Schema", 2 }, { "tableSchema", 2 }, { "destSchema", 2 } } },
            { "g02-v1.3.xml", { { "osc3Schema", 2 }, { "tableSchema", 3 }, { "destSchema", 2 } } },
            { "g03-step19-tables.xml", { { "osc3Schema", 2 }, { "tableSchema", 4 }, { "destSchema", 2 } } },
            { "g04-step45-grainpos.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "tableSchema", 4 }, { "destSchema", 2 } } },
            { "g05-string-knobs.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "stringSchema", 2 }, { "tableSchema", 4 }, { "destSchema", 2 } } },
            { "g06-redundant-knobs.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "stringSchema", 2 }, { "redundantKnobsSchema", 2 },
                                           { "tableSchema", 4 }, { "destSchema", 2 } } },
            // 3aa8133d (2026-11-01, the import): Lorenz and mod ranges (2026-10-23) stamped too.
            { "g07-import-2026-11-01.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                             { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 } } },
            // 53a0c1a1 (2026-11-05): Dense board MIX, knob only.
            { "g08-boardmix2-2026-11-05.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                                { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                                { "boardMixSchema", 2 } } },
            // 82486f5b (2026-11-08): Dense board MIX with its routes.
            { "g09-boardmix3-2026-11-08.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                                { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                                { "boardMixSchema", 3 } } },
            // d80effec (2026-11-08, its own branch): GLIDE, on a boardMixSchema 2 base.
            { "g10-glide-boardmix2.xml", { { "glideSchema", 2 }, { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 },
                                           { "stringSchema", 2 }, { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 },
                                           { "tableSchema", 4 }, { "boardMixSchema", 2 } } },
            // bd8ba05c (2026-11-12): Dense board bypass at 0 %.
            { "g11-boardmix4-2026-11-12.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                                { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                                { "boardMixSchema", 4 }, { "glideSchema", 2 } } },
            // 737c9000 (2026-11-12): a guitarBodySchema no build reads any more.
            { "g12-guitarbody.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                      { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                      { "boardMixSchema", 3 }, { "glideSchema", 2 }, { "guitarBodySchema", 2 } } },
            // 3daa948d (2026-11-12): Classic board bypass at 0 %.
            { "g13-boardmix5-2026-11-12.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                                { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                                { "boardMixSchema", 5 }, { "glideSchema", 2 } } },
            // 45f9c4ca (2026-11-13, its own branch): body levels, on a boardMixSchema 3 base.
            { "g14-bodylevel-boardmix3.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                               { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                               { "boardMixSchema", 3 }, { "glideSchema", 2 }, { "bodyLevelSchema", 2 } } },
            // Today's stamps: nothing left to migrate but the operator modes.
            { "g15-current.xml", { { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
                                   { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
                                   { "boardMixSchema", 5 }, { "glideSchema", 2 }, { "bodyLevelSchema", 2 } } },
        };
        return list;
    }

    inline juce::File folder()
    {
        if (const auto dir = juce::SystemStats::getEnvironmentVariable ("ILANA_OLDSTATE_DIR", ""); dir.isNotEmpty())
            return juce::File (dir);
        return juce::File (ILANA_SOURCE_DIR).getChildFile ("tests/old-states");
    }

    inline int stampOf (const Generation& generation, const char* key)
    {
        for (const auto& [name, version] : generation.stamps)
            if (juce::String (name) == key)
                return version;
        return 1;
    }

    inline juce::String number (float value) { return juce::String::formatted ("%.9g", (double) value); }

    // Every parameter that differs from its default, one "id value" line each.
    inline juce::StringArray parameterLines (IlanaSynthAudioProcessor& processor)
    {
        juce::StringArray lines;
        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            {
                const auto value = processor.apvts.getRawParameterValue (ranged->paramID)->load();
                const auto fallback = ranged->convertFrom0to1 (ranged->getDefaultValue());
                if (value != fallback)
                    lines.add (ranged->paramID + " " + number (value));
            }
        return lines;
    }

    inline juce::uint64 fnv (juce::uint64 hash, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const juce::uint8*> (data);
        for (size_t i = 0; i < size; ++i)
            hash = (hash ^ bytes[i]) * 1099511628211ull;
        return hash;
    }

    inline juce::uint64 parameterHash (IlanaSynthAudioProcessor& processor)
    {
        auto hash = 14695981039346656037ull;
        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            {
                const auto value = processor.apvts.getRawParameterValue (ranged->paramID)->load();
                hash = fnv (hash, ranged->paramID.toRawUTF8(), (size_t) ranged->paramID.getNumBytesAsUTF8());
                hash = fnv (hash, &value, sizeof (value));
            }
        return hash;
    }

    // A chord for 0.5 s and its release, hashed bit for bit.
    inline juce::uint64 renderHash (IlanaSynthAudioProcessor& processor)
    {
        auto hash = 14695981039346656037ull;
        juce::AudioBuffer<float> buffer (2, 256);
        for (int block = 0; block < 140; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 60, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == 94)
                for (const auto note : { 48, 60, 67 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                hash = fnv (hash, buffer.getReadPointer (c), 256 * sizeof (float));
        }
        return hash;
    }

    inline juce::String hex (juce::uint64 value) { return juce::String::toHexString ((juce::int64) value).paddedLeft ('0', 16); }

    // What loading one corpus file gives: "render <hash>" then the parameter lines.
    inline juce::StringArray loadResult (const juce::File& file)
    {
        auto processor = std::make_unique<IlanaSynthAudioProcessor>();
        processor->setNonRealtime (true);
        processor->prepareToPlay (48000.0, 256);
        juce::StringArray lines;
        if (! processor->loadPresetFromFile (file))
        {
            lines.add ("unreadable");
            return lines;
        }
        processor->flushAsyncUpdates();
        const auto parameters = parameterLines (*processor);
        lines.add ("render " + hex (renderHash (*processor)));
        lines.addArray (parameters);
        return lines;
    }

    inline juce::StringArray factoryResult()
    {
        juce::StringArray lines;
        auto processor = std::make_unique<IlanaSynthAudioProcessor>();
        processor->keepAsyncUpdatesOnThisThread();
        const auto names = processor->getFactoryPresetNames();
        for (int i = 0; i < names.size(); ++i)
        {
            processor->loadFactoryPreset (i);
            processor->flushAsyncUpdates();
            lines.add (hex (parameterHash (*processor)) + " " + names[i]);
        }
        return lines;
    }

    // ILANA_OLDSTATE_MAKE=1: the corpus (see the top of this file).
    inline void makeCorpus()
    {
        const auto dir = folder();
        dir.createDirectory();
        const auto& presets = Presets::getFactoryPresets();

        const auto defaults = std::make_unique<IlanaSynthAudioProcessor>()->captureWholePatch();

        // Presets that render on their own (no bounce, no saved file), spread over the library.
        std::vector<int> candidates;
        for (int i = 1; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].bounces.empty() && presets[(size_t) i].savedState == nullptr)
                candidates.push_back (i);

        const auto& list = generations();
        for (size_t g = 0; g < list.size(); ++g)
        {
            const auto& generation = list[g];
            const auto presetIndex = candidates[(g * 37 + 5) % candidates.size()];
            auto processor = std::make_unique<IlanaSynthAudioProcessor>();
            processor->keepAsyncUpdatesOnThisThread();
            processor->loadFactoryPreset (presetIndex);
            processor->flushAsyncUpdates();
            auto state = processor->captureWholePatch();

            // Properties as a new patch has them go (a stamp is set below).
            for (int i = state.getNumProperties(); --i >= 0;)
            {
                const auto name = state.getPropertyName (i);
                if (name.toString().endsWith ("Schema")
                    || (defaults.hasProperty (name) && defaults.getProperty (name) == state.getProperty (name)))
                    state.removeProperty (name, nullptr);
            }
            state.setProperty ("build", "old-state corpus " + juce::String (generation.file).upToLastOccurrenceOf (".", false, false), nullptr);
            for (const auto& [name, version] : generation.stamps)
                state.setProperty (name, version, nullptr);

            const auto parameterDefault = [&processor] (const juce::String& id, float& value)
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (processor->apvts.getParameter (id)))
                {
                    value = ranged->convertFrom0to1 (ranged->getDefaultValue());
                    return true;
                }
                return false;
            };

            // Only what differs from a new patch, and not what that generation lacked.
            for (int i = state.getNumChildren(); --i >= 0;)
            {
                const auto child = state.getChild (i);
                if (! child.hasType ("PARAM"))
                    continue;
                const auto id = child.getProperty ("id").toString();
                float fallback = 0.0f;
                const auto isDefault = parameterDefault (id, fallback) && (float) child.getProperty ("value") == fallback;
                const auto tooNew = (stampOf (generation, "grainPosSchema") < 2 && id.endsWith ("_grain_pos"))
                                    || (stampOf (generation, "bodyLevelSchema") < 2 && id == "body_legacy_level");
                if (isDefault || tooNew)
                    state.removeChild (i, nullptr);
            }

            // Old-style values for every migration to move (the knobs the
            // redundant-knobs step removed are plain saved values).
            const auto put = [&state] (const juce::String& id, float value)
            {
                for (int i = 0; i < state.getNumChildren(); ++i)
                    if (state.getChild (i).getProperty ("id").toString() == id)
                    {
                        state.getChild (i).setProperty ("value", value, nullptr);
                        return;
                    }
                state.appendChild (juce::ValueTree ("PARAM", { { "id", id }, { "value", value } }), nullptr);
            };
            const auto source = (float) (int) Mod::Source::Lfo1;
            const auto odd = (g % 2) == 1;
            put ("sub_on", 1.0f);
            put ("sub_table", odd ? 2.0f : 1.0f);
            put ("sub_octave", -1.0f);
            put ("sub_level", 0.4f);
            put ("osc1_table", odd ? 45.0f : 125.0f);
            put ("osc2_table", odd ? 130.0f : 18.0f);
            put ("osc1_sample_start", 0.25f);
            put ("lfo2_shape", (float) LfoSimShapes::Lorenz);
            put ("lfo2_sync", 0.0f);
            put ("lfo2_rate", 1.5f);
            put ("lfo2_p2", 0.7f);
            put ("glide", odd ? 0.05f : 0.2f);
            put ("f1_env", 2.0f);
            put ("filter_velocity", 0.3f);
            put ("amp_curve", 0.3f);
            put ("osc2_pd_env", 1.0f);
            put ("osc2_pd_env_amt", 0.6f);
            put ("sb_model", odd ? 1.0f : 0.0f);
            put ("sb_mix", 0.4f);
            put ("body_type", odd ? 2.0f : 5.0f);
            put ("mod61_src", source);
            put ("mod61_dst", (float) (Mod::explicitDestinationsV10 + 3));
            put ("mod61_amt", 0.4f);
            put ("mod62_src", source);
            put ("mod62_dst", (float) Mod::destinationForParamId ("sb_mix"));
            put ("mod62_amt", 0.5f);
            put ("mod63_src", source);
            put ("mod63_dst", (float) Mod::destinationForParamId ("glide"));
            put ("mod63_amt", 0.3f);
            put ("mod64_src", source);
            put ("mod64_dst", (float) Mod::destinationForParamId ("f1_cutoff"));
            put ("mod64_amt", 0.25f);

            const auto file = dir.getChildFile (generation.file);
            if (auto xml = state.createXml())
                xml->writeTo (file);
            std::cout << "wrote " << file.getFullPathName() << " (" << presets[(size_t) presetIndex].name << ", "
                      << file.getSize() << " bytes)" << std::endl;
        }
    }

    inline bool sameLines (const juce::StringArray& expected, const juce::StringArray& actual, juce::String& firstDifference)
    {
#if JUCE_LINUX
        constexpr auto exact = true;
#else
        constexpr auto exact = false;
#endif
        juce::StringArray want, got;
        for (const auto& line : expected)
            if (exact || ! line.startsWith ("render "))
                want.add (line);
        for (const auto& line : actual)
            if (exact || ! line.startsWith ("render "))
                got.add (line);

        for (int i = 0; i < juce::jmax (want.size(), got.size()); ++i)
        {
            const auto a = want[i], b = got[i];
            if (a == b)
                continue;
            const auto idA = a.upToFirstOccurrenceOf (" ", false, false), idB = b.upToFirstOccurrenceOf (" ", false, false);
            const auto valueA = a.fromFirstOccurrenceOf (" ", false, false).getDoubleValue();
            const auto valueB = b.fromFirstOccurrenceOf (" ", false, false).getDoubleValue();
            if (! exact && idA == idB && std::abs (valueA - valueB) <= 1.0e-5 * juce::jmax (1.0, std::abs (valueA)))
                continue;
            firstDifference = "expected \"" + a + "\", got \"" + b + "\"";
            return false;
        }
        return true;
    }
}

void runOldStateTests()
{
    fixedRandomSeeds() = true;
    struct Restore { ~Restore() { fixedRandomSeeds() = false; } } restore;

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_OLDSTATE_MAKE", "").isNotEmpty())
        OldStates::makeCorpus();

    const auto write = juce::SystemStats::getEnvironmentVariable ("ILANA_OLDSTATE_WRITE", "").isNotEmpty();
    const auto dir = OldStates::folder();

    const auto compare = [write] (const juce::File& expectedFile, const juce::StringArray& actual, const juce::String& what)
    {
        if (write)
        {
            expectedFile.replaceWithText (actual.joinIntoString ("\n") + "\n", false, false, "\n");
            std::cout << "wrote " << expectedFile.getFullPathName() << std::endl;
            return;
        }
        juce::StringArray expected;
        expected.addLines (expectedFile.loadFileAsString());
        expected.removeEmptyStrings();
        juce::String difference;
        check (expectedFile.existsAsFile() && OldStates::sameLines (expected, actual, difference),
               "old states: " + what + " loads as before" + (difference.isNotEmpty() ? " (" + difference + ")" : juce::String()));
    };

    for (const auto& generation : OldStates::generations())
    {
        const auto file = dir.getChildFile (generation.file);
        if (! file.existsAsFile())
        {
            check (false, "old states: " + file.getFullPathName() + " exists");
            continue;
        }
        compare (file.withFileExtension ("expected"), OldStates::loadResult (file), generation.file);
    }

    compare (dir.getChildFile ("factory-presets.expected"), OldStates::factoryResult(), "every factory preset's parameters");
}
