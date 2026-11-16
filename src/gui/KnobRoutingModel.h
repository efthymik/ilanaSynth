#pragma once

// KnobRoutingModel (ARCH-10 step 1): the modulation routing a KnobControl shows,
// without any pixels. It reads which mod slots route into the knob's
// destination (the ring and badge list), answers "does this source already
// drive it", builds the two right-click menus, and makes the edits those
// menus and a dropped source ask for (each one named undo step, through the
// processor). KnobControl owns one and forwards to it; the pages and the tests
// keep calling KnobControl's own methods.
//
// Included from ParamControls.h just before KnobControl: it uses ModDotStrip::Dot,
// knobClipboard() and addMidiLearnItems(), which that file defines above.

class KnobRoutingModel
{
public:
    // The knob's menu ids: routeMenuBase + index * stride + the route item's own id (1..15),
    // clear of the menu's other ids (up to 6116).
    static constexpr int routeMenuBase = 7000, routeMenuStride = 100, maxRouteSubmenus = 64;

    // `config` is the knob's ring config (its destination changes with
    // setModulationTarget); it must outlive the model, as the knob's member does.
    void bind (IlanaSynthAudioProcessor* processorIn, juce::RangedAudioParameter* parameterIn,
               const juce::String& parameterIdIn, const ModRingConfig* configIn)
    {
        processor = processorIn;
        parameter = parameterIn;
        parameterId = parameterIdIn;
        config = configIn;
    }

    void setParameter (juce::RangedAudioParameter* parameterIn) { parameter = parameterIn; }

    // The mod destination the knob's ring shows (0: not modulatable).
    int destination() const { return config != nullptr ? config->destination : 0; }

    // The routings into the destination, in slot order, and the source of the
    // strongest unbypassed one (0: none).
    std::vector<ModDotStrip::Dot> routings;
    int dominantSource = 0;

    // The slot already routing this source into the knob, or -1.
    int findRoutingFrom (int source) const
    {
        for (const auto& dot : routings)
            if (dot.source == source)
                return dot.slot;

        return -1;
    }

    bool routesFrom (int source) const { return findRoutingFrom (source) >= 0; }

    // Reads the mod slots that route into the destination into `found` (cheap:
    // raw parameter reads) and sets dominantSource. True when that differs from
    // `routings`; the caller decides whether to take it.
    bool readSlots (std::vector<ModDotStrip::Dot>& found)
    {
        auto strongest = 0.0f;
        dominantSource = 0;

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processor->readModSlot (i);

            if (slot.destination != destination() || slot.source == Mod::Source::None)
                continue;

            const auto bipolar = Mod::isBipolarRoute (slot);
            found.push_back ({ i, (int) slot.source, Mod::visualDepth (slot), slot.bypass, bipolar, (int) slot.polarity,
                               slot.stereo && Mod::canBeStereoAnywhere (slot.destination), Mod::canBeStereoAnywhere (slot.destination) });

            if (! slot.bypass && std::abs (slot.depth) > strongest)
            {
                strongest = std::abs (slot.depth);
                dominantSource = (int) slot.source;
            }
        }

        return found.size() != routings.size()
               || ! std::equal (found.begin(), found.end(), routings.begin(),
                                [] (const auto& a, const auto& b)
                                { return a.slot == b.slot && a.source == b.source && a.bypass == b.bypass
                                         && a.bipolar == b.bipolar && a.polarity == b.polarity && a.stereo == b.stereo
                                         && std::abs (a.depth - b.depth) < 1.0e-4f; });
    }

    // Sets a route's polarity (one undo step).
    void setRoutePolarity (int slot, int choice)
    {
        if (processor == nullptr)
            return;

        processor->performEdit ("Modulation polarity", [this, slot, choice] { processor->setModSlotValue (slot, "pol", (float) choice); });
    }

    // A source dropped on the knob: one new routing at the default depth (one undo step).
    void assignSource (int sourceIndex)
    {
        processor->performEdit ("Add modulation",
                                [this, sourceIndex] { processor->assignModSlot (sourceIndex, destination(), 0.35f); });
    }

    // The undo step a knob-menu result is named after.
    static juce::String undoName (int result, const juce::String& knobName)
    {
        return result == 1000   ? juce::String ("Clear modulation")
               : result == 2000 ? "Reset " + knobName
               : result == 3001 ? "Paste " + knobName
               : result >= 6000 ? juce::String ("Add modulation")
               : result >= 5000 ? juce::String ("Remove modulation")
                                : juce::String ("Add modulation");
    }

    // The routing edits of the knob menu's results (the caller has begun the edit):
    // 1000 clear all, 1..999 a source, 5000+ remove a slot, 6000+ a new LFO or envelope.
    void applyChoice (int result)
    {
        if (result == 1000)
        {
            processor->clearModSlotsForTarget (destination());
        }
        else if (result >= 6100)
        {
            // A new envelope: into the pool, then routed.
            processor->setRevealed (IlanaSynthAudioProcessor::Module::Envelope, result - 6100, true);
            processor->assignModSlot ((int) envelopeSource (result - 6100), destination(), 0.35f);
        }
        else if (result >= 6000)
        {
            processor->setRevealed (IlanaSynthAudioProcessor::Module::Lfo, result - 6000, true);
            processor->assignModSlot ((int) Mod::lfoSourceFor (result - 6000), destination(), 0.35f);
        }
        else if (result >= 5000)
        {
            processor->clearModSlot (result - 5000);
        }
        else
        {
            processor->assignModSlot (result - 1, destination(), 0.35f);
        }
    }

    // How the routing's polarity reads in a menu: up, down, both ways, or
    // the source's own range.
    static juce::String polaritySymbol (const ModDotStrip::Dot& dot)
    {
        switch ((Mod::Polarity) dot.polarity)
        {
            case Mod::Polarity::Unipolar: return "+";
            case Mod::Polarity::Negative: return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
            case Mod::Polarity::Bipolar:  return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1"));
            default:                      return dot.bipolar ? juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) : "+";
        }
    }

    // "Modulate with": LFOs, envelopes, macros and the rest, by their
    // ModNames names (the UI test reads it).
    juce::PopupMenu buildModulateWithMenu() const
    {
        juce::PopupMenu sourceMenu, lfoMenu, envMenu, macroMenu, otherMenu;

        // Only the LFOs and envelopes in the pools (a source that has no
        // card isn't offered); "New LFO" / "New envelope" adds the next.
        // The LFOs' B outputs file under LFOs.
        for (const auto i : ModNames::sourcesInMenuOrder())
        {
            const auto source = (Mod::Source) i;
            if (source == Mod::Source::InputEnv && ! IlanaSynthAudioProcessor::isEffectBuild)
                continue;
            if (! modSourceInPatch (*processor, source) && ! routesFrom (i))
                continue;
            const auto group = ModNames::groupOf (i);
            auto& target = group == ModNames::SourceGroup::lfo || group == ModNames::SourceGroup::lfoB ? lfoMenu
                         : group == ModNames::SourceGroup::envelope ? envMenu
                         : group == ModNames::SourceGroup::macro ? macroMenu : otherMenu;
            target.addItem (i + 1, ModNames::source (i, processor), true, routesFrom (i));
        }

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processor->isLfoShown (lfo))
            {
                lfoMenu.addSeparator();
                lfoMenu.addItem (6000 + lfo, "New LFO  (LFO " + juce::String (lfo + 1) + ")");
                break;
            }

        for (int env = 0; env < 16; ++env)
            if (! envelopeShown (*processor, env))
            {
                envMenu.addSeparator();
                envMenu.addItem (6100 + env, "New envelope  (" + ModNames::source ((int) envelopeSource (env)) + ")");
                break;
            }

        sourceMenu.addSubMenu ("LFOs", lfoMenu);
        sourceMenu.addSubMenu ("Envelopes", envMenu);
        sourceMenu.addSubMenu ("Macros", macroMenu);
        sourceMenu.addSubMenu ("Performance and more", otherMenu);
        return sourceMenu;
    }

    // The right-click menu (the snapshot tool draws it offscreen: a real
    // menu window can't open under its X server).
    // `extraMenu`: items the knob's owner puts first in its right-click menu,
    // with their own actions (the PATCH tab's effect windows: assign to an LFO box).
    juce::PopupMenu buildModMenu (const std::function<void (juce::PopupMenu&)>& extraMenu) const
    {
        juce::PopupMenu menu;

        if (extraMenu)
        {
            extraMenu (menu);
            if (menu.getNumItems() > 0)
                menu.addSeparator();
        }

        if (processor == nullptr)
            return menu;

        if (destination() != 0)
        {
            menu.addSubMenu ("Modulate with", buildModulateWithMenu());

            if (! routings.empty())
            {
                // Each routing into the knob: its polarity, stereo, amount,
                // bypass and remove (the ring's own menu), so none of it
                // needs the thin ring or the MATRIX.
                for (size_t i = 0; i < routings.size() && i < (size_t) maxRouteSubmenus; ++i)
                {
                    const auto& dot = routings[i];
                    juce::PopupMenu routeMenu;
                    ModDotStrip::addRouteItems (routeMenu, dot, routeMenuBase + (int) i * routeMenuStride, false);
                    menu.addSubMenu (ModNames::source (dot.source, processor) + "  " + polaritySymbol (dot) + " "
                                         + juce::String (Mod::shownPercent (dot.depth, dot.bipolar)) + "%"
                                         + (dot.bypass ? "  (bypassed)" : ""),
                                     routeMenu, true, nullptr, false, 0);
                }

                menu.addItem (1000, "Clear all modulation to this knob");
            }

            menu.addSeparator();
        }

        if (parameter != nullptr)
            menu.addItem (2000, "Reset to default  (double-click)");

        menu.addItem (3500, juce::String (juce::CharPointer_UTF8 ("Type a value\xe2\x80\xa6")));
        menu.addSeparator();
        menu.addItem (3000, "Copy value");

        if (knobClipboard().isNotEmpty())
            menu.addItem (3001, "Paste value");

        const auto isMacro = parameterId.startsWith ("macro") && ! parameterId.containsChar ('_');

        if (isMacro)
        {
            const auto macroIndex = parameterId.getTrailingIntValue() - 1;

            menu.addSeparator();
            menu.addItem (4000, "MIDI Learn  (CC " + juce::String (processor->getMacroCc (macroIndex)) + ")");
        }
        else if (parameter != nullptr && parameter->isAutomatable())
        {
            menu.addSeparator();
            addMidiLearnItems (menu, *processor, parameterId);
        }

        return menu;
    }

private:
    IlanaSynthAudioProcessor* processor = nullptr;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::String parameterId;
    const ModRingConfig* config = nullptr;
};
