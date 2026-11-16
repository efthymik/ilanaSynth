#pragma once

// KnobGeometry (ARCH-10 step 2): where a KnobControl's parts go in each layout
// mode, as one pure function of the cell and the knob's settings. It touches
// no component, font or look-and-feel, so a test can tabulate it (see
// tools/KnobGeometryUiTests.h). KnobControl::resized() builds a
// KnobLayoutSpec from its state, calls computeKnobGeometry() and applies the
// result; nothing else in the class decides a part's place.
//
// The modes (a knob is in exactly one, tested in this order):
//   packed     dial with its value inside it and its name under it (setPackedDial)
//   inlineText compact dial with name and value stacked at its right (setInline)
//   compact    the bare dial, the owner draws name and value (setCompact)
//   inlineKnob dial at the left, name above value to its right (setInlineKnob)
//   stacked    name, dial, value one under the other (the default)

#include <juce_graphics/juce_graphics.h>

#include "IlanaLookAndFeel.h"

enum class KnobLayoutMode { stacked, compact, inlineText, inlineKnob, packed };

namespace KnobMetrics
{
    // The room a packed knob keeps round its dial for the modulation rings
    // (KnobControl::packedPadX / packedPadTop).
    inline constexpr int packedPadX = 3, packedPadTop = 6;
    // The packed name's cell reaches this far past the dial's cell each side.
    inline constexpr int packedNameOverhang = 4;
    // The stacked value's text box: its height, and the extra room a ring-spaced knob keeps under the dial.
    inline constexpr int valueBoxHeight = 14, valueRowHeight = 16, ringSpacedExtra = 3;
    inline constexpr int valueBoxMinWidth = 64, valueBoxMaxWidth = 80;
    // Gap between an inline dial and its name / value.
    inline constexpr int inlineTextGap = 6, inlineKnobTextGap = 3;
    // The inline-text block: a 13 px name over a 14 px value.
    inline constexpr int inlineNameHeight = 13, inlineValueHeight = 14;
    // A ring-drawing dial in an inline cell is capped to this much less than its half size.
    inline constexpr float inlineRingCapInset = 4.0f, inlineRingCapFloor = 9.0f;
}

struct KnobLayoutSpec
{
    juce::Rectangle<int> bounds;  // the knob's local bounds
    KnobLayoutMode mode = KnobLayoutMode::stacked;
    float packedDial = 0.0f;      // packed: the dial's size
    int maxDial = 48;             // the role's largest dial (IlanaTheme::KnobSize)
    int inlineDial = 40;          // inlineKnob: the dial's largest size
    bool modulatable = false;     // draws modulation rings (a destination)
    bool capDialLikeRings = false; // inlineKnob: cap as a ring-drawing dial does
    bool ringSpaced = false;      // stacked: keeps the ring knobs' room under the dial
    bool hasLabel = false;
    int labelBlockHeight = 0;     // stacked: the name's line plus its air (0 without a name)
    int nameLineHeight = 13;      // the name's own line
};

struct KnobGeometry
{
    juce::Rectangle<int> knob;       // the dial's cell (KnobControl::knobBounds)
    juce::Rectangle<int> slider;     // the slider's bounds
    juce::Rectangle<int> label;      // the name's label (empty: none)
    juce::Rectangle<int> inlineValue; // inlineText: where the value is drawn
    juce::Rectangle<int> packedName; // packed: the name's area
    juce::Rectangle<int> valueBand;  // stacked: where the value's text box sits (bottom of the slider)
    float dialRadiusCap = 0.0f;      // > 0: the dial draws no bigger than this radius
    int valueBoxWidth = 0;           // stacked: the text box's width
    int valueRow = 0;                // stacked: the room under the dial for the value
};

inline KnobGeometry computeKnobGeometry (const KnobLayoutSpec& spec)
{
    using namespace KnobMetrics;
    KnobGeometry g;
    auto area = spec.bounds;

    if (spec.mode == KnobLayoutMode::packed)
    {
        // The cell inside the room for the rings: the dial at its top, the name under it.
        const auto cell = area.reduced (packedPadX, 0).withTrimmedTop (packedPadTop);
        const auto dial = juce::roundToInt (spec.packedDial);
        g.knob = juce::Rectangle<int> (dial, dial).withPosition (cell.getCentreX() - dial / 2, cell.getY());
        g.slider = g.knob;
        g.packedName = { cell.getX() - packedPadX - packedNameOverhang, g.knob.getBottom(),
                         cell.getWidth() + 2 * packedPadX + 2 * packedNameOverhang, juce::jmax (0, cell.getBottom() - g.knob.getBottom()) };
        return g;
    }

    if (spec.mode == KnobLayoutMode::inlineText)
    {
        const auto dial = juce::jmin (area.getHeight(), spec.maxDial);

        // (As an inline knob: a modulatable dial a little smaller, so a ring fits the cell.)
        if (spec.modulatable)
            g.dialRadiusCap = juce::jmax (inlineRingCapFloor, (float) dial * 0.5f - inlineRingCapInset);
        g.knob = area.removeFromLeft (dial).withSizeKeepingCentre (dial, dial);
        g.slider = g.knob;
        area.removeFromLeft (inlineTextGap);
        const auto top = area.getY() + (area.getHeight() - (inlineNameHeight + inlineValueHeight)) / 2;
        g.label = { area.getX(), top, area.getWidth(), inlineNameHeight };
        g.inlineValue = { area.getX(), top + inlineNameHeight, area.getWidth(), inlineValueHeight };
        return g;
    }

    if (spec.mode == KnobLayoutMode::compact)
    {
        g.knob = area;
        g.slider = area;
        return g;
    }

    if (spec.mode == KnobLayoutMode::inlineKnob)
    {
        // The dial is a square as high as the cell; the name above the
        // value, level with the dial's middle line, to its right.
        const auto dial = juce::jmin (area.getHeight(), spec.inlineDial);
        // A modulatable dial in a low cell draws a little smaller, so at
        // least one ring fits round it inside the cell.
        if (spec.modulatable || spec.capDialLikeRings)
            g.dialRadiusCap = juce::jmax (inlineRingCapFloor, (float) dial * 0.5f - inlineRingCapInset);
        g.knob = area.withWidth (dial).withSizeKeepingCentre (dial, dial);
        g.slider = area;
        const auto textX = g.knob.getRight() + inlineKnobTextGap;
        g.label = { textX, area.getCentreY() - inlineNameHeight, juce::jmax (0, area.getRight() - textX), inlineNameHeight };
        return g;
    }

    // Stacked: label, dial and value as one tight group, so the gaps between
    // them are the same whatever size cell a page gives the knob. The group sits
    // at the top, where combo and toggle labels in the same row sit; a knob
    // without a label (a matrix cell) is centred instead. A knob that draws
    // rings keeps 3 px more between its arcs and the value.
    g.valueRow = valueRowHeight + (spec.ringSpaced ? ringSpacedExtra : 0);
    const auto labelHeight = spec.labelBlockHeight;
    g.valueBoxWidth = juce::jlimit (valueBoxMinWidth, valueBoxMaxWidth, area.getWidth());
    const auto dial = juce::jlimit (IlanaTheme::KnobSize::minimum, spec.maxDial,
                                    juce::jmin (area.getWidth(), area.getHeight() - (spec.hasLabel ? labelHeight : 0) - g.valueRow));
    const auto groupHeight = juce::jmin (area.getHeight(), (spec.hasLabel ? labelHeight : 0) + dial + g.valueRow);
    auto group = spec.hasLabel ? area.removeFromTop (groupHeight) : area.withSizeKeepingCentre (area.getWidth(), groupHeight);

    g.label = spec.hasLabel ? group.removeFromTop (spec.nameLineHeight) : juce::Rectangle<int>();
    if (spec.hasLabel)
        group.removeFromTop (labelHeight - spec.nameLineHeight);
    g.knob = group;
    g.slider = group;
    g.valueBand = group.removeFromBottom (valueBoxHeight);
    return g;
}
