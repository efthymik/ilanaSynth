// The VECTOR page, and operatorVoiceFilterOff (PLAY's FILTER card). VectorPage
// is declared here; its member functions are in VectorPage.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../../SectionColours.h"
#include "../AnimationUtils.h"
#include "../FilterDisplay.h"
#include "../FilterWidgets.h"
#include "../IlanaLookAndFeel.h"
#include "../ParamControls.h"
#include "../VectorPad.h"
#include "OscPage.h"
#include "PageHelpers.h"

// An operator (DX7) voice with both filters wide open has none: PLAY's FILTER OFF
// note says so (a macro sitting at 0 pulls nothing).
inline bool operatorVoiceFilterOff (const IlanaSynthAudioProcessor& p)
{
    auto operatorVoice = false;
    for (int osc = 0; osc < OscillatorIds::count && ! operatorVoice; ++osc)
        operatorVoice = p.isOscillatorShown (osc) && OscRole::isOperator (p, osc) && OscRole::usesOperatorEg (p, osc);
    return operatorVoice && FilterDisplay::isPassThrough (p, 0, true) && FilterDisplay::isPassThrough (p, 1, true);
}

// M8.5: the VECTOR page: the vector pad (oscillators at the corners,
// moved by hand, by a path or by its wander). EVOLVE (a macro drifting
// within a range) moved onto the macro's own card in the bottom strip, and
// VECTOR X / Y are dragged from the source bar, their one home (review 8:
// I8-13, I8-14, S8-20). Since UI review 29 the pad has a SHAPE, one corner per
// oscillator (ilana: "a triangle if only using 3 oscs, and a slider if two").
class VectorPage : public juce::Component,
                   private IlanaAnim::FrameTimer
{
public:
    explicit VectorPage (IlanaSynthAudioProcessor& p);

    // Not modulation sources, so not in a source's colour: the accent.
    static juce::Colour colour() { return IlanaTheme::accent(); }
    // PATH and WANDER, the point's motion: a second colour from the section swatches (UI review 31, item 40).
    static juce::Colour motionColour() { return juce::Colour (SectionColours::swatches[4]); }

    static juce::StringArray pathShapeNames() { return { "Circle", "Figure 8", "Square", "Line X", "Line Y" }; }

    // A shape's eight points (x, y in 0..1), in path order.
    static std::array<juce::Point<float>, 8> pathShapePoints (int shapeIndex);

    // The shape menu's id (1..5) the path's points make now, 0 for none.
    int matchedPathShape() const;

    // Writes a shape's points, one undo step (public: the UI test calls it).
    void writePathShape (int shapeIndex);

    // Each corner's name on its menu, by where it sits on the shape.
    static juce::String cornerName (int shapeIndex, int corner);

    void paint (juce::Graphics& g) override;

    void resized() override;

    // A switch level with `knob`'s ring middle and its name on the ring's name line (as FM's EXTRAS row, UI
    // review 28), centred in `cell`'s width.
    static void placeSwitchBeside (ToggleControl& toggle, KnobControl& knob, juce::Rectangle<int> cell);

    // Each corner's LEVEL and on switch follow the oscillator its menu picks (made again when that changes).
    void updateCornerControls();

    // The pad as it comes (a square on OSC 1-4) takes one corner per oscillator the patch has, those oscillators
    // in order. A pad already shaped or re-cornered keeps what it has. (Public: the tests and snapshots call it.)
    void fitShapeToPatch();

private:
    float readParam (const juce::String& id) const;

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    VectorPadDisplay pad;
    std::array<std::unique_ptr<VectorCornerWave>, VectorShape::maxCorners> cornerWaves;
    std::array<std::unique_ptr<ComboControl>, VectorShape::maxCorners> cornerMenus;
    ToggleControl on, path, sync;
    ComboControl shape;
    KnobControl x, y, rate, division, drift, driftRate;
    juce::ComboBox pathShapes;
    std::array<std::unique_ptr<KnobControl>, VectorShape::maxCorners> cornerLevels;
    std::array<std::unique_ptr<ToggleControl>, VectorShape::maxCorners> cornerSwitches;
    std::array<int, VectorShape::maxCorners> cornerOsc { -1, -1, -1, -1, -1, -1 };
    juce::Rectangle<int> vectorCard, cornersBox, motionBox;
    static constexpr int boxHeaderHeight = 24;
    bool shownActive = false;
    int shownOscillators = -1;
    juce::int64 cornerKey = -1; // (the shape and the corners' picks: a change relayouts the corner rows)
};
