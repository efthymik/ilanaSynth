// What the pages share: addAll, the section and box titles, the heading
// constants, PillToggle and PillChoice. Global inline code, as the widget headers are,
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

// A small selector of two or three solid-fill pills (the slope, SUB's octave).
class PillChoice : public ParamBoundComponent
{
public:
    PillChoice (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray labelsIn, juce::Colour colourIn, const juce::String& tip, std::vector<int> valuesIn = {})
        : ParamBoundComponent (state, id), labels (std::move (labelsIn)), colour (colourIn), values (std::move (valuesIn))
    {
        if (values.empty())
            for (int i = 0; i < labels.size(); ++i)
                values.push_back (i);
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    // Stacked (FILTER's SLOPE): the pills one over the other in a knob's cell, the caption under them
    // where a knob's name sits.
    void setStacked (const juce::String& captionText)
    {
        caption = captionText;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = optionArea();
        const auto stacked = caption.isNotEmpty();
        const auto width = bounds.getWidth() / (float) (stacked ? 1 : labels.size());
        const auto height = stacked ? bounds.getHeight() / (float) labels.size() : bounds.getHeight();
        const auto mouse = getMouseXYRelative().toFloat();
        if (stacked)
        {
            g.setColour (IlanaTheme::Ui::text3.withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
            g.setFont (stackCaptionFont());
            g.drawText (caption, juce::Rectangle<float> (0.0f, bounds.getBottom() + 1.0f, (float) getWidth(), (float) getHeight() - bounds.getBottom() - 1.0f),
                        juce::Justification::centredTop, false);
        }
        for (int option = 0; option < labels.size(); ++option)
        {
            const auto cellBounds = (stacked ? juce::Rectangle<float> (bounds.getX(), bounds.getY() + height * (float) option, width, height).reduced (0.0f, 1.0f)
                                             : juce::Rectangle<float> (bounds.getX() + width * (float) option, bounds.getY(), width, bounds.getHeight()).reduced (1.5f, 1.0f));
            const auto on = values[(size_t) option] == current;
            const auto hovered = isMouseOver() && cellBounds.contains (mouse);
            g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, hovered ? 0.12f : 0.0f));
            g.fillRoundedRectangle (cellBounds, 4.0f);
            if (! on)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (cellBounds.reduced (0.5f), 4.0f, 1.0f);
            }
            g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
            g.drawText (labels[option], cellBounds, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto area = optionArea();
        const auto at = caption.isNotEmpty() ? (event.position.y - area.getY()) / area.getHeight() : (event.position.x - area.getX()) / area.getWidth();
        setValue (values[(size_t) juce::jlimit (0, labels.size() - 1, (int) (at * (float) labels.size()))]);
    }

    // A stacked choice in a packed knob's bounds: the pills where its dial is, the caption where its name is.
    static constexpr int stackPadX = 3, stackPadTop = 6, stackDial = 28;

private:
    juce::Rectangle<float> optionArea() const
    {
        if (caption.isEmpty())
            return getLocalBounds().toFloat();
        return juce::Rectangle<float> ((float) stackPadX + 1.0f, (float) stackPadTop, (float) getWidth() - 2.0f * (float) stackPadX - 2.0f, (float) stackDial);
    }

    static juce::Font stackCaptionFont() { return IlanaTheme::font (juce::jlimit (7.6f, 8.6f, 7.6f * (float) stackDial / 26.0f), true); }

    juce::String caption;
    juce::StringArray labels;
    juce::Colour colour;
    std::vector<int> values;
};
