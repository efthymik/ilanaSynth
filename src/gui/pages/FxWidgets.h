// The FX rack's small widgets (TapGrid, GateGrid, FxStackContent,
// SlotSwitch), shared by the FX and PATCH tabs. Global inline code, as the
// widget headers in src/gui are.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../AnimationUtils.h"
#include "../DisplayStyle.h"
#include "../FxLibrary.h"
#include "../IlanaLookAndFeel.h"

class TapGrid : public juce::Component,
                public juce::SettableTooltipClient,
                private IlanaAnim::FrameTimer
{
public:
    explicit TapGrid (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        setTooltip ("Custom tap grid\nDrag up or down to set each tap's level.\nDouble-click a tap to clear it. Esc cancels a drag.");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setWantsKeyboardFocus (true);
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, area, 6.0f);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("CUSTOM TAP GRID", getLocalBounds().removeFromTop (14).reduced (9, 0),
                    juce::Justification::centredLeft);

        auto bars = getBarBounds().toFloat();
        const auto columnWidth = bars.getWidth() / 16.0f;

        for (int step = 0; step < 16; ++step)
        {
            auto cell = bars.withWidth (columnWidth).reduced (1.5f, 0.0f);
            bars.removeFromLeft (columnWidth);

            const auto value = getStep (step);

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (value > 0.001f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (step == hoverStep ? 0.95f : 0.65f));
                g.fillRoundedRectangle (cell.withTop (cell.getBottom() - cell.getHeight() * value), 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        gestures.begin();
        lastStep = -1;
        processorRef.beginEdit ("Tap grid");
        setFromMouse (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! gestures.cancelled())
            setFromMouse (event);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        gestures.end();
        processorRef.endEdit();
        lastStep = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto index = stepAt (event.getPosition());

        processorRef.performEdit ("Clear tap", [this, index]
        {
            writeStep (index, 0.0f);
            gestures.end();
        });
        repaint();
    }

    // Esc during a drag puts every tap it touched back.
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() != juce::KeyPress::escapeKey || ! gestures.isOpen() || gestures.cancelled())
            return false;

        gestures.cancel();
        repaint();
        return true;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverStep != -1)
        {
            hoverStep = -1;
            repaint();
        }
    }

private:
    juce::Rectangle<int> getBarBounds() const
    {
        return getLocalBounds().reduced (8, 9).withTrimmedTop (12);
    }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds();
        const auto columnWidth = (float) bars.getWidth() / 16.0f;

        return juce::jlimit (0, 15, (int) ((float) (position.x - bars.getX()) / columnWidth));
    }

    float getStep (int index) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_taps_step" + juce::String (index + 1)))
            return value->load();

        return 0.0f;
    }

    // Writing a tap switches the pattern to Custom (6), so the grid is what plays.
    void writeStep (int index, float value)
    {
        if (auto* parameter = processorRef.apvts.getParameter ("fx_taps_step" + juce::String (index + 1)))
            gestures.set (parameter, parameter->convertTo0to1 (value));

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter ("fx_taps_pattern")))
            if (choice->getIndex() != 6)
                gestures.set (choice, choice->convertTo0to1 (6.0f));
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        const auto index = stepAt (event.getPosition());
        const auto value = juce::jlimit (0.0f, 1.0f,
                                         1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight()));

        // A fast sweep skips bars between mouse events: fill them in.
        IlanaDrag::paintRun (lastStep, lastValue, index, value, [this] (int s, float v) { writeStep (s, v); });
        lastStep = index;
        lastValue = value;
        hoverStep = index;
        repaint();
    }

    void timerCallback() override
    {
        if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    IlanaDrag::Gestures gestures;
    int hoverStep = -1, lastStep = -1;
    float lastValue = 0.0f;
};

// Trance gate steps: bar height is each step's level, bar width its LENGTH,
// the playing step lights up. Editing a built-in pattern copies it into Custom first.
class GateGrid : public juce::Component,
                 public juce::SettableTooltipClient,
                 private IlanaAnim::FrameTimer
{
public:
    explicit GateGrid (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Gate steps\nDrag up or down to set each step's level.\nRight-click a step to toggle it. Esc cancels a drag.");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setWantsKeyboardFocus (true);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 6.0f);
        const auto colour = fxColour (16);
        const auto euclid = euclidTakesOver();
        const auto columns = numColumns();
        const auto steps = euclid ? columns : numSteps();
        const auto playing = processorRef.getGateDisplayStep();
        const auto bars = getBarBounds().toFloat();
        const auto open = gateLength();
        const auto chosen = pattern();
        // Random, Build and Break play other steps every cycle: there is no fixed row to draw.
        const auto moving = ! euclid && (chosen == 4 || chosen == 6 || chosen == 7);
        const auto hits = juce::jlimit (0, 32, juce::roundToInt (readValue ("euc_hits")));
        const auto rotate = juce::jlimit (0, 31, juce::roundToInt (readValue ("euc_rotate")));

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        auto title = getLocalBounds().removeFromTop (14).reduced (9, 0);
        if (const auto swing = swingAmount(); swing > 0.004f)
            g.drawText ("SWING " + juce::String (juce::roundToInt (swing * 100.0f)) + "%", title, juce::Justification::centredRight);
        juce::String label ("STEPS");
        if (euclid)
            label = "EUCLID  " + juce::String (hits) + " IN " + juce::String (columns);
        else if (chosen == 4)
            label = "RANDOM  NEW STEPS EVERY CYCLE";
        else if (chosen == 6)
            label = "BUILD  ONE MORE STEP OPENS EACH CYCLE";
        else if (chosen == 7)
            label = "BREAK  ONE MORE STEP CLOSES EACH CYCLE";
        g.drawText (label, title, juce::Justification::centredLeft);

        for (int step = 0; step < columns; ++step)
        {
            const auto cell = cellFor (step, columns, bars).reduced (1.5f, 0.0f);
            const auto active = step < steps;
            const auto value = euclid ? (euclidHit (step, hits, columns, rotate) ? 1.0f : 0.0f) : (moving ? 0.0f : level (step));

            g.setColour (juce::Colours::white.withAlpha (active ? (step % 4 == 0 ? 0.11f : 0.07f) : 0.02f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (active && moving)
            {
                g.setColour (colour.withAlpha (0.13f));
                g.fillRoundedRectangle (cell, 2.0f);
            }
            else if (active && value > 0.001f)
            {
                // LENGTH under 100 % draws the open part of the step only.
                const auto bar = cell.withTop (cell.getBottom() - cell.getHeight() * value);
                g.setColour (colour.withAlpha (step == playing ? 1.0f : (step == hoverStep ? 0.85f : 0.6f)));
                g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * open)), 2.0f);
            }

            if (active && step == playing)
            {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (cell.withHeight (2.0f).withY (cell.getBottom() + 2.0f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (euclidTakesOver()) // (the rhythm comes from the Euclid card; these steps don't play)
            return;

        gestures.begin();
        lastStep = -1;

        if (event.mods.isPopupMenu())
        {
            const auto step = stepAt (event.getPosition());
            processorRef.performEdit ("Toggle gate step", [this, step]
            {
                setLevel (step, level (step) > 0.5f ? 0.0f : 1.0f);
                gestures.end();
            });
            return;
        }

        processorRef.beginEdit ("Gate steps");
        dragging = true;
        setFromMouse (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragging && ! gestures.cancelled())
            setFromMouse (event);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging)
            return;

        gestures.end();
        processorRef.endEdit();
        dragging = false;
        lastStep = -1;
    }

    // Esc during a drag puts every step it touched back (and the pattern).
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() != juce::KeyPress::escapeKey || ! dragging || gestures.cancelled())
            return false;

        gestures.cancel();
        repaint();
        return true;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverStep = -1;
        repaint();
    }

private:
    juce::Rectangle<int> getBarBounds() const { return getLocalBounds().reduced (8, 10).withTrimmedTop (10); }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds().toFloat();
        const auto columns = numColumns();

        for (int step = 0; step < columns; ++step)
            if ((float) position.x < cellFor (step, columns, bars).getRight())
                return step;

        return columns - 1;
    }

    // While EUCLID is on and aimed at the Trance Gate it plays the gate (its own steps, up to 32); the pattern is not heard.
    float readValue (const char* id) const { return processorRef.apvts.getRawParameterValue (id)->load(); }
    bool euclidTakesOver() const { return readValue ("euc_on") > 0.5f && juce::roundToInt (readValue ("euc_target")) == 2; }
    int numColumns() const { return euclidTakesOver() ? juce::jlimit (2, 32, juce::roundToInt (readValue ("euc_steps"))) : 16; }
    float swingAmount() const { return juce::jlimit (0.0f, 0.5f, readValue ("fx_gate_swing")); }

    // A step's cell. SWING makes every pair's first step 1 + SWING long and its second 1 - SWING, as the gate plays them.
    juce::Rectangle<float> cellFor (int step, int columns, juce::Rectangle<float> bars) const
    {
        const auto unit = bars.getWidth() / (float) columns;
        const auto swing = swingAmount();
        const auto second = (step & 1) != 0;
        const auto x = bars.getX() + unit * (float) (step - (second ? 1 : 0)) + (second ? unit * (1.0f + swing) : 0.0f);
        const auto width = unit * (second ? 1.0f - swing : 1.0f + swing);
        return { x, bars.getY(), juce::jmax (1.0f, juce::jmin (width, bars.getRight() - x)), bars.getHeight() };
    }

    int numSteps() const { return (int) readValue ("fx_gate_steps"); }
    int pattern() const { return (int) processorRef.apvts.getRawParameterValue ("fx_gate_pattern")->load(); }
    float gateLength() const { return juce::jlimit (0.05f, 1.0f, processorRef.apvts.getRawParameterValue ("fx_gate_length")->load()); }

    float level (int step) const
    {
        if (pattern() == 8)
            return processorRef.apvts.getRawParameterValue ("fx_gate_step" + juce::String (step + 1))->load();

        return IlanaSynthAudioProcessor::gatePatternLevel (pattern(), step);
    }

    void setLevel (int step, float value)
    {
        // First edit of a built-in pattern: copy it into Custom.
        if (pattern() != 8)
        {
            for (int i = 0; i < 16; ++i)
                if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (i + 1)))
                    gestures.set (parameter, IlanaSynthAudioProcessor::gatePatternLevel (pattern(), i));

            if (auto* choice = processorRef.apvts.getParameter ("fx_gate_pattern"))
                gestures.set (choice, choice->convertTo0to1 (8.0f));
        }

        if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (step + 1)))
            gestures.set (parameter, juce::jlimit (0.0f, 1.0f, value));

        repaint();
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        auto value = 1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight());

        // Snap near the ends so full and closed steps are easy to hit.
        value = value > 0.92f ? 1.0f : (value < 0.08f ? 0.0f : value);
        hoverStep = stepAt (event.getPosition());

        // A fast sweep skips bars between mouse events: fill them in.
        IlanaDrag::paintRun (lastStep, lastValue, hoverStep, value, [this] (int s, float v) { setLevel (s, v); });
        lastStep = hoverStep;
        lastValue = value;
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    IlanaDrag::Gestures gestures;
    int hoverStep = -1, lastStep = -1;
    float lastValue = 0.0f;
    bool dragging = false;
};

// Scrolling content for the FX stack: paints the cards (and, over their
// controls, a dragged card's ghost) and hands the mouse back to the page,
// which reorders by card headers.
class FxStackContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter, overPainter;
    std::function<void (const juce::MouseEvent&)> onDown, onDrag, onUp, onMove;
    std::function<bool (const juce::KeyPress&)> onKey; // Esc during a reorder drag

    FxStackContent() { setWantsKeyboardFocus (true); }

    void paint (juce::Graphics& g) override
    {
        if (painter != nullptr)
            painter (g);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (overPainter != nullptr)
            overPainter (g);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus(); // so Esc reaches keyPressed during a drag

        if (onDown != nullptr)
            onDown (event);
    }

    bool keyPressed (const juce::KeyPress& key) override { return onKey != nullptr && onKey (key); }
    void mouseDrag (const juce::MouseEvent& event) override { if (onDrag != nullptr) onDrag (event); }
    void mouseUp (const juce::MouseEvent& event) override { if (onUp != nullptr) onUp (event); }
    void mouseMove (const juce::MouseEvent& event) override { if (onMove != nullptr) onMove (event); }
};

// An FX slot's on switch for modules without an on parameter of their own:
// on means playing, off bypasses the slot. Drawn like a ToggleControl switch
// (13 px of label space above the pill), so every module's header matches.
class SlotSwitch : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    SlotSwitch (IlanaSynthAudioProcessor& p, int slotIndex)
        : parameter (p.apvts.getParameter ("fx_slot" + juce::String (slotIndex + 1) + "_bypass"))
    {
        setTooltip ("On\nSwitch this effect off to bypass its slot.");
        startTimerHz (10);
    }

    // The rack's rows draw the pill alone, filling its area.
    void setBare (bool shouldBeBare) { bare = shouldBeBare; repaint(); }

    // On in the effect's family colour (colour pass, 2026-10-28).
    void setColour (juce::Colour newColour) { if (newColour != colour) { colour = newColour; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        // Slides and fades as every switch does (the shared animator).
        IlanaTheme::paintSwitch (g, (bare ? getLocalBounds() : getLocalBounds().withTrimmedTop (13)).toFloat(),
                                 IlanaTheme::fade (*this, 0, isOn() ? 1.0f : 0.0f),
                                 colour, IlanaTheme::fade (*this, 1, isMouseOver() ? 1.0f : 0.0f, IlanaTheme::FadeRate::hover));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (isOn() ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool isOn() const { return parameter == nullptr || parameter->getValue() < 0.5f; }

    void timerCallback() override
    {
        if (IlanaAnim::showing (*this) && isOn() != shownOn)
        {
            shownOn = isOn();
            repaint();
        }
    }

    juce::RangedAudioParameter* parameter = nullptr;
    juce::Colour colour = IlanaTheme::accent();
    bool shownOn = true, bare = false;
};
