// What the pages share: addAll, the section and box titles, the heading
// constants and PillToggle. Global inline code, as the widget headers are,
// so a page in its own .cpp and the pages still in PluginEditor.cpp share it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../FilterWidgets.h"
#include "../IlanaLookAndFeel.h"

template <typename... Components>
void addAll (juce::Component& parent, Components&... components)
{
    (parent.addAndMakeVisible (components), ...);
}

// A page section's heading (a display, a pool, the rack): the title in the
// label colour, with no rule under it (an underline reads as a selected tab: V11-13) and no dot (review 6: a dot
// reads as a power light, and on cards it means a module's colour beside a
// real switch). The text starts where a card's tag does, so headings line
// up with the cards under them.
inline void paintSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                        const juce::String& subtitle = {})
{
    // (A page's title, the cards' title style: UI review 31, item 39.)
    const auto font = juce::Font (IlanaTheme::cardTitleFont());
    const auto width = juce::GlyphArrangement::getStringWidthInt (font, text);
    g.setColour (IlanaTheme::Ui::text);
    g.setFont (font);
    g.drawText (text, area, juce::Justification::centredLeft);

    if (subtitle.isEmpty())
        return;

    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
    const auto room = area.withTrimmedLeft (width + 16);
    g.drawText (IlanaTheme::fittedHint (IlanaTheme::captionFragment (subtitle), juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label)), (float) room.getWidth()),
                room, juce::Justification::centredLeft, false);
}

// The one title of a box inside a card (SEQ's GENERATE parts, VECTOR's
// boxes, PHYSICAL's BODY and SOUNDBOARD; review 10, I10-7): the name in the
// label font, a quiet state word after it, and the box's switch at the right
// (reserveRight leaves its room).
inline void paintSubBoxTitle (juce::Graphics& g, juce::Rectangle<int> header, const juce::String& title, const juce::String& state,
                       bool on, int reserveRight = 0, juce::Colour tint = {})
{
    header.removeFromRight (reserveRight);
    // (The inner title style, grey whatever the box's colour: UI review 31, item 39.)
    juce::ignoreUnused (tint, on);
    const auto font = juce::Font (IlanaTheme::innerTitleFont());
    const auto width = juce::GlyphArrangement::getStringWidthInt (font, title);
    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (font);
    g.drawText (title, header, juce::Justification::centredLeft);

    if (state.isEmpty())
        return;

    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
    g.drawText (state, header.withTrimmedLeft (width + 8), juce::Justification::centredLeft, true);
}

// Page headings sit where a card's title does: 12 px in from the card edge
// (cards start 12 px in from the page), 26 px tall like a card's header.
inline constexpr int headingX = 24, headingHeight = 26;

// An on/off as one pill (the LFO's SYNC, RETRIG and KEY, on PLAY and in its pop-up): solid while on.
class PillToggle : public ParamBoundComponent
{
public:
    PillToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text, juce::Colour colourIn, const juce::String& tip)
        : ParamBoundComponent (state, id), label (text), colour (colourIn)
    {
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto on = current > 0;
        g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, isMouseOver() ? 0.12f : 0.0f));
        const auto radius = bounds.getHeight() * 0.5f;
        g.fillRoundedRectangle (bounds, radius);
        if (! on)
        {
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        }
        g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
        g.drawText (label, bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { setValue (current > 0 ? 0 : 1); }

    juce::String getDisplayText() const override { return label; }

    void setText (const juce::String& text)
    {
        if (text != label)
        {
            label = text;
            repaint();
        }
    }

    // The text's width, so a row of pills can be sized to what they say.
    float textWidth() const { return juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true)), label) + 2.0f; }

private:
    juce::String label;
    juce::Colour colour;
};
