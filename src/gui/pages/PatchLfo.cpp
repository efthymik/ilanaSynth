#include "PatchLfo.h"

FxLfoControls::FxLfoControls (IlanaSynthAudioProcessor& p, int lfoId)
    : processorRef (p), id (lfoId)
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_shape", 1 }, "Shape", FxLfo::shapeNames(), 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lfo_sync", 1 }, "Sync", false));
    NormalisableRange<float> rateRange (FxLfo::minRateHz, FxLfo::maxRateHz);
    rateRange.setSkewForCentre (1.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_rate", 1 }, "Rate", rateRange, 0.5f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       { return v < 1.0f ? String (v, 2) + " Hz" : String (v, 1) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_div", 1 }, "Division", FxLfo::divisionNames(), FxLfo::defaultDivision));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_depth", 1 }, "Depth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       { return String (roundToInt (v)) + "%"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_phase", 1 }, "Phase", NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       { return String (roundToInt (v)) + juce::String::fromUTF8 ("\xc2\xb0"); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_polarity", 1 }, "Polarity", StringArray { "Bipolar", "Unipolar" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_smooth", 1 }, "Smooth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       { return v <= 0.0f ? String ("Off") : String (roundToInt (v)) + "%"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lfo_retrig", 1 }, "Retrigger", false));
    state = std::make_unique<AudioProcessorValueTreeState> (holder, nullptr, "FxLfo", std::move (layout));

    const auto colour = fxLfoColour (id);
    shape = std::make_unique<ComboControl> (*state, "lfo_shape", "SHAPE");
    sync = std::make_unique<ToggleControl> (*state, "lfo_sync", "SYNC");
    rate = std::make_unique<KnobControl> (*state, "lfo_rate", "RATE", colour, false);
    division = std::make_unique<ComboControl> (*state, "lfo_div", "RATE");
    depth = std::make_unique<KnobControl> (*state, "lfo_depth", "DEPTH", colour, false);
    phase = std::make_unique<KnobControl> (*state, "lfo_phase", "PHASE", colour, false);
    polarity = std::make_unique<ComboControl> (*state, "lfo_polarity", "POLARITY");
    smooth = std::make_unique<KnobControl> (*state, "lfo_smooth", "SMOOTH", colour, false);
    retrig = std::make_unique<ToggleControl> (*state, "lfo_retrig", "RETRIG");
    for (auto* knob : { rate.get(), depth.get(), phase.get(), smooth.get() })
        knob->setInlineKnob (true);
    shape->setTooltip ("SHAPE: the LFO's wave. Sample & Hold, Smooth Random, Drunk and Chaos are random. Drawn and Steps are yours: draw on the picture.");
    sync->setTooltip ("SYNC: the rate follows the song's tempo, from 8 bars to 1/32 (with the transport playing, in step with the bar).");
    rate->setTooltip ("RATE: how fast it moves, 0.02 to 20 Hz.");
    division->setTooltip ("RATE: one cycle per this much of the bar.");
    depth->setTooltip ("DEPTH: how far it moves every knob it is assigned to (each knob has its own amount on the cable).");
    phase->setTooltip ("PHASE: where in its cycle it starts (with SYNC, against the bar).");
    smooth->setTooltip ("SMOOTH: glides the LFO's output, as a fraction of a cycle. Turns Square, Sample & Hold and Steps into slewed curves.");
    retrig->setTooltip ("RETRIG: every note starts the LFO's cycle again, from PHASE. With SYNC and the transport playing, the bar decides instead.");
    polarity->setTooltip ("POLARITY: Bipolar moves a knob both ways round where it is set; Unipolar moves it one way from there.");

    show();
    for (auto* parameter : state->processor.getParameters())
        parameter->addListener (this);
}

FxLfoControls::~FxLfoControls ()
{
    for (auto* parameter : state->processor.getParameters())
        parameter->removeListener (this);
    // (The controls hold attachments to the parameters: they go first.)
    shape.reset();
    sync.reset();
    rate.reset();
    division.reset();
    depth.reset();
    phase.reset();
    polarity.reset();
    smooth.reset();
    retrig.reset();
    if (gestureOpen)
        processorRef.endEdit();
}

void FxLfoControls::show ()
{
    if (gestureOpen)
        return;
    const auto patchPointer = processorRef.getFxPatchShared(); // (no copy)
    const auto& patch = *patchPointer;
    const auto* lfo = patch.findLfo (id);
    if (lfo == nullptr)
        return;
    const juce::ScopedValueSetter<bool> quiet (syncing, true);
    const auto& s = lfo->settings;
    set ("lfo_shape", (float) s.shape);
    set ("lfo_sync", s.sync ? 1.0f : 0.0f);
    set ("lfo_rate", s.rateHz);
    set ("lfo_div", (float) s.division);
    set ("lfo_depth", s.depth * 100.0f);
    set ("lfo_phase", s.phase * 360.0f);
    set ("lfo_polarity", s.bipolar ? 0.0f : 1.0f);
    set ("lfo_smooth", s.smooth * 100.0f);
    set ("lfo_retrig", s.retrig ? 1.0f : 0.0f);
    shown = s;
}

std::vector<juce::Component*> FxLfoControls::windowItems () const
{
    return { shape.get(), sync.get(), shown.sync ? (juce::Component*) division.get() : (juce::Component*) rate.get(), depth.get(),
             smooth.get(), phase.get(), polarity.get(), retrig.get() };
}

std::vector<juce::Component*> FxLfoControls::allItems () const
{
    return { shape.get(), sync.get(), rate.get(), division.get(), depth.get(), phase.get(), polarity.get(), smooth.get(), retrig.get() };
}

void FxLfoControls::set (const char* paramId, float value)
{
    auto* parameter = state->getParameter (paramId);
    const auto normalised = parameter->convertTo0to1 (value);
    if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
        parameter->setValueNotifyingHost (normalised);
}

float FxLfoControls::get (const char* paramId) const
{
    const auto* parameter = state->getParameter (paramId);
    return parameter->convertFrom0to1 (parameter->getValue());
}

FxLfo::Settings FxLfoControls::fromControls () const
{
    FxLfo::Settings s;
    s.shape = juce::roundToInt (get ("lfo_shape"));
    s.sync = get ("lfo_sync") > 0.5f;
    s.rateHz = get ("lfo_rate");
    s.division = juce::roundToInt (get ("lfo_div"));
    s.depth = get ("lfo_depth") / 100.0f;
    s.phase = juce::jlimit (0.0f, 1.0f, get ("lfo_phase") / 360.0f);
    s.bipolar = get ("lfo_polarity") < 0.5f;
    s.smooth = get ("lfo_smooth") / 100.0f;
    s.retrig = get ("lfo_retrig") > 0.5f;
    s.drawn = shown.drawn; // (drawn on the picture, not a control)
    return s.sanitised();
}

void FxLfoControls::parameterValueChanged (int, float)
{
    if (syncing)
        return;
    const auto next = fromControls();
    if (next == shown)
        return;
    const auto layoutChanged = next.sync != shown.sync;
    shown = next;
    if (gestureOpen)
        processorRef.setFxLfoSettings (id, next);
    else
        processorRef.performEdit (fxLfoName (id) + " settings", [this, next] { processorRef.setFxLfoSettings (id, next); });
    if (layoutChanged && onLayoutChanged)
        juce::MessageManager::callAsync ([cb = onLayoutChanged] { cb(); });
}

void FxLfoControls::parameterGestureChanged (int, bool starting)
{
    if (starting && ! gestureOpen)
    {
        gestureOpen = true;
        processorRef.beginEdit (fxLfoName (id) + " settings");
    }
    else if (! starting && gestureOpen)
    {
        gestureOpen = false;
        processorRef.endEdit();
    }
}

void FxLfoDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    IlanaTheme::paintWell (g, bounds, 6.0f);
    const auto detailed = bounds.getHeight() >= 100.0f;
    paintFxLfoWave (g, plotArea(), live.has_value() ? *live : controls.settings(), fxLfoColour (controls.id),
                    processorRef.getFxLfoPhase (controls.id), true, true, detailed);
}

void FxLfoDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! canDraw())
        return;
    if (e.getNumberOfClicks() > 1)
    {
        auto next = controls.settings();
        next.drawn = FxLfo::defaultDrawn();
        processorRef.performEdit (fxLfoName (controls.id) + " drawing", [this, next] { processorRef.setFxLfoSettings (controls.id, next); });
        controls.show();
        repaint();
        return;
    }
    processorRef.beginEdit (fxLfoName (controls.id) + " drawing");
    editing = true;
    live = controls.settings();
    lastIndex = -1;
    drawAt (e.position);
}

void FxLfoDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (editing)
        drawAt (e.position);
}

void FxLfoDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! editing)
        return;
    editing = false;
    processorRef.endEdit();
    live.reset();
    controls.show();
    repaint();
}

juce::MouseCursor FxLfoDisplay::getMouseCursor ()
{
    return canDraw() ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor;
}

juce::Rectangle<float> FxLfoDisplay::plotArea () const
{
    const auto bounds = getLocalBounds().toFloat();
    return bounds.getHeight() >= 100.0f ? bounds.reduced (34.0f, 8.0f).withTrimmedTop (16.0f).withTrimmedBottom (16.0f)
                                        : bounds.reduced (10.0f, 8.0f);
}

void FxLfoDisplay::drawAt (juce::Point<float> position)
{
    if (! live.has_value())
        return;
    auto& settings = *live;
    const auto plot = plotArea();
    const auto cycles = 2.0;
    const auto stepped = FxLfo::shapeOf (settings.shape) == LfoShapes::Steps;
    const auto along = juce::jlimit (0.0, 1.0, (double) ((position.x - plot.getX()) / plot.getWidth())) * cycles + (double) settings.phase;
    const auto tablePhase = along - std::floor (along);
    const auto index = juce::jlimit (0, FxLfo::drawSteps - 1, stepped ? (int) (tablePhase * FxLfo::drawSteps)
                                                                     : juce::roundToInt (tablePhase * FxLfo::drawSteps) % FxLfo::drawSteps);

    // Back from the picture to the shape's value: undo POLARITY and DEPTH.
    const auto centreY = settings.bipolar ? plot.getCentreY() : plot.getBottom();
    const auto half = settings.bipolar ? plot.getHeight() * 0.5f : plot.getHeight();
    const auto output = juce::jlimit (-1.0f, 1.0f, (centreY - position.y) / half);
    const auto depth = juce::jmax (0.05f, settings.depth);
    const auto value = juce::jlimit (-1.0f, 1.0f, settings.bipolar ? output / depth : 2.0f * output / depth - 1.0f);

    // Every point the pointer passed since the last event (the short way round).
    auto from = lastIndex < 0 ? index : lastIndex;
    auto delta = index - from;
    if (delta > FxLfo::drawSteps / 2)
        delta -= FxLfo::drawSteps;
    else if (delta < -FxLfo::drawSteps / 2)
        delta += FxLfo::drawSteps;
    const auto steps = std::abs (delta);
    for (int k = 0; k <= steps; ++k)
    {
        const auto at = ((from + (delta < 0 ? -k : k)) % FxLfo::drawSteps + FxLfo::drawSteps) % FxLfo::drawSteps;
        const auto blend = steps == 0 ? 1.0f : (float) k / (float) steps;
        settings.drawn[(size_t) at] = lastIndex < 0 ? value : lastValue + (value - lastValue) * blend;
    }
    lastIndex = index;
    lastValue = value;
    processorRef.setFxLfoSettings (controls.id, settings.sanitised());
    repaint();
}

FxLfoWindow::FxLfoWindow (IlanaSynthAudioProcessor& p, int lfoId)
    : id (lfoId), controls (p, lfoId), display (p, controls)
{
    setName ("FX WINDOW " + fxLfoName (lfoId)); // (the UI test finds it by name)
    constrainer.owner = this;
    closeButton.setTooltip ("Close this window");
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
    closeButton.onClick = [this] { if (onClose) onClose (*this); };
    addAndMakeVisible (closeButton);
    addAndMakeVisible (display);
    for (auto* item : controls.allItems())
        addChildComponent (item);
    controls.onLayoutChanged = [safe = juce::Component::SafePointer<FxLfoWindow> (this)] { if (safe != nullptr) safe->resized(); };
    const auto rows = 3;
    setSize (3 * cellWidth + 2 * gap + 2 * pad, titleHeight + displayHeight + gap + rows * cellHeight + (rows - 1) * gap + pad);
}

void FxLfoWindow::refresh ()
{
    const auto synced = controls.isSynced();
    controls.show();
    if (synced != controls.isSynced())
        resized();
    display.repaint();
}

void FxLfoWindow::paint (juce::Graphics& g)
{
    using namespace IlanaTheme;
    const auto colour = fxLfoColour (id);
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f).reduced (2.0f), 10.0f);
    paintCard (g, bounds.reduced (1.0f), 10.0f, colour.withAlpha (0.55f));
    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (pad, 0);
    paintTag (g, { (float) title.getX() + 4.0f, (float) title.getCentreY() }, colour);
    g.setColour (Ui::text);
    g.setFont (font (TextSize::body, true));
    g.drawText (fxLfoName (id), title.withTrimmedLeft (Space::s16).withTrimmedRight (60), juce::Justification::centredLeft, true); // off-grid
}

void FxLfoWindow::resized ()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (titleHeight).reduced (pad, 0);
    closeButton.setBounds (title.removeFromRight (28).withSizeKeepingCentre (28, 28)); // off-grid
    area = area.withTrimmedLeft (pad).withTrimmedRight (pad).withTrimmedBottom (pad);
    display.setBounds (area.removeFromTop (displayHeight));
    area.removeFromTop (gap);
    for (auto* item : controls.allItems())
        item->setVisible (false);
    const auto items = controls.windowItems();
    for (size_t k = 0; k < items.size(); ++k)
    {
        const auto column = (int) k % 3, row = (int) k / 3;
        items[k]->setBounds (area.getX() + column * (cellWidth + gap), area.getY() + row * (cellHeight + gap), cellWidth, cellHeight);
        items[k]->setVisible (true);
    }
}

void FxLfoWindow::mouseDown (const juce::MouseEvent& e)
{
    toFront (false);
    dragging = e.y < titleHeight;
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void FxLfoWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        dragger.dragComponent (this, e, &constrainer);
}

void FxLfoWindow::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onMoved)
        onMoved (*this);
    dragging = false;
}

void FxLfoWindow::setLimits (juce::Rectangle<int> area)
{
    limits = area;
    setBounds (getBounds().constrainedWithin (limits));
}

FxLfoAmounts::FxLfoAmounts (IlanaSynthAudioProcessor& p)
    : processorRef (p)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int k = 0; k < maxShown; ++k)
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amt" + juce::String (k), 1 }, "Amount",
                                                                 juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 50.0f,
                                                                 juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                     [] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; })));
    state = std::make_unique<juce::AudioProcessorValueTreeState> (holder, nullptr, "FxLfoAmounts", std::move (layout));
    for (int k = 0; k < maxShown; ++k)
    {
        knobs[(size_t) k] = std::make_unique<KnobControl> (*state, "amt" + juce::String (k), "AMOUNT", IlanaTheme::accent(), false);
        knobs[(size_t) k]->setInlineKnob (true);
        knobs[(size_t) k]->setTooltip ("How far the LFO moves this knob at full DEPTH, either way. Not modulatable.");
    }
    for (auto* parameter : state->processor.getParameters())
        parameter->addListener (this);
}

FxLfoAmounts::~FxLfoAmounts ()
{
    for (auto* parameter : state->processor.getParameters())
        parameter->removeListener (this);
    for (auto& knob : knobs)
        knob.reset();
    if (gestureOpen)
        processorRef.endEdit();
}

void FxLfoAmounts::show (int lfo, int slot, const std::vector<std::pair<juce::String, juce::String>>& targets, const juce::String& effectName,
               juce::Colour colour)
{
    if (gestureOpen)
        return;
    lfoId = lfo;
    slotIndex = slot;
    params.clear();
    const auto patchPointer = processorRef.getFxPatchShared(); // (no copy)
    const auto& patch = *patchPointer;
    const juce::ScopedValueSetter<bool> quiet (syncing, true);
    for (const auto& route : patch.lfoRoutes)
        if (route.lfo == lfo && route.slot == slot && (int) params.size() < maxShown)
        {
            const auto k = (int) params.size();
            params.push_back (route.param);
            auto name = route.param;
            for (const auto& [param, label] : targets)
                if (param == route.param)
                    name = FxLfo::shortName (label, effectName);
            auto& knob = *knobs[(size_t) k];
            if (knob.getLabelText() != name.toUpperCase())
                knob.setLabelText (name.toUpperCase());
            knob.setIdentityColour (colour);
            auto* parameter = state->getParameter ("amt" + juce::String (k));
            const auto normalised = parameter->convertTo0to1 (route.amount * 100.0f);
            if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
                parameter->setValueNotifyingHost (normalised);
        }
}

int FxLfoAmounts::totalAssigned () const
{
    const auto patchPointer = processorRef.getFxPatchShared(); // (no copy)
    const auto& patch = *patchPointer;
    return (int) std::count_if (patch.lfoRoutes.begin(), patch.lfoRoutes.end(), [this] (const auto& r) { return r.lfo == lfoId && r.slot == slotIndex; });
}

void FxLfoAmounts::parameterValueChanged (int index, float)
{
    if (syncing || ! juce::isPositiveAndBelow (index, (int) params.size()))
        return;
    auto* parameter = state->getParameter ("amt" + juce::String (index));
    const auto amount = parameter->convertFrom0to1 (parameter->getValue()) / 100.0f;
    const auto param = params[(size_t) index];
    if (gestureOpen)
        processorRef.setFxLfoAmount (lfoId, slotIndex, param, amount);
    else
        processorRef.performEdit ("LFO amount", [this, param, amount] { processorRef.setFxLfoAmount (lfoId, slotIndex, param, amount); });
}

void FxLfoAmounts::parameterGestureChanged (int, bool starting)
{
    if (starting && ! gestureOpen)
    {
        gestureOpen = true;
        processorRef.beginEdit ("LFO amount");
    }
    else if (! starting && gestureOpen)
    {
        gestureOpen = false;
        processorRef.endEdit();
    }
}
