// KnobControl's geometry (ARCH-10 step 2): computeKnobGeometry (src/gui/KnobGeometry.h)
// tabulated for every layout mode at several cell sizes. Each row builds a real
// knob in its mode, lays it out in the cell and checks that
//   - the knob put its slider, name and value where the pure function says;
//   - every part is inside the cell, and the name and the value don't overlap the dial;
//   - the value's text box clears the dial and its rings by the checklist's 3 px
//     (stacked knobs; a packed value is inside its dial, an inline one beside it).
// ILANA_KNOB_GEOMETRY=1 prints the table instead of only checking it.
// Included by Snapshot.cpp after FxLfoUiTests.h; runs from runUiTests.
#pragma once

namespace KnobGeometryUi
{
struct Row
{
    const char* name;
    KnobLayoutMode mode;
    int cellW, cellH;
    bool modulatable;
    bool labelled;
    float packedDial;
    int maxDial;
    bool ringSpaced;
};

// The rows: each mode, with and without rings, at small / usual / roomy cells.
inline const std::vector<Row>& rows()
{
    static const std::vector<Row> table {
        { "stacked ring 48",        KnobLayoutMode::stacked,    64, 100, true,  true,  0.0f, 48, false },
        { "stacked ring 40",        KnobLayoutMode::stacked,    58,  90, true,  true,  0.0f, 40, false },
        { "stacked ring 36 narrow", KnobLayoutMode::stacked,    44,  82, true,  true,  0.0f, 36, false },
        { "stacked plain",          KnobLayoutMode::stacked,    64, 100, false, true,  0.0f, 48, false },
        { "stacked plain spaced",   KnobLayoutMode::stacked,    64, 100, false, true,  0.0f, 48, true  },
        { "stacked no name",        KnobLayoutMode::stacked,    64,  80, true,  false, 0.0f, 48, false },
        { "stacked tall cell",      KnobLayoutMode::stacked,    64, 160, true,  true,  0.0f, 48, false },
        { "compact",                KnobLayoutMode::compact,    36,  36, true,  false, 0.0f, 48, false },
        { "inline text",            KnobLayoutMode::inlineText, 120, 40, true,  true,  0.0f, 40, false },
        { "inline text plain",      KnobLayoutMode::inlineText, 120, 36, false, true,  0.0f, 36, false },
        { "inline knob",            KnobLayoutMode::inlineKnob, 110, 44, true,  true,  0.0f, 48, false },
        { "inline knob plain",      KnobLayoutMode::inlineKnob, 110, 44, false, true,  0.0f, 48, false },
        { "packed 26",              KnobLayoutMode::packed,     32,  60, true,  true,  26.0f, 48, false },
        { "packed 36",              KnobLayoutMode::packed,     42,  70, true,  true,  36.0f, 48, false },
        { "packed 48",              KnobLayoutMode::packed,     54,  80, true,  true,  48.0f, 48, false },
    };
    return table;
}

inline void checkRow (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor, const Row& row, bool print)
{
    const auto id = row.modulatable ? juce::String ("osc1_level") : juce::String ("macro1");
    KnobControl knob (processor.apvts, id, row.labelled ? "LEVEL" : "");
    editor.addAndMakeVisible (knob); // (the editor's look-and-feel, as a page's knob has)
    knob.setSizeRole (row.maxDial);
    knob.setMatchRingSpacing (row.ringSpaced);
    switch (row.mode)
    {
        case KnobLayoutMode::compact:    knob.setCompact (true); break;
        case KnobLayoutMode::inlineText: knob.setInline (true); break;
        case KnobLayoutMode::inlineKnob: knob.setInlineKnob (true); break;
        case KnobLayoutMode::packed:     knob.setPackedDial (row.packedDial); break;
        case KnobLayoutMode::stacked:    break;
    }
    knob.setBounds (0, 0, row.cellW, row.cellH);
    const juce::String tag = juce::String ("Knob geometry, ") + row.name + ": ";

    const auto& g = knob.getGeometry();
    const auto expected = computeKnobGeometry (knob.layoutSpec (knob.getLocalBounds()));
    expect (knob.layoutMode() == row.mode, tag + "the knob is in the mode the row asks for");
    expect (g.knob == expected.knob && g.slider == expected.slider && g.label == expected.label
                && g.inlineValue == expected.inlineValue && g.packedName == expected.packedName,
            tag + "resized() laid out what computeKnobGeometry says");

    const auto cell = knob.getLocalBounds();
    expect (cell.contains (g.knob) && cell.contains (g.slider), tag + "dial and slider are inside the cell");
    if (! g.label.isEmpty())
        expect (cell.contains (g.label), tag + "the name's label is inside the cell");
    if (! g.valueBand.isEmpty())
        expect (cell.contains (g.valueBand), tag + "the value's box is inside the cell");
    if (! g.label.isEmpty() && row.mode != KnobLayoutMode::inlineKnob)
        expect (! g.label.intersects (g.knob), tag + "the name doesn't overlap the dial's cell");

    // Clearance of the value from the dial and its rings.
    double gap = -1.0;
    if (row.mode == KnobLayoutMode::stacked && ! g.valueBand.isEmpty())
    {
        const auto centre = knob.getDialCentre();
        // The dial's lowest point, or the rings' (the sweep's ends sit 0.81 r below the centre, plus their stroke).
        const auto dialBottom = centre.y + knob.getDialRadius();
        const auto ringsBottom = knob.getRingsBounds().isEmpty() ? dialBottom : (double) knob.getRingsBounds().getBottom();
        gap = (double) g.valueBand.getY() - juce::jmax ((double) dialBottom, ringsBottom);
        // The checklist: the value ~3 px clear of the dial and its rings (every stacked row measures 4;
        // the text is centred in its 14 px box, so the ink is a little further).
        expect (gap >= 3.0, tag + "the value's box starts at least 3 px below the dial and its rings");
    }
    if (row.mode == KnobLayoutMode::packed)
    {
        const auto name = knob.getPackedNameArea();
        gap = (double) name.getY() - (knob.getDialCentre().y + knob.getDialRadius());
        expect (gap >= 0.0, tag + "the packed name's cell starts at the dial's bottom");
        expect (name.getBottom() <= knob.getHeight(), tag + "the packed name's cell is inside the knob");
    }
    if (row.mode == KnobLayoutMode::inlineText)
        expect (g.inlineValue.getX() - g.knob.getRight() >= KnobMetrics::inlineTextGap, tag + "the inline value is 6 px right of the dial");
    if (row.mode == KnobLayoutMode::inlineKnob)
        expect (g.label.getX() - g.knob.getRight() >= KnobMetrics::inlineKnobTextGap, tag + "the inline name is 3 px right of the dial");

    if (print)
    {
        std::cerr << "KNOBGEOM " << row.name << ": cell " << row.cellW << "x" << row.cellH << " knob " << g.knob.toString()
                  << " label " << g.label.toString() << " value " << g.valueBand.toString() << " dial r " << knob.getDialRadius()
                  << " gap " << gap << std::endl;
    }

    editor.removeChildComponent (&knob);
}
} // namespace KnobGeometryUi

inline void runKnobGeometryUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto print = juce::SystemStats::getEnvironmentVariable ("ILANA_KNOB_GEOMETRY", "").isNotEmpty();
    for (const auto& row : KnobGeometryUi::rows())
        KnobGeometryUi::checkRow (processor, editor, row, print);
}
