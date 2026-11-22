// The MATRIX tab (the mod matrix and its curve dock). The classes are
// declared here; their member functions are in MatrixPage.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../AnimationUtils.h"
#include "../IlanaLookAndFeel.h"
#include "../MatrixWidgets.h"
#include "../ModNames.h"
#include "../ModulePool.h"
#include "../ParamControls.h"
#include "../RemapEditor.h"

// The curve dock's right half (UI review 31, item 33; it was four help
// sentences): the route's destination as its knob, live (its value and the
// range every route into it sweeps, rings in their sources' colours), and the
// other routes into the same knob as chips; a click on one opens its curve.
class MatrixDockTarget : public juce::Component
{
public:
    explicit MatrixDockTarget (IlanaSynthAudioProcessor& p) : processorRef (p) {}

    std::function<void (int slot)> onPickRow;

    void setSlot (int newSlot);

    int getSlot() const { return slot; }
    KnobControl* getKnob() const { return knob.get(); }
    int getNumOtherRoutes() const { return (int) chips.size(); }
    juce::Button* getOtherRoute (int index) const { return juce::isPositiveAndBelow (index, (int) chips.size()) ? chips[(size_t) index].get() : nullptr; }

    // Polled by the page: rebuilds the knob when the destination changes and
    // the chips when the routes into it do.
    void refresh();

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    static constexpr int chipHeight = 22;

    juce::Rectangle<int> textColumn() const;

    IlanaSynthAudioProcessor& processorRef;
    int slot = -1, destination = 0;
    juce::String shownKey;
    std::unique_ptr<KnobControl> knob;
    std::vector<std::unique_ptr<juce::TextButton>> chips;
};

class MatrixPage : public juce::Component,
                   public juce::SettableTooltipClient,
                   private IlanaAnim::FrameTimer
{
public:
    explicit MatrixPage (IlanaSynthAudioProcessor& p);
    ~MatrixPage() override;

    void paint (juce::Graphics& g) override;

    // Under the rows: the docked remap editor beside a summary of its row,
    // or, when there is room and none is open, a note on how to open it.
    void paintDock (juce::Graphics& g);

    // A click on a sortable heading.
    void mouseUp (const juce::MouseEvent& event) override;

    void mouseMove (const juce::MouseEvent&) override { repaint (headerArea); }
    void mouseExit (const juce::MouseEvent&) override { repaint (headerArea); }

    enum class Sort { none, slot, source, destination, amount };

    // Sorts by a column: a first click sorts (amounts largest first), a
    // second reverses, a third goes back to slot order. Public for tests.
    void setSort (Sort column);

    Sort getSort() const { return sort; }
    const std::vector<int>& getVisibleSlots() const { return visibleRows; }
    RemapEditor* getRemapEditor() const { return remapEditor.get(); }

    // Opens the slot's remap editor in the dock under the rows (or closes
    // it if open). The rows never move for it (UI review 6, V6-22).
    // A click on a row: its curve in the dock (opened by hand, so it stays
    // open; another click on the same row leaves it as it is).
    void showRemap (int slot);

    void toggleRemap (int slot);

    // The dock's own opening (no scrolling, no relayout: resized() is the caller).
    void openRemapQuietly (int slot, bool automatic);

    void closeRemapQuietly();

    // (The X closes it until a curve is opened by hand, the routes change or the page is shown again.)
    void closeRemap();

    // Shows a routing's row: scrolled into view and outlined for a moment
    // (a DRIVES row on an LFO's card jumps here). -1 leaves the list as is.
    void focusSlot (int slot);

    // The dock's area (empty when it isn't shown); the tests.
    juce::Rectangle<int> getDockArea() const { return dockArea; }
    MatrixDockTarget& getDockTarget() { return dockTarget; }
    // Whether the source group headings are drawn (only when every row fits with them).
    bool areGroupHeadingsShown() const { return headingsShown; }
    int getRowHeight() const { return rowHeight; }
    juce::Rectangle<int> getHeadingArea (int source) const;
    int getSoloSlot() const { return soloSlot; }

    // "Show destination": the editor switches to the knob's page and flashes it.
    std::function<void (int destination)> onShowDestination;

    // The row's own menu (UI review 31, item 33; MVF-4).
    void showRowMenu (int slot);

    // 1 duplicate, 2 show destination, 3 solo / end solo, 4 remove; public for the tests.
    void rowMenuChoice (int slot, int choice);

    // A new row with the slot's source, amount, curve, polarity and via, and
    // no destination yet: its destination list opens. Returns the new slot.
    int duplicateRow (int slot);

    // (The tests duplicate without the list popping up.)
    bool openListsOnDuplicate = true;

    // Solo: every other route off until the solo ends (the same item again,
    // or another row's solo); those the solo switched off come back on.
    void soloRow (int slot);

    // A click on a source's group heading: all its routes off, or (all off
    // already) all back on (UI review 31, item 33; MVF-2).
    void toggleSourceRoutes (int source);

    // Folds every repeated routing it can into one row.
    void mergeAll();

    // The "!" of a repeated row: merge it with each of its twins.
    void showMergeMenu (int slot);

    // The rows an empty matrix will have, dimmed, behind the card (N16-5): the
    // column headings and a ghost of each control, fading down the page, so the
    // page reads as the matrix it becomes. Same columns as MatrixRow.
    void paintGhostRows (juce::Graphics& g) const;

    juce::Rectangle<float> emptyStateCard() const;

    // An empty matrix explains the three ways in, with a little animated
    // routing and an arrow down to the source chips.
    void paintEmptyState (juce::Graphics& g);

    void resized() override;

    // Switching to the tab shows the current routings straight away.
    void visibilityChanged() override;

private:
    int rowHeight = MatrixRow::rowHeight;
    static constexpr int dockHeight = 232;

    struct Heading
    {
        juce::String text;
        juce::Rectangle<int> area;
        Sort sort;
    };

    // The column headings and where they sit, aligned with MatrixRow.
    std::vector<Heading> headings() const;

    struct Starter
    {
        const char* label;
        const char* tip;
        Mod::Source source, aux;
        std::vector<Mod::Destination> destinations;
        float depth;
    };

    static const std::vector<Starter>& starterRoutings();

    juce::Rectangle<int> starterArea() const;

    void layoutStarters();

    void addStarter (const Starter& starter);

    void addRouting();

    // The used slots, in the chosen order.
    std::vector<int> sortedSlots() const;

    // force: re-sort now, even mid-gesture (a heading click).
    void updateRows (bool force = false);

    // A heading over each source's group, in a long list sorted by source
    // (none over rows with no source: they drive nothing).
    static constexpr int groupHeadingHeight = 18, groupHeadingsFrom = 8;

    bool wantsGroupHeadings() const;

    int sourceOfSlot (int slot) const { return (int) processorRef.readModSlot (slot).source; }

    // The list's height as layoutList lays it out: the rows, the headings,
    // the add row under them and the 8 px foot.
    int listContentHeight (int height, int headingsHeight) const;

    int numGroups() const;

    int freeSlot() const;

    void layoutList();

    void timerCallback() override;

    float emptyShownSeconds = 0.0f, emptyClock = 0.0f;
    juce::Rectangle<int> readoutArea;
    juce::String readoutSignature;
    IlanaAnim::ChangeGate changeGate;

    // Shows the patch's macro names in the source lists.
    void refreshMacroNames();

    // Greys out the LFOs and envelopes that aren't in their pools.
    void refreshPoolSources();

    // Grouped by source unless the user picks another order (S8-25).
    Sort sort = Sort::source;
    bool descending = false, viaExpanded = false;
    int numDuplicates = 0, numIdle = 0;
    juce::String repeatText;
    juce::Rectangle<int> dockArea;
    std::unique_ptr<RemapEditor> remapEditor;
    bool dockDismissed = false, dockAutomatic = false, headingsShown = false;
    // The soloed row (-1: none) and the routes its solo switched off.
    int soloSlot = -1;
    std::vector<int> soloMuted;
    bool soloWasOff = false;
    // The patch the solo belongs to: another patch loaded drops the solo
    // (its list names the old patch's routes); the page closing ends it.
    unsigned soloPatchGeneration = 0;
    void dropSoloOfOldPatch();

    IlanaSynthAudioProcessor& processorRef;
    MatrixDockTarget dockTarget { processorRef };
    juce::StringArray shownMacroNames;
    std::vector<bool> shownPoolSources;
    juce::Viewport viewport;
    // The rows' canvas: it also paints a small heading over each source's
    // group once the matrix is long (S13-3); a click on one switches that
    // source's routes off or on.
    struct ListCanvas : juce::Component,
                        public juce::TooltipClient
    {
        std::function<void (int source)> onHeadingClick;

        int headingAt (juce::Point<int> position) const
        {
            for (const auto& heading : headings)
                if (heading.area.contains (position))
                    return heading.source;
            return 0;
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            setMouseCursor (headingAt (event.getPosition()) != 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            if (event.mouseWasDraggedSinceMouseDown() || event.mods.isPopupMenu())
                return;
            if (const auto source = headingAt (event.getPosition()); source != 0 && onHeadingClick != nullptr)
                onHeadingClick (source);
        }

        juce::String getTooltip() override
        {
            for (const auto& heading : headings)
                if (heading.area.contains (getMouseXYRelative()))
                    return heading.allOff ? "Click to switch this source's routes back on."
                                          : "Click to switch all of this source's routes off (click again to bring them back), to hear what it does.";
            return {};
        }

        void paint (juce::Graphics& g) override
        {
            // As the mockup: a small square swatch, the source's name in bold
            // in its colour, the route count quiet after it.
            const auto nameFont = IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.06f);
            for (const auto& heading : headings)
            {
                const auto name = heading.text.upToFirstOccurrenceOf ("   ", false, false);
                const auto count = heading.text.fromFirstOccurrenceOf ("   ", false, false);
                g.setColour (heading.colour.withMultipliedAlpha (heading.allOff ? 0.4f : 1.0f));
                g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) heading.area.getX() + 8.0f, (float) heading.area.getCentreY() }), 2.0f);
                g.setFont (nameFont);
                g.drawText (name, heading.area.withTrimmedLeft (18), juce::Justification::centredLeft, true);
                const auto nameWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (nameFont), name);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawText (count, heading.area.withTrimmedLeft (18 + nameWidth + 8), juce::Justification::centredLeft, true);
            }
        }

        struct Item { juce::Rectangle<int> area; juce::Colour colour; juce::String text; int source = 0; bool allOff = false; };
        std::vector<Item> headings;
    } list;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    std::vector<int> visibleRows;
    juce::String headingKey;
    int totalUsed = 0; // the routings in use, before the filter
    juce::TextEditor filterField;
    juce::TextButton addButton, mergeButton;
    DashedAddButton addRow { "+  ADD MODULATION", "+  ADD MODULATION" };
    std::vector<std::unique_ptr<juce::TextButton>> starterButtons;
    std::vector<std::unique_ptr<RemapShapeTile>> shapeTiles;
    juce::Rectangle<int> headerArea;
};
