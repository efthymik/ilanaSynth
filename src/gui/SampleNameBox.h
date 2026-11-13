#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The sample's name in the card header: a packed menu box ("SAMPLE  Rhodes C3.wav") whose click
// opens the load menu instead of a list.
class SampleNameBox : public juce::ComboBox
{
public:
    SampleNameBox()
    {
        setInterceptsMouseClicks (true, false);
        getProperties().set ("packedMenu", true);
    }

    std::function<void()> onPopup;

    // The load menu opens on the press only, never through ComboBox's "menu
    // active" flag. The base class sets that flag on the press and clears it
    // only in its own list's callback; our menu has its own callback, so the
    // flag stayed set after the first open and every later click was swallowed
    // until the editor was rebuilt (ilana: "granular source drop down will
    // sometimes fail to appear"). PopupCombo (ParamControls.h) does the same.
    void mouseDown (const juce::MouseEvent& event) override
    {
        // (Asynchronously, as ComboBox does: the press that dismissed another
        // open menu gets to finish first.)
        if (isEnabled() && ! event.mods.isPopupMenu())
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SampleNameBox> (this)]
                                             { if (safe != nullptr) safe->openMenu(); });
    }

    void mouseDrag (const juce::MouseEvent&) override {}
    void mouseUp (const juce::MouseEvent&) override {}

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
        {
            openMenu();
            return true;
        }
        return false; // (one item: nothing to step through)
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        juce::Component::mouseWheelMove (event, wheel); // (the page scrolls; no item to step to)
    }

    // (Reached by accessibility's "press" action: clear the flag it set first.)
    void showPopup() override
    {
        hidePopup();
        openMenu();
    }

    // A press can arrive twice in a few milliseconds (a touchpad tap, a double
    // event from the host); the second would close the first's menu as it opened.
    void openMenu()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastOpen < 150.0)
            return;
        lastOpen = now;
        if (onPopup != nullptr)
            onPopup();
    }

    // True while the base class's flag is clear (the UI test checks it).
    bool menuFlagClear() const { return ! isPopupActive(); }

    void setCaptionAndName (const juce::String& caption, const juce::String& name)
    {
        getProperties().set ("textPrefix", caption);
        if (getText() != name)
        {
            clear (juce::dontSendNotification);
            addItem (name, 1);
            setSelectedId (1, juce::dontSendNotification);
        }
        repaint();
    }

private:
    double lastOpen = -1.0e9;
};
