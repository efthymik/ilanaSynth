// EnvSection and LfoSection (the source pop-up's editors) and their parts.
// The classes are declared here; their member functions are in EnvLfoPages.cpp.
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
#include "../ModRoutes.h"
#include "../ModulePool.h"
#include "../OperatorPoolCards.h"
#include "../ParamControls.h"
#include "../ParamInfo.h"
#include "../SequencerEditors.h"
#include "PageHelpers.h"

// An LFO's RATE (UI review 4, V12 and S22): Hz while free-running, note
// values (1/16, 1/8T...) while SYNC is on. Two knobs share one place, one
// on the RATE parameter and one on DIVISION, and SYNC picks which is shown;
// both values stay stored. Modulating RATE still acts while synced (the
// DSP scales the division's rate by it, up to 4 octaves each way), so the
// division knob stands in for RATE's modulation: it shows RATE's ring and
// dots, and a source dropped on it routes to RATE.
// The knobs are the page's children: lay out layoutItem() (the RATE knob)
// like any knob, then call matchBounds().
class LfoRateControl : private juce::Timer
{
public:
    LfoRateControl (IlanaSynthAudioProcessor& p, int lfoIndex, juce::Colour accent, bool followsTheme);

    void addTo (juce::Component& parent);

    // Whether the page shows this LFO's controls at all.
    void setShown (bool shouldShow);

    juce::Component* layoutItem() { return &rate; }

    void setBounds (juce::Rectangle<int> bounds);

    // The division knob follows the RATE knob's place.
    void matchBounds() { division.setBounds (rate.getBounds()); }

    KnobControl& getRateKnob() { return rate; }
    KnobControl& getDivisionKnob() { return division; }

    bool isSynced() const;

    // Follows SYNC (the timer does this too).
    void refresh();

private:
    void timerCallback() override { refresh(); }

    IlanaSynthAudioProcessor& processorRef;
    int lfo = 0;
    bool shown = true;
    KnobControl rate, division;
};

// PLAY's LFO card RATE (the approved design): a labelled horizontal slider
// with its value, on the card's one baseline with SHAPE, SYNC and RETRIG. As
// LfoRateControl it follows SYNC (Hz, or a note value), and a source dropped
// on it still routes to RATE.
class LfoRateSlider : public juce::Component,
                      public juce::DragAndDropTarget,
                      private juce::Timer
{
public:
    LfoRateSlider (IlanaSynthAudioProcessor& p, int lfoIndex, juce::Colour accent);

    void addTo (juce::Component& parent) { parent.addChildComponent (*this); }
    void setShown (bool shouldShow) { shown = shouldShow; setVisible (shown); refresh(); }
    ValueSliderControl& getRateSlider() { return rate; }
    ValueSliderControl& getDivisionSlider() { return division; }

    void resized() override;

    bool isInterestedInDragSource (const SourceDetails& details) override;

    void itemDropped (const SourceDetails& details) override;

private:
    bool isSynced() const;

    void refresh();

    void timerCallback() override { refresh(); }

    IlanaSynthAudioProcessor& processorRef;
    int lfo = 0;
    bool shown = true;
    juce::Label label;
    ValueSliderControl rate, division;
    int destination = 0;
};

// PLAY's OP ENV tab (UI review 8, I8-18): the Operator Env every operator
// plays, drawn as one overlay of the operators' envelopes. It is a picture
// and a link, not a second editor: a click opens the OP ENV editor.
class OperatorEnvOverview : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit OperatorEnvOverview (IlanaSynthAudioProcessor& p);

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override;

    void mouseUp (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    std::array<OperatorPool::ShapeCache, (size_t) OscillatorIds::count> caches;
    IlanaAnim::ChangeGate gate;
};

// ENV 1-16's names on the MOD page (cards, panel title), 0-based.
inline juce::String envelopeTitle (int env)
{
    const juce::StringArray titles { "AMP ENV", "FILT ENV", "FILT 2 ENV", "ENV 4" };
    return env < 4 ? titles[env] : "ENV " + juce::String (env + 1);
}

class EnvSection : public juce::Component,
                   private juce::Timer
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef);

    // The bend of an envelope's segments has one name everywhere it is
    // set (UI review 6, I6-22): CURVE, as on the LFO and the matrix.

    // OP ENV and OP PITCH after ENV 1-16.
    static constexpr int opEnvId = 16;
    static constexpr int opPitchId = 17;

    // One wording for "unused" on every page (UI review 8, S8-17), PLAY's
    // ENVELOPE card and the MOD page's AMP ENV and OP cards alike.
    static const char* ampUnusedText() { return "unused: the oscillators play OP ENV"; }

    // Shows an envelope, adding its card when the patch doesn't have it yet.
    void select (int index);

    // The pop-up editor's layout (ilana's compact mockup, 2026-10-25): no pool (its chip chose the
    // envelope) and no card, the graph at the left, ATTACK DECAY SUSTAIN RELEASE over DELAY HOLD
    // VELOCITY KEY RATE.
    void setCompact (bool shouldBeCompact);

    static constexpr float compactDial = 28.0f;
    static constexpr int compactPitch = 41, compactRow = 39, compactGap = 10; // (41: VELOCITY and KEY RATE 12 px apart at 175 %, UI review 26)
    static constexpr int compactKnobsWidth = 4 * compactPitch, compactHeight = 2 * compactRow;
    // The pop-up's graph starts this much above the knobs (ilana's layout, 2026-10-29): its owner gives the section the room.
    static constexpr int compactGraphRise = 6;

    // The editor row under the cards (the design's 118 px): the graph at the
    // left (40 %), the knob card at the right, exactly as tall as its nine
    // knobs and its header need.
    // (118 for the card, plus the graph's 16 px time ruler under its frame: the card ends level with the frame, P-4.)
    static constexpr int rulerBand = 16;
    static constexpr int editorHeight = 118 + rulerBand;
    static constexpr int panelHeightNeeded = editorHeight - rulerBand;

    void resized() override;

    void paint (juce::Graphics& g) override;

    int getSelected() const { return selected; }
    OperatorEnvEditor& getOperatorEditor() { return opEditor; }

    // ENV 1-16's colours, 0-based.
    static juce::Colour colourOf (int env);

private:
    // ENV 6-16 in their mod source colours.
    static juce::Colour extraColour (int env);

    // A patch loaded, or an envelope removed, can take away the one shown.
    // A patch whose operators all play the Operator Env (a DX7 voice)
    // opens on OP ENV rather than the unused AMP ENV (UI review 7, I7-8).
    void timerCallback() override;

    struct ExtraUnit
    {
        std::unique_ptr<EnvelopeDisplay> display;
        std::array<std::unique_ptr<KnobControl>, 5> knobs;
    };
    struct Unit
    {
        juce::Component* display = nullptr;
        std::vector<juce::Component*> knobs;
    };

    void addStageTwoKnobs (IlanaSynthAudioProcessor& p, const juce::String& prefix, juce::Colour colour, Unit& unit,
                           bool followsTheme);

    std::vector<std::unique_ptr<KnobControl>> stageTwoKnobs;

    // The selection points at a card the pool doesn't have (an envelope
    // removed, or OP ENV / OP PITCH on a patch without the Operator Env:
    // they are in the pool only on a patch that plays it, OP PITCH also
    // while the matrix still routes it).
    bool selectionGone() const;

    void updateVisibility();

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    bool compact = false;
    OperatorEnvEditor opEditor { processorRef };
    EffectRules ampRules { processorRef };
    bool ampWasUsed = true;
    EnvelopeDisplay ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display;
    KnobControl ampA, ampD, ampS, ampR, ampVel;
    KnobControl feA, feD, feS, feR, feVel;
    KnobControl f2A, f2D, f2S, f2R, f2Vel;
    KnobControl meA, meD, meS, meR, meVel;
    KnobControl e4A, e4D, e4S, e4R, e4Vel;
    std::vector<Unit> units;
    std::vector<ExtraUnit> extraUnits;
    int selected = 0;
};

// The Clocked S&H source's picture: its value over the last few seconds,
// as held steps, so its DIVISION can be seen (it has no cycle of its own).
class ClockShView : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    explicit ClockShView (IlanaSynthAudioProcessor& p);

    static juce::Colour colour() { return modSourceColour ((int) Mod::Source::ClockSh); }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    static constexpr int size = 120;
    IlanaSynthAudioProcessor& processorRef;
    std::array<float, (size_t) size> history {};
    int head = 0;
};

class LfoSection : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private IlanaAnim::FrameTimer
{
public:
    // The cards past the LFOs: the patch's MSEG while it is used, the
    // Clocked S&H once routed (edited here, beside the LFOs: UI review 6,
    // V5-7 / I6-21), and the Operator Env's LFO (UI review 7, I7-7).
    static constexpr int msegId = IlanaSynthAudioProcessor::numLfos;
    static constexpr int clockId = IlanaSynthAudioProcessor::numLfos + 1;
    static constexpr int opLfoId = IlanaSynthAudioProcessor::numLfos + 2;

    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef);

    ~LfoSection() override;

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { lastShape = -1; timerCallback(); }

    // The pop-up editor's layout (ilana's compact mockup, 2026-10-25): no pool (its chip chose the
    // LFO) and no card. The graph at the left; a column with SHAPE, the switches as pills, TRIGGER
    // (and AXIS) with FIRE on a simulated shape, then the knobs four to a row.
    void setCompact (bool shouldBeCompact);

    // The compact layout's height for what it shows now (its owner sizes the pop-up to it).
    int compactHeight() const;

    // The shapes drawn by hand (Draw, Steps, Curve) and the MSEG get a pop-up twice as wide: the extra
    // width is all graph, so a step or a point is big enough to grab (UI review 30, S8). The other
    // shapes keep ilana's compact card.
    bool compactWantsWideGraph() const;

    // The compact layout's size changed (another shape, another source).
    std::function<void()> onCompactSizeChanged;

    // The pop-up (ilana's layout, 2026-10-29): SHAPE sits in its title row. The section's bounds then start
    // `skip` px higher (the title row and the gap under it); SHAPE goes in the first `row` px of them,
    // ending `rightInset` px short of the right edge (clear of the pop-up's close button).
    void setCompactTitleRow (int skip, int row, int rightInset);

    int titleSkip = 0, titleRow = 0, titleRightInset = 0;

    // In the pop-up the section's bounds reach up over the title row; only SHAPE there is its own, so the
    // rest of that row (the drag strip, the close button, their hover) stays the pop-up's.
    bool hitTest (int x, int y) override;

    static constexpr float compactDial = 28.0f;
    static constexpr int compactColumn = 4 * 46, compactPitch = 46, compactRow = 39, compactGap = 10, compactLine = 16, compactLineGap = 4;

    static constexpr int lfoRulerBand = 16; // (LfoDisplay: its ruler and the 2 px before it)

    void resized() override;

    void paint (juce::Graphics& g) override;

    static juce::Colour lfoColour (int index);

    // Shows an LFO (or the MSEG, or OP LFO), adding its card when the patch
    // doesn't have it yet.
    void select (int index);

    // (PLAY's LFO card labels its switch the same way.) RETRIG on a plain shape (one LFO per voice, restarted by its note);
    // VOICE on a simulated one (each voice runs its own simulation; PER VOICE did not fit
    // its slot, N16-6), whose TRIGGER says when it restarts.
    int shapeOf (int lfo) const;

    // The compact layout's knobs, in the full layout's order (RATE, SMOOTH, then the shape's own).
    std::vector<juce::Component*> compactKnobs (int lfo) const;

    // Where the knob rows start under `lines` lines of menus and switches.
    static int compactRowsTop (int lines) { return lines * (compactLine + compactLineGap); }

    void layoutCompact (juce::Rectangle<int> area);

    static void labelRunSwitch (ToggleControl& toggle, bool simulated);

    // The UI test reaches these through here.
    int getSelected() const { return selected; }
    LfoRateControl& getRateControl (int lfo) { return controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, lfo)]->rate; }
    juce::Rectangle<int> getPanelBounds() const { return panel; }
    OperatorLfoEditor& getOperatorEditor() { return opLfoEditor; }

private:
    struct Controls
    {
        Controls (IlanaSynthAudioProcessor& p, int lfo, juce::Colour accent, bool followsTheme)
            : Controls (p, p.apvts, lfo, accent, followsTheme) {}

        Controls (IlanaSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state, int lfo, juce::Colour accent, bool followsTheme)
            : shape (state, "lfo" + juce::String (lfo) + "_shape", "SHAPE"),
              rate (p, lfo - 1, accent, followsTheme),
              sync (state, "lfo" + juce::String (lfo) + "_sync", "SYNC"),
              div (state, "lfo" + juce::String (lfo) + "_div", "DIVISION"),
              retrig (state, "lfo" + juce::String (lfo) + "_retrig", "RETRIG"),
              key (state, "lfo" + juce::String (lfo) + "_key", "KEY"),
              phase (state, "lfo" + juce::String (lfo) + "_phase", "START", accent, followsTheme)
              , physA (state, "lfo" + juce::String (lfo) + "_phys_a", "HEIGHT", accent, followsTheme)
              , physB (state, "lfo" + juce::String (lfo) + "_phys_b", "BOUNCE", accent, followsTheme)
              , kick (state, "lfo" + juce::String (lfo) + "_kick", "KICK")
              , smooth (state, "lfo" + juce::String (lfo) + "_smooth", "SMOOTH", accent, followsTheme)
              , stereo (state, "lfo" + juce::String (lfo) + "_stereo", "STEREO", accent, followsTheme)
              , seed (state, "lfo" + juce::String (lfo) + "_seed", "SEED", accent, followsTheme)
              , trigger (state, "lfo" + juce::String (lfo) + "_trigger", "TRIGGER")
              , axis (state, "lfo" + juce::String (lfo) + "_axis", "OUT 1 AXIS")
              , loop (state, "lfo" + juce::String (lfo) + "_loop", "LOOP")
        {
            // The LFO's own colour lights its switches (the design's SYNC), as it does its knobs.
            if (! followsTheme)
                for (auto* toggle : { &sync, &retrig, &key, &loop, &kick })
                {
                    toggle->getButton().setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
                    // (A switch drawn as a switch takes its colour from the
                    // property, not the button colour: the design's pink SYNC.)
                    toggle->setSwitchColour (accent);
                }

            // The SHAPE menu carries the LFO's colour too (the design's .sel.c).
            shape.setTint (accent);

            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                sim.push_back (std::make_unique<KnobControl> (state, "lfo" + juce::String (lfo) + "_p" + juce::String (param + 1),
                                                              "P" + juce::String (param + 1), accent, followsTheme));
            fire.setButtonText ("FIRE");
            fire.setTooltip ("Fire\nTriggers the LFO now, as a new note or TRIGGER would: drops the ball, plucks the "
                             "spring, restarts a seeded sequence.");
        }

        ComboControl shape;
        LfoRateControl rate;
        ToggleControl sync;
        ComboControl div;
        ToggleControl retrig;
        ToggleControl key;
        KnobControl phase;
        KnobControl physA, physB;
        ToggleControl kick;
        // M8.1
        KnobControl smooth, stereo, seed;
        ComboControl trigger, axis;
        ToggleControl loop;
        juce::TextButton fire;
        std::vector<std::unique_ptr<KnobControl>> sim;
        int labelledShape = -1;
    };

    float read (const juce::String& id) const;

    // Oscillators that play (or warp with) an envelope choice, for the
    // card's "what it drives".
    juce::StringArray envelopeUses (int ampChoice, int warpChoice) const;

    // Choosing a simulated shape from the menu loads its knobs' defaults
    // (presets and automation keep whatever they set).
    void shapePicked (int lfo);

    // FIRE: a momentary press of the LFO's fire parameter.
    void fire (int lfo);

    // Names and value text of a simulated shape's knobs.
    void labelSimulated (Controls& c, int shape);

    // The panel's grid (UI review 8, I8-17): three option rows on the left
    // (SHAPE; the switches; TRIGGER, OUTPUT and FIRE), up to two rows of
    // four knobs on the right.
    struct Grid
    {
        static constexpr int perRow = 4;
        static constexpr int rowHeight = 13 + 24 + 14;
        static constexpr int rowGap = 8;
        std::array<juce::Rectangle<int>, 3> left;
        juce::Rectangle<int> right;
        int knobHeight = 0, oneRowHeight = 0;
        bool twoRows = true;

        // Switch `index` of `slots` across the second left row.
        juce::Rectangle<int> switchSlot (int index, int slots) const
        {
            const auto width = left[1].getWidth() / slots;
            return left[1].withX (left[1].getX() + width * index).withWidth (width).reduced (2, 1);
        }

        // The LFO's switches in fixed places of unequal width: RETRIG's slot is
        // wide enough for "VOICE" on a simulated shape (V10-6 / I10-6), the
        // others for their names.
        juce::Rectangle<int> lfoSwitchSlot (int index) const
        {
            static constexpr float weights[] { 1.0f, 1.7f, 1.0f, 1.1f, 1.1f };
            auto before = 0.0f, total = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                total += weights[i];
                before += i < index ? weights[i] : 0.0f;
            }
            const auto unit = (float) left[1].getWidth() / total;
            return left[1].withX (left[1].getX() + juce::roundToInt (before * unit)).withWidth (juce::roundToInt (weights[index] * unit)).reduced (2, 1);
        }

        // Knobs in reading order, `perRow` to a row (a null keeps a place
        // empty); all in one row when two rows wouldn't fit.
        void place (const std::vector<juce::Component*>& knobs) const
        {
            if (! twoRows)
            {
                layoutRow (right, knobs);
                return;
            }

            // (Knobs for one row only take its taller height, so what the
            // LFO drives has a band no wider than its text needs: V12-3.)
            const auto height = oneRowHeight; // the same row height for one or two rows, so RATE never moves
            for (size_t row = 0; row < 2 && row * (size_t) perRow < knobs.size(); ++row)
            {
                std::vector<juce::Component*> items ((size_t) perRow, nullptr);
                for (size_t k = 0; k < (size_t) perRow && row * (size_t) perRow + k < knobs.size(); ++k)
                    items[k] = knobs[row * (size_t) perRow + k];
                layoutRow (right.withTrimmedTop ((int) row * (height + rowGap)).withHeight (height), items);
            }
        }

        // What the grid leaves free at the bottom: under the second option
        // row (or the third, when used), across the knobs too while their
        // second row is empty.
        juce::Rectangle<int> freeBelow (bool thirdRowUsed, bool secondKnobRowUsed) const
        {
            const auto top = (thirdRowUsed ? left[2] : left[1]).getBottom() + 4;
            const auto bottom = right.getBottom();
            // Two rows of knobs and a short band: the band runs the full width
            // under both columns and lists its routes in columns (N16-7: the
            // left column's box held a line and a "+3 more").
            if (secondKnobRowUsed && twoRows)
            {
                const auto fullTop = juce::jmax (top, right.getY() + 2 * oneRowHeight + rowGap + 2);
                if (bottom - fullTop >= 30)
                    return { left[0].getX(), fullTop, right.getRight() - left[0].getX(), bottom - fullTop };
            }
            if (bottom - top < 24)
                return {};
            if (secondKnobRowUsed || ! twoRows)
                return { left[0].getX(), top, left[0].getWidth(), bottom - top };
            return { left[0].getX(), juce::jmax (top, right.getY() + oneRowHeight + 4), right.getRight() - left[0].getX(),
                     bottom - juce::jmax (top, right.getY() + oneRowHeight + 4) };
        }
    };

    static Grid gridFor (juce::Rectangle<int> inner);

    // The free band's lines, small, one under the other.
    void paintInfoLines (juce::Graphics& g, const juce::StringArray& lines, juce::Colour) const;

    // What a source drives, listed in the free band (UI review 8, S8-7 /
    // V8-6: the panel's room says something rather than sitting empty):
    // "DRIVES" then each route's target and depth, two columns.
    void paintRoutes (juce::Graphics& g, Mod::Source source, juce::Colour colour) const;

    // The DRIVES rows jump to their matrix rows (V9-31).
    int routeAt (juce::Point<int> position) const;

    void mouseMove (const juce::MouseEvent& event) override;

    void mouseExit (const juce::MouseEvent&) override;

    void mouseDown (const juce::MouseEvent& event) override;

    // What a DRIVES row stands for (the UI test): its matrix slot, or -1.
    int getNumRouteRows() const { return (int) routeHits.size(); }
    int getRouteSlot (int row) const { return juce::isPositiveAndBelow (row, (int) routeHits.size()) ? routeHits[(size_t) row].second : -2; }
    juce::Point<int> getRouteCentre (int row) const { return routeHits[(size_t) row].first.getCentre(); }

    // MOVE TO LFO: what the old MSEG module's routes do moves onto an LFO
    // drawn the same (SHAPE › MSEG), the one drawn-shape editor (UI review
    // 8, I8-4 / V8-2). Patch loads do it already (review 9, I9-2); the
    // button stays for a patch whose MSEG became movable since.
    bool msegMovable() const;

    void moveMsegToLfo();

    // The MSEG, the Clocked S&H and OP LFO are in the pool only while the
    // patch uses them: the MSEG while a route or an oscillator's ENVELOPE
    // plays it, the Clocked S&H while routed, OP LFO on a patch that plays
    // the Operator Env (or still routes it).
    bool cardInPool (int id) const;

    void updateVisibility();

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    bool compact = false;
    int lastCompactHeight = -1;
    bool lastCompactWide = false;
    std::vector<std::array<std::unique_ptr<PillToggle>, 5>> pills; // the compact layout's SYNC RETRIG KEY LOOP KICK
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    MsegEditor msegEditor;
    ToggleControl msegLoop;
    KnobControl msegRate;
    ClockShView clockView;
    ComboControl clockDiv;
    OperatorLfoEditor opLfoEditor;
    juce::TextButton msegMove;
    juce::Rectangle<int> infoArea; // the grid's free band (routes, the MSEG's notes)
    mutable std::vector<std::pair<juce::Rectangle<int>, int>> routeHits; // DRIVES rows and their matrix slots
    int hoverRoute = -1;
    int selected = 0;
    int lastShape = -1;
};
