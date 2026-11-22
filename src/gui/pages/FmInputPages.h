// The FM tab (FmPage) and ilanaSynth FX's INPUT tab (InputPage). The classes
// are declared here; their member functions are in FmInputPages.cpp.
#pragma once

#include <map>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../AnimationUtils.h"
#include "../CardTabs.h"
#include "../EnvelopeDisplay.h"
#include "../FmDiagram.h"
#include "../FmOperatorInfo.h"
#include "../FmWidgets.h"
#include "../IlanaLookAndFeel.h"
#include "../OperatorPoolCards.h"
#include "../OscPicker.h"
#include "../OutputView.h"
#include "../ParamControls.h"
#include "OscPage.h"
#include "PageHelpers.h"

// FM between six oscillators: the algorithms, the operator diagram and the
// selected operator's settings on the left; the full matrix of amounts on the
// right (rows = from, columns = to, plus the noise operator), with the FM
// style and, under it, OSC 1 and OSC 2's ring mod and hard sync.
//
// An oscillator on the Operator Env (a DX7 voice's operators) shows its
// envelope as a graph you drag, in the synth's words and units: ATTACK to
// RELEASE in ms, PEAK to END in dB, its output as LEVEL (UI review 6, I6-7 to
// I6-9). The envelope is the one editor MOD's OP ENV card shows too
// (OperatorEnvEditor, UI review 8, I8-6). The voice's pitch envelope and LFO
// are MOD's OP PITCH and OP LFO; the card's header links there (I8-8, S8-3,
// V8-21).
class FmPage : public juce::Component,
               private IlanaTheme::IdentityColourListener,
               private IlanaAnim::FrameTimer
{
    // One operator's settings: its tuning and feedback style, its ENVELOPE,
    // and its pitch and levels in one order on every page (RATIO, SEMI,
    // FINE, OUTPUT, LEVEL: UI review 8, V8-5). LEVEL is always the
    // oscillator's level (%); on the Operator Env, OUTPUT is its output level
    // (dB: review 9, I9-7, was LEVEL beside a TRIM); otherwise KEY SCALE is
    // its key scaling (KEY LVL until review 9, I9-26).
    struct OperatorControls
    {
        OperatorControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefixIn, juce::Colour colour, int index)
            : prefix (prefixIn),
              tune (state, prefix + "_tune", "TUNING"),
              snap (state, prefix + "_ratio_snap", "SNAP"),
              feedbackType (state, prefix + "_fb_type", "FEEDBACK TYPE"),
              ampEnv (state, prefix + "_amp_env", FmOperatorInfo::envelopeLabel),
              ratio (state, prefix + "_ratio", "RATIO", colour, false),
              fixedHz (state, prefix + "_fixed_hz", "FIXED", colour, false),
              semi (state, prefix + "_semi", "SEMI", colour, false),
              fine (state, prefix + "_fine", "FINE", colour, false),
              level (state, prefix + "_level", "LEVEL", colour, false),
              keyLevel (state, prefix + "_key_level", "KEY SCALE", colour, false),
              egOut (state, prefix + "_eg_out", "OUTPUT", colour, false),
              // The operator's feedback, beside FB TYPE (it is also the matrix
              // diagonal's cell: review 10, I10-4).
              feedback (state, FmDiagram::routeId (index, index), "FEEDBACK", colour, false)
        {
            FmOperatorInfo::sectionEnvelopeMenu (ampEnv.getComboBox());
        }

        juce::String prefix;
        ComboControl tune, snap, feedbackType, ampEnv;
        KnobControl ratio, fixedHz, semi, fine, level, keyLevel, egOut, feedback;

        std::vector<juce::Component*> all()
        {
            return { &tune, &snap, &feedbackType, &ampEnv, &ratio, &fixedHz, &semi, &fine, &level, &keyLevel, &egOut, &feedback };
        }
    };

public:
    explicit FmPage (IlanaSynthAudioProcessor& p);

    ~FmPage() override { IlanaTheme::identityColourListeners (&processorRef).remove (this); }

    // Colour coding: the send knobs, an operator's controls and its graph in
    // the oscillator's new colour.
    void identityColoursChanged() override;

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }
    static juce::Colour noiseColour() { return IlanaTheme::Ui::text2; }

    int getSelectedOperator() const { return selectedOperator; }

    void selectOperator (int op);

    void paint (juce::Graphics& g) override;

    void visibilityChanged() override;

    // The matrix follows the oscillators added to the patch.
    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override;

    // Columns of oscillators that ignore FM (sample, granular, a string
    // not set to Osc In) are greyed out and can't be edited; routes already
    // there stay in the patch but do nothing. A switched-off oscillator's
    // row and column are greyed the same way (I6-37, V6-13).
    bool refreshFmInputs (bool force);

    bool refreshShown();

    void resized() override;

private:
    float read (const juce::String& id) const;

public:
    // The ALGORITHMS heading's note: the DX7 algorithm a patch matches by its
    // number, else a basic routing's name, else CUSTOM ROUTING (UI review 6,
    // I6-6 and I6-41).
    juce::String getAlgorithmLabel() const;

private:
    // "x1.00 · Operator Env · OUT": the operator in a line (V6-33).
    juce::String operatorText() const;

    bool usesOperatorEnv (int op) const { return FmOperatorInfo::usesOperatorEnv (processorRef, op); }
    bool isFmType (int op) const { return OscRole::isOperator (processorRef, op); }

    bool anyOperatorEnv() const;

    // A cell's knob can be edited: its source plays, and its target plays and
    // takes FM (source -1: the noise row).
    bool isLiveCell (int source, int target) const;

    // The readout of an empty matrix: how to add a route, or why none can be
    // added (no shown oscillator takes FM in, e.g. a Physical one).
    juce::String noRoutesText() const;

    float cellAmount (int source, int target) const;

    KnobControl& cellKnob (int source, int target);

    // An empty route is a dot that turns into its knob under the mouse, so
    // the routes in use stand out from the grid (V6-15, S6-17); a dead one is
    // a faint knob or nothing.
    bool refreshCellKnobs (bool force = false);

    void updateOperatorVisibility();

    // The operator card's two looks: the stacked row of the Operator Env's
    // host area, or the plain card's grid of inline knobs (the design's
    // 3 x 2 grid: the dial with its name and value beside it).
    static void setOperatorCardStyle (OperatorControls& controls, bool grid);

    // An operator on any envelope but the Operator Env: TUNING, SNAP and
    // ENVELOPE in one row, two rows of three inline knobs, FB TYPE with the
    // hint under them, and the envelope's graph filling the height at the
    // right; no empty strip anywhere in the card.
    void layoutPlainOperator (OperatorControls& controls);

    void layoutOperatorCard();

    // An operator on the Operator Env (ilana, 2026-11-03, "the op env is not well integrated", option A): the operator's
    // own controls in a column at the left (TUNING and SNAP, then two rows of three: the dials and FEEDBACK's TYPE), and
    // at the right the envelope as one panel: its tabs and the card's ENVELOPE menu on the first line, the graph, and the
    // eight stage knobs under it.
    void layoutOperatorEnvCard (OperatorControls& controls, juce::Rectangle<int> inner);

    // One route of the ROUTES list: an oscillator into another, into itself (feedback), or the noise (-1) into one.
    struct Route { int source = 0, target = 0; };

    std::vector<Route> activeRoutes() const;

    juce::String routesSignature() const;

    juce::String routeName (const Route& route) const;

    juce::TextButton& removeButtonFor (const Route& route);

    void showAddRouteMenu();

    // ROUTES (a row per route: its colour, its name, its depth, a remove cross), + ADD ROUTE, FM MODE, then the
    // extras on one grid: RING MOD, SYNC 2 TO 1, the noise's type and COLOUR.
    void layoutRoutes();

    void layoutCableKnobs();

    void paintRoutes (juce::Graphics& g);

    // RING MOD, SYNC 2 TO 1 and the NOISE FM row mean nothing to a DX7
    // voice: on a patch whose oscillators play the Operator Env they fold
    // behind a MORE line unless one is in use (S8-21).
    bool matrixExtrasInUse() const;

    bool matrixExtrasShown() const { return extrasOpen || matrixExtrasInUse(); }

    static constexpr int operatorCardHeight = 240, plainCardHeight = 168;

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {}, playing {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    OutputView outputView;
    OperatorEnvEditor envelope;
    OscPicker picker;
    juce::TextButton pitchLfoLink, makeOperator;
    CardTabs pageTabs;
    ComboControl mode;
    ToggleControl hardSync;
    EffectRules effectRules;
    std::unique_ptr<KnobControl> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    bool extrasOpen = false, lastExtrasShown = true;
    juce::TextButton moreButton, addRouteButton;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> cableKnobs;
    std::map<std::pair<int, int>, std::unique_ptr<juce::TextButton>> removeButtons;
    std::vector<std::pair<Route, juce::Rectangle<int>>> routeRows;
    juce::Rectangle<int> extrasTitle, noiseTypeTitle, outputTitle;
    juce::String lastRoutesSignature;
    static constexpr int cell = 35;
    // An operator on another envelope shows that envelope where an Operator
    // Env operator shows its own (one card anatomy, I8-11, V8-6).
    std::unique_ptr<EnvelopeDisplay> ampGraph;
    int ampGraphEnvelope = -1, ampGraphOperator = -1;
    std::unique_ptr<KnobControl> noiseColourKnob;
    juce::ComboBox noiseTypeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseTypeAttachment;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    juce::Rectangle<int> matrixCard, operatorCard, algorithmsTitle, ampHint;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count * 4> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0, tabsLeft = 0, lastDiagramMinimum = 0, lastDiagramPreferred = 0;
    bool lastAnyOperatorEnv = false;
    juce::String lastAlgorithmLabel, hoverAlgorithm;
};

// M7.5: ilanaSynth FX's INPUT page: the input's level and envelope, its
// gain and trigger, where it goes, and quick starts.
class InputPage : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    explicit InputPage (IlanaSynthAudioProcessor& p);

    static juce::Colour inputColour() { return juce::Colour (0xff5fd3ff); }
    static juce::Colour routeColour() { return juce::Colour (0xffffb454); }

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    void timerCallback() override;

    int readChoice (const juce::String& id) const;

    IlanaSynthAudioProcessor& processorRef;
    KnobControl gain, dry;
    ComboControl trigger;
    KnobControl threshold, note, attack, release, toBody, toStrings;
    std::array<juce::TextButton, 4> quickStarts;
    juce::Rectangle<int> inputCard, routeCard, startCard, meter, helpArea;
    float shownLevel = 0.0f, shownEnvelope = 0.0f;
};
