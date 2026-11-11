#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FxLibrary.h"
#include "IlanaLookAndFeel.h"

// The FX chain in the tab row, on every page (UI review 31, item 35): one
// small chip per loaded effect, in chain order, in its family's colour (the
// library's). A click bypasses it or brings it back, a double-click opens it
// on FX. Where the row is short the chips become coloured dots (the name in
// the tooltip and the hover line).
class FxChainChips : public juce::Component,
                     public juce::TooltipClient,
                     private juce::Timer
{
public:
    explicit FxChainChips (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        refresh();
        startTimerHz (10);
    }

    // The chain changed in a way that changes the chips' width.
    std::function<void()> onWidthChange;
    // A double-click: show this slot (0-based) on the FX page.
    std::function<void (int slot)> onOpen;

    static constexpr int height = 22, gap = 5, dotWidth = 20;

    // The width the chips want with names, and as dots.
    int getNamedWidth() const
    {
        auto width = 0;
        for (const auto& chip : chain)
            width += namedWidth (chip.type) + gap;
        return juce::jmax (0, width - gap);
    }

    int getDotsWidth() const { return chain.empty() ? 0 : (int) chain.size() * (dotWidth + gap) - gap; }

    // The widest of the two that fits `room` (0 when there is no chain).
    int widthFor (int room) const
    {
        const auto named = getNamedWidth();
        return named <= room ? named : juce::jmin (room, getDotsWidth());
    }

    int getNumChips() const { return (int) chain.size(); }
    bool showsNames() const { return named; }
    // Each chip's slot and box, for the tests.
    std::vector<std::pair<int, juce::Rectangle<int>>> getChipBoxes() const
    {
        std::vector<std::pair<int, juce::Rectangle<int>>> boxes;
        for (size_t i = 0; i < chain.size(); ++i)
            boxes.emplace_back (chain[i].slot, boxFor ((int) i));
        return boxes;
    }

    // Bypass a slot or bring it back, one undo step (the click, and the test).
    void toggleBypass (int slot)
    {
        if (auto* bypass = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
        {
            const auto wasBypassed = bypass->getValue() > 0.5f;
            processorRef.performEdit ((wasBypassed ? "Turn on " : "Bypass ") + fxTypeName (typeOf (slot)), [bypass, wasBypassed]
            {
                bypass->beginChangeGesture();
                bypass->setValueNotifyingHost (wasBypassed ? 0.0f : 1.0f);
                bypass->endChangeGesture();
            });
            refresh();
        }
    }

    void resized() override { named = getNamedWidth() <= getWidth(); }

    void paint (juce::Graphics& g) override
    {
        const auto font = chipFont();

        for (size_t i = 0; i < chain.size(); ++i)
        {
            const auto& chip = chain[i];
            const auto box = boxFor ((int) i).toFloat().reduced (0.5f);
            const auto colour = fxColour (chip.type);
            const auto over = (int) i == hovered;
            const auto radius = box.getHeight() * 0.5f;

            // On: the family's tint and rim (the picker's tiles); bypassed: a
            // dark pill, a hollow dot and the name in the hint grey.
            g.setColour (chip.bypassed ? IlanaTheme::Ui::panel.interpolatedWith (IlanaTheme::Ui::hover, over ? 0.6f : 0.0f)
                                       : IlanaTheme::Ui::panel.interpolatedWith (colour, over ? 0.26f : 0.16f));
            g.fillRoundedRectangle (box, radius);
            g.setColour (chip.bypassed ? IlanaTheme::Ui::line.brighter (over ? 0.5f : 0.15f) : colour.withAlpha (over ? 0.9f : 0.55f));
            g.drawRoundedRectangle (box.reduced (0.5f), radius - 0.5f, 1.0f);

            const auto dotCentre = named ? juce::Point<float> (box.getX() + 11.0f, box.getCentreY()) : box.getCentre();
            const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre (dotCentre);

            if (chip.bypassed)
            {
                g.setColour (colour.withAlpha (0.7f));
                g.drawEllipse (dot.reduced (0.5f), 1.2f);
            }
            else
            {
                g.setColour (colour);
                g.fillEllipse (dot);
            }

            if (named)
            {
                g.setColour (chip.bypassed ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text);
                g.setFont (font);
                g.drawText (labelFor (chip.type), box.withTrimmedLeft (19.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, false);
            }
        }
    }

    juce::String getTooltip() override
    {
        if (! juce::isPositiveAndBelow (hovered, (int) chain.size()))
            return {};
        const auto& chip = chain[(size_t) hovered];
        return fxTypeName (chip.type).toUpperCase() + (chip.bypassed ? " (bypassed)" : juce::String())
               + "\nClick to " + (chip.bypassed ? "turn it back on" : "bypass it") + "; double-click to open it on the FX page.";
    }

    void mouseMove (const juce::MouseEvent& event) override { setHovered (indexAt (event.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasClicked() || event.mods.isPopupMenu())
            return;

        const auto index = indexAt (event.getPosition());
        if (index < 0)
            return;

        const auto slot = chain[(size_t) index].slot;

        // A double-click opens the effect and leaves its bypass alone: the
        // single click waits for the double-click's time before it bypasses.
        if (event.getNumberOfClicks() >= 2)
        {
            ++pendingClick;
            if (onOpen != nullptr)
                onOpen (slot);
            return;
        }

        const auto click = ++pendingClick;
        juce::Timer::callAfterDelay (clickWait, [safeThis = juce::Component::SafePointer<FxChainChips> (this), click, slot]
        {
            if (safeThis != nullptr && safeThis->pendingClick == click)
                safeThis->toggleBypass (slot);
        });
    }

    static constexpr int clickWait = 260; // ms: the double-click's window

private:
    struct Chip
    {
        int slot = 0, type = 0;
        bool bypassed = false;
        bool operator== (const Chip& other) const { return slot == other.slot && type == other.type && bypassed == other.bypassed; }
    };

    static juce::Font chipFont()
    {
        return juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f));
    }

    static juce::String labelFor (int type) { return fxTypeName (type).toUpperCase(); }

    static int namedWidth (int type)
    {
        return 19 + juce::GlyphArrangement::getStringWidthInt (chipFont(), labelFor (type)) + 9;
    }

    int typeOf (int slot) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1)))
            return (int) value->load();
        return 0;
    }

    juce::Rectangle<int> boxFor (int index) const
    {
        auto x = 0;
        for (int i = 0; i < index; ++i)
            x += (named ? namedWidth (chain[(size_t) i].type) : dotWidth) + gap;
        const auto width = named ? namedWidth (chain[(size_t) index].type) : dotWidth;
        return { x, (getHeight() - height) / 2, width, height };
    }

    int indexAt (juce::Point<int> point) const
    {
        for (size_t i = 0; i < chain.size(); ++i)
            if (boxFor ((int) i).contains (point))
                return (int) i;
        return -1;
    }

    void setHovered (int index)
    {
        if (index != hovered)
        {
            hovered = index;
            repaint();
        }
    }

    void timerCallback() override { refresh(); }

    void refresh()
    {
        std::vector<Chip> now;
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = typeOf (slot);
            if (type <= 0 || fxTypeName (type).isEmpty())
                continue;
            const auto* bypass = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_bypass");
            now.push_back ({ slot, type, bypass != nullptr && bypass->load() > 0.5f });
        }

        if (now == chain)
            return;

        const auto widthChanges = now.size() != chain.size()
                                  || ! std::equal (now.begin(), now.end(), chain.begin(), [] (const Chip& a, const Chip& b) { return a.type == b.type; });
        chain = std::move (now);
        hovered = -1;

        if (widthChanges && onWidthChange != nullptr)
            onWidthChange();

        resized();
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::vector<Chip> chain;
    int hovered = -1;
    int pendingClick = 0;
    bool named = true;
};
