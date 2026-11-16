// The modulation source editors: PLAY's cards and the editor's source pop-up
// (SourcePopover). The classes are declared here; their member functions are
// in ModSourcePanel.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../AnimationUtils.h"
#include "../EnvelopeDisplay.h"
#include "../FmOperatorInfo.h"
#include "../IlanaLookAndFeel.h"
#include "../LfoDisplay.h"
#include "../LfoShapeMenu.h"
#include "../ModNames.h"
#include "../ModulationCardView.h"
#include "../ModuleMenu.h"
#include "../ModulePool.h"
#include "../OperatorPoolCards.h"
#include "../ParamControls.h"
#include "EnvLfoPages.h"
#include "OscPage.h"
#include "PageHelpers.h"

// A modulation source edited where you are (ilana, 2026-10-09, after Vital
// and Serum 2: "dragging works everywhere, but the sources are spread over
// tabs"): PLAY's MODULATION card and the dock's pop-out editor both show the
// selected source's graph, its main controls and what it drives, so a
// source can be dragged, shaped and its depths set without a trip to MOD.

// The matrix slots a source drives: as the slot's source, its LFO's second
// output, or its VIA. Bypassed and zero-depth routes count (they are listed,
// dimmed), so a route set to 0 % doesn't vanish under the mouse.
inline std::vector<int> modSlotsDrivenBy (const IlanaSynthAudioProcessor& processor, int source)
{
    std::vector<int> slots;
    const auto s = (Mod::Source) source;
    const auto lfo = Mod::lfoIndexFor (s);
    const auto sourceB = lfo >= 0 ? Mod::lfoBSourceFor (lfo) : Mod::Source::None;

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);
        if (routing.destination == 0 || routing.source == Mod::Source::None)
            continue;
        if (routing.source == s || routing.aux == s || (sourceB != Mod::Source::None && routing.source == sourceB))
            slots.push_back (slot);
    }
    return slots;
}

// How many knobs a source moves now (the badge on its tab and chip).
inline int modRouteCount (const IlanaSynthAudioProcessor& processor, int source)
{
    auto count = 0;
    for (const auto slot : modSlotsDrivenBy (processor, source))
        count += processor.readModSlot (slot).isActive() ? 1 : 0;
    return count;
}

inline juce::Colour sourceTabColour (int source)
{
    return source == opEnvSourceId ? OperatorPool::colour() : modSourceColour (source);
}

inline juce::String sourceTabName (int source, const IlanaSynthAudioProcessor& processor)
{
    return source == opEnvSourceId ? juce::String ("OP ENV") : ModNames::sourceUpper (source, &processor);
}

// OP ENV's list of what plays it: each oscillator's OUTPUT as a bar, under a
// header that folds it away.
class OperatorEnvList : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit OperatorEnvList (IlanaSynthAudioProcessor& p);

    std::function<void()> onToggle;
    bool isOpen() const { return open; }
    static constexpr int headerHeight = Space::cardHeaderMod, rowHeight = 16;
    int getIdealHeight() const { return headerHeight + (open ? 4 + rowHeight * juce::jmax (1, (int) OperatorPool::operatorsOnEnv (processorRef).size()) + 4 : 0); }

    void paint (juce::Graphics& g) override;

    void mouseUp (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    bool open = true; // (open, as PLAY's OP ENV card listed them: ilana, 2026-10-09)
};

// "DRIVES": each route of one source, its target, a bar of its depth and the
// number. Drag a bar sideways to set the depth (shift for fine), double-click
// it to zero, click the target to open its matrix row, right-click for
// bypass and remove. Collapsible to its header line (PLAY folds it when the
// card is short: the controls come first).
class SourceRouteList : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit SourceRouteList (IlanaSynthAudioProcessor& p);

    void setSource (int newSource, juce::Colour newColour);

    // Folded: the header line only, with the count; a click on it opens it.
    void setCollapsed (bool shouldCollapse);

    bool isCollapsed() const { return collapsed; }
    std::function<void (bool)> onCollapsedChange;
    // The header clicked (folded or opened by hand).
    std::function<void (bool collapsed)> onUserToggle;

    static constexpr int headerHeight = Space::cardHeaderOsc, rowHeight = 18;

    // The height that lists every route (at least one row, for the empty note).
    int getIdealHeight() const { return headerHeight + 6 + juce::jmax (1, (int) slots.size()) * rowHeight + 6; }
    int getNumRoutes() const { return (int) slots.size(); }
    int getRouteSlot (int row) const { return juce::isPositiveAndBelow (row, (int) slots.size()) ? slots[(size_t) row] : -1; }
    juce::Rectangle<int> getRowBounds (int row) const { return rowArea (row); }
    juce::Rectangle<int> getBarBounds (int row) const { return barArea (rowArea (row)); }

    void paint (juce::Graphics& g) override;

    void mouseMove (const juce::MouseEvent& event) override { updateHover (event.getPosition()); }
    void mouseExit (const juce::MouseEvent&) override { updateHover ({ -1, -1 }); }

    void mouseDown (const juce::MouseEvent& event) override;

    void mouseDrag (const juce::MouseEvent& event) override;

    void mouseUp (const juce::MouseEvent&) override;

    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    juce::Rectangle<int> headerArea() const { return getLocalBounds().reduced (10, 0).removeFromTop (headerHeight).withTrimmedTop (Space::s4); } // off-grid

    juce::Rectangle<int> rowArea (int row) const;

    // The targets' column: as wide as the longest target (no wider than a
    // third of the row), so the bars start right after the names.
    int nameWidth (juce::Rectangle<int> row) const { return juce::jlimit (80, juce::jmax (80, row.getWidth() / 3), longestName + 18); }
    static constexpr int valueWidth = 46;

    juce::Rectangle<int> barArea (juce::Rectangle<int> row) const;

    int rowAt (juce::Point<int> position) const;

    void updateHover (juce::Point<int> position);

    void showMenu (int slot);

    // Repaint only when a route of this source changed (added, removed,
    // moved, its depth set from a knob ring).
    void refresh (bool force);

    void timerCallback() override;

public:
    // The number of routes changed (the owner may re-lay itself out).
    std::function<void()> onRoutesChanged;

private:
    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    juce::Colour colour { IlanaTheme::accent() };
    std::vector<int> slots;
    juce::int64 lastSignature = -1;
    bool collapsed = false, editing = false, hoverName = false;
    int hoverRow = -1, dragRow = -1, longestName = 0;
    float dragStartDepth = 0.0f;
};

// One source's editor: its graph and main controls side by side over its
// DRIVES list. An LFO: the drawn shape, SHAPE, RATE, SMOOTH, SYNC and
// RETRIG. An envelope: the graph and ATTACK, DECAY, SUSTAIN and RELEASE.
// Anything else (velocity, the wheel, a macro...) has no settings: a line
// on what it is, its live value, and its routes. Its rarer settings stay
// on MOD (onOpenInMod).
class SourceEditor : public juce::Component,
                     private juce::Timer
{
public:
    explicit SourceEditor (IlanaSynthAudioProcessor& p);

    // The envelope (0-15, the pool's order) a source is, or -1.
    static int envelopeOf (int source);

    static juce::String envelopePrefix (int env);

    int getSource() const { return source; }
    SourceRouteList& getRouteList() { return routes; }

    // OP ENV's picture: a click opens the operator's envelope on FM.
    std::function<void()> onEditOperatorEnv;

    void setSource (int newSource);

    // Lay out with the DRIVES list folded to its header line (PLAY on a short card).
    void setRoutesFolded (bool fold);

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    enum class Kind { lfo, envelope, opEnv, other };
    static constexpr int gap = 10, menuHeight = 13 + 4 + 28, knobRowHeight = 70, minGraphHeight = 90;

    juce::String describeOther() const { return describe (source); }

public:
    // What a source with no settings is, in one line (its pop-up and PLAY's card).
    static juce::String describe (int source);

private:
    // An envelope nothing plays dims, and says so over its graph (UI review
    // 6, I6-2; one wording with MOD: S8-17).
    void paintOverChildren (juce::Graphics& g) override;

    void refreshUnused();

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    bool built = false;
    Kind kind = Kind::other;
    juce::Colour colour { IlanaTheme::accent() };
    std::unique_ptr<juce::Component> graph;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<LfoRateControl> rate;
    SourceRouteList routes;
    OperatorEnvList operatorList;
    juce::TextButton opEnvLink;
    juce::String unusedNote;
    juce::Rectangle<int> graphArea;
    std::array<float, 80> history {};
    size_t historyPos = 0;
};

// A performance source's live value as a bar (its pop-up, UI review 31,
// items 19 and 28). MOD WHEEL and PRESSURE are also moved here with the mouse:
// drag along it (a click jumps there), double-click for 0. What it sends is
// what a hardware wheel would (IlanaSynthAudioProcessor::sendUiModWheel), so
// the next MIDI message takes over again.
class SourceValueBar : public juce::Component,
                       public juce::SettableTooltipClient,
                       private IlanaAnim::FrameTimer
{
public:
    // Every frame: an LFO's or envelope's value moves at the display's refresh (it was 30 a second).
    explicit SourceValueBar (IlanaSynthAudioProcessor& p) : processorRef (p) { startTimerHz (60); }

    static constexpr int height = 22;

    void setSource (int newSource, juce::Colour newColour);

    bool isMovable() const { return source == (int) Mod::Source::ModWheel || source == (int) Mod::Source::Aftertouch; }
    float getShownValue() const { return shown; }

    // Sets the value as a drag would (0..1); the tests.
    void moveTo (float value);

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& event) override { mouseDrag (event); }
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { moveTo (0.0f); }

private:
    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    juce::Colour colour { IlanaTheme::accent() };
    float shown = 0.0f, held = 0.0f;
    IlanaAnim::BlockSmoother liveSmoother;
    double heldUntil = 0.0;
};

// The dock's pop-out editor: a click on a source chip (or EDIT on PLAY's
// ENVELOPE and LFO cards) opens it above the dock on whatever page is
// showing, so a source is shaped and its depths set beside the knobs it
// drives. An LFO, an envelope, the MSEG and the Clocked S&H get their full
// editor, every setting MOD had (ilana, 2026-10-25: "integrate all settings
// in the pop up") laid out compact, over the DRIVES list; anything else
// the short editor.
// The header drags it anywhere in the page area; the cross, Escape or a
// second click on the chip closes it. While it is open its source is
// pinned (the knobs it drives stay lit).
class SourcePopover : public juce::Component
{
public:
    SourcePopover (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef);

    std::function<void()> onClose;
    // Its size changed (DRIVES opened or closed): the owner places it again.
    std::function<void()> onSizeChanged;

    int getSource() const { return source; }
    SourceEditor& getEditor() { return editor; }

    void setSource (int newSource);

    // The short editor's size; the full ones compact (ilana's mockup, 2026-10-25): an envelope's card
    // 330 wide, an LFO's 400, as tall as their controls and DRIVES.
    // (A performance source's: 300 wide, its line, its value and DRIVES; UI review 31, item 19. It was 620 x 340.)
    static constexpr int preferredWidth = 300, describeHeight = 32, headerHeight = Space::cardHeaderPopup;
    static constexpr int padding = Space::s4, sideInset = 14, bottomInset = 14;
    static constexpr int compactHeader = 26, compactInset = 8, compactTop = 6, routesGap = 6;
    // (390: the envelope's graph about as wide as the LFO's, ilana 2026-10-29; 410: five switch pills in the LFO column at the real fonts.)
    static constexpr int envCardWidth = 390, lfoCardWidth = 410;
    // (A drawn LFO shape or the MSEG: the graph twice as wide, UI review 30.)
    static constexpr int lfoWideExtra = 210;
    // (OP PITCH's and OP LFO's DX7 editors, compact as the others (UI review 26): the graph and a column of ring knobs.)
    static constexpr int operatorGraphWidth = 196;

    juce::Point<int> preferredSize() const;

    // Above the chip it came from, inside the page area; where it was
    // dragged to once moved.
    juce::Rectangle<int> placeIn (juce::Rectangle<int> area, juce::Rectangle<int> chip);

    void paint (juce::Graphics& g) override;

    SourceValueBar& getValueBar() { return valueBar; }

    void resized() override;

    void mouseMove (const juce::MouseEvent& event) override;

    void mouseDown (const juce::MouseEvent& event) override;

    void mouseDrag (const juce::MouseEvent& event) override;

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    bool keyPressed (const juce::KeyPress& key) override;

private:
    enum class Kind { lfo, envelope, other };

    int headerBottom() const { return padding + compactHeader; }

    // DRIVES folded to its header line, or open to its whole list (up to 200 px).
    int routesHeight() const;

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    int source = 0;
    Kind kind = Kind::other;
    SourceEditor editor;
    SourceRouteList routes;
    SourceValueBar valueBar;
    juce::Rectangle<int> describeArea;
    std::unique_ptr<LfoSection> lfoSection;
    std::unique_ptr<EnvSection> envSection;
    juce::TextButton closeButton;
    juce::Rectangle<int> pageArea;
    juce::Point<int> topLeft, dragStart;
    bool moved = false, dragging = false;
};

// PLAY's MODULATION card tabs: one per source in the patch (the envelopes
// and LFOs in their pools, the MSEG and the Operator Env's while used),
// in equal columns, so the row reads as a grid rather than ragged pills.
// Each tab is the source's drag handle (the grip at its right), says how
// many knobs it moves, and a click shows it in the card's editor.
class SourceTab : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    SourceTab (IlanaSynthAudioProcessor& p, int sourceIndex);

    std::function<void (int)> onSelect;
    int getSource() const { return source; }

    void setSelected (bool shouldSelect);

    void setRouteCount (int count);

    // A small pill with just this name (PLAY's carousels): a dot, or the route count, then the name.
    void setCompactLabel (const juce::String& text);

    void paint (juce::Graphics& g) override;

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void mouseUp (const juce::MouseEvent& event) override;

    void mouseDrag (const juce::MouseEvent&) override;

private:
    void paintCompact (juce::Graphics& g) const;

    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    int routes = 0;
    bool selected = false;
    juce::String compactLabel;
};

// PLAY's MODULATION card (option 1 of the 2026-10-09 plan, after Serum 2's
// mod section): the ENVELOPE and LFO cards in one. A grid of source tabs
// (the envelopes and LFOs in the patch, then "+"), all one width so the rows
// line up with the card's edges; under them the selected source's editor
// and its DRIVES list, closed to its header line until opened (ilana's rule
// for PLAY: settings before assignments), and folded again on a card too
// short for both.
class ModulationCard : public juce::Component,
                       public ModulationCardView,
                       private juce::Timer
{
public:
    explicit ModulationCard (IlanaSynthAudioProcessor& p);

    // EDIT ›: the selected source's full editor on MOD (OP ENV's: on FM).
    std::function<void (int source)> onOpenInMod;

    // OP ENV's editor (FM, the first operator on it).
    std::function<void()> onEditOperatorEnv;

    int getSelectedSource() const override { return selected; }
    const std::vector<int>& getSources() const { return sources; }
    std::vector<int> getSourceList() const override { return sources; }
    void selectSource (int source) override { select (source); }
    int sourceOfTab (const juce::Component& tab) const override;
    std::vector<juce::Component*> getSourceTabs() const override;
    SourceEditor& getEditor() { return editor; }
    std::vector<SourceTab*> getTabs() const;

    void select (int source);

    static constexpr int headerHeight = Space::cardHeaderFull, pad = 10, tabHeight = 28, tabGap = Space::s6, minTabWidth = 104, maxRows = 2;

    void paint (juce::Graphics& g) override;

    void resized() override;

    void mouseMove (const juce::MouseEvent& event) override;

    void mouseExit (const juce::MouseEvent& event) override { mouseMove (event.withNewPosition (juce::Point<int> (-1, -1))); }

    void mouseUp (const juce::MouseEvent& event) override;

    void openSelectedInMod() override;

private:
    static constexpr int editWidth = 60;
    // The graph and controls keep at least this; the DRIVES list folds to
    // its header line before they get less (controls first).
    static constexpr int minControlsHeight = 130;

    juce::Rectangle<int> editLinkArea() const { return { getWidth() - 12 - editWidth, headerHeight / 2 - 8, editWidth, 16 }; }

    void layoutEditor();

    // The sources the card lists: the envelopes and LFOs in their pools, the
    // patch's MSEG and the Operator Env's while it uses them.
    std::vector<int> wantedSources() const;

    bool anyToAdd() const;

    void refreshSources();

    void showAddMenu();

    void showMoreMenu();

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    SourceEditor editor;
    std::vector<int> sources, hiddenSources;
    std::vector<std::unique_ptr<SourceTab>> tabs;
    juce::TextButton addTab, moreTab;
    juce::Rectangle<int> editorArea;
    int selected = (int) Mod::lfoSourceFor (0);
    bool hadOpEnv = false;
    bool hoverEdit = false;
    // The DRIVES list is closed until opened (ilana, 2026-10-09: what a
    // source drives is a drop-down, the controls come first); opened, it
    // stays open from source to source.
    bool routesOpen = false;
};
