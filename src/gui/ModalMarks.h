#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

// Real instruments on the modal exciters' SIZE and SCALE knobs (ilana, 2026-11-12: "have the knobs for guitar
// size ... and in the violin ... snap to the real instrument sizes, if u wanna adjust more precisely near the
// notches, you can do it by holding alt while draggin").
//
// Each mark is the instrument's size against the one the engine was fitted to (x1). SIZE is the body's
// linear size (bowed: body length, as the engine's violin x1 / viola x1.17 / cello x2.1; guitar: the square
// root of body length times lower bout, from the makers' specs; piano: the case length, Piano V's American
// grand = a 274 cm concert grand). SCALE is the speaking length of the string (guitar: scale length
// against 650 mm; bowed: string length against the violin's 328 mm; piano: the longest bass string).
// The knobs carry them as ring ticks ("ringMarks") that a drag snaps to (KnobSlider) unless Alt is held, and
// the instrument picture names them. Display only: the engine and presets are unchanged.
namespace ModalMarks
{
struct Mark
{
    float value;
    const char* name;
};

// excite 11 Modal Piano, 12 Modal Guitar, 13 Modal Bowed; `scale` picks SCALE over SIZE.
inline const std::vector<Mark>& marks (int excite, bool scale)
{
    static const std::vector<Mark> none;
    static const std::vector<Mark> guitarSize { { 0.47f, "SOPRANO UKE" }, { 0.63f, "TENOR UKE" }, { 0.83f, "REQUINTO" },
                                                { 0.96f, "PARLOUR" },     { 1.0f, "CLASSICAL" },  { 1.07f, "DREADNOUGHT" },
                                                { 1.13f, "JUMBO" },       { 1.3f, "GUITARRON" } };
    static const std::vector<Mark> guitarScale { { 0.51f, "SOPRANO UKE" }, { 0.66f, "TENOR UKE" }, { 0.83f, "REQUINTO" },
                                                 { 1.0f, "CLASSICAL" },    { 1.1f, "BARITONE" } };
    // (The engine's measured bodies (PR #259): pochette 0.6, 3/4 violin / violino piccolo 0.8, and the fitted violin,
    // viola and cello exactly as the Modal Violin / Viola / Cello presets; the double bass 3.1.)
    static const std::vector<Mark> bowedSize { { 0.6f, "POCHETTE" }, { 0.8f, "3/4 VIOLIN" }, { 1.0f, "VIOLIN" }, { 1.17f, "VIOLA" },
                                               { 2.1f, "CELLO" },     { 3.1f, "DOUBLE BASS" } };
    static const std::vector<Mark> bowedScale { { 0.94f, "3/4 VIOLIN" }, { 1.0f, "VIOLIN" }, { 1.13f, "VIOLA" }, { 2.0f, "CELLO" } };
    static const std::vector<Mark> pianoSize { { 0.55f, "UPRIGHT" }, { 0.57f, "BABY GRAND" }, { 0.77f, "MODEL B" },
                                               { 1.0f, "CONCERT GRAND" }, { 1.12f, "F308" } };
    static const std::vector<Mark> pianoScale { { 0.58f, "UPRIGHT" }, { 0.65f, "BABY GRAND" }, { 0.85f, "MODEL B" },
                                                { 1.0f, "CONCERT GRAND" }, { 1.1f, "F308" } };
    switch (excite)
    {
        case 11: return scale ? pianoScale : pianoSize;
        case 12: return scale ? guitarScale : guitarSize;
        case 13: return scale ? bowedScale : bowedSize;
        default: return none;
    }
}

// The slider properties KnobSlider and the look-and-feel read: the values, and their names in the same order.
inline juce::String ringMarks (int excite, bool scale)
{
    juce::StringArray values;
    for (const auto& mark : marks (excite, scale))
        values.add (juce::String (mark.value));
    return values.joinIntoString (",");
}

inline juce::String ringMarkNames (int excite, bool scale)
{
    juce::StringArray names;
    for (const auto& mark : marks (excite, scale))
        names.add (mark.name);
    return names.joinIntoString (",");
}

// The mark this value sits on (a snapped drag lands exactly on it), or null.
inline const Mark* markAt (int excite, bool scale, float value)
{
    for (const auto& mark : marks (excite, scale))
        if (std::abs (std::log2 (value / mark.value)) < 0.005f)
            return &mark;
    return nullptr;
}

// What this SIZE / SCALE is called: the instrument at its mark, else the two it sits between
// (or SMALL / BIG past the ends). Empty for an engine without marks.
inline juce::String name (int excite, bool scale, float value)
{
    const auto& list = marks (excite, scale);
    if (list.empty() || ! (value > 0.0f))
        return {};
    if (const auto* mark = markAt (excite, scale, value))
        return mark->name;
    if (value < list.front().value)
        return "SMALL " + juce::String (list.front().name);
    if (value > list.back().value)
        return "BIG " + juce::String (list.back().name);
    for (size_t i = 1; i < list.size(); ++i)
        if (value < list[i].value)
            return juce::String (list[i - 1].name) + juce::String::fromUTF8 (" \xe2\x80\xba ") + list[i].name;
    return {};
}
} // namespace ModalMarks
