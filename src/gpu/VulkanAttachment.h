#pragma once

#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ilana::vulkan
{
// Draws a component (normally the editor) and its children through Vulkan:
// a child native window that ignores the mouse and keyboard covers the
// component, a swapchain presents a Canvas into it, and the component's
// repaints become dirty rectangles on that canvas (it installs itself as
// the component's CachedComponentImage, as juce::OpenGLContext does), so the
// peer no longer paints it. Pop-up menus, tooltips and other desktop windows
// keep their own renderer.
//
// attachTo returns nullptr when Vulkan can't run here (the caller then keeps
// the old renderer); a device lost or a surface failure later detaches by
// itself and calls onFailure, so the caller can fall back.
class Attachment
{
public:
    static std::unique_ptr<Attachment> attachTo (juce::Component&, std::function<void()> onFailure = {});
    virtual ~Attachment() = default;

    virtual bool isActive() const = 0;
    virtual juce::String describe() const = 0; // device name, swapchain size and present mode

    // For tests (ilanaSnapshot --vulkanlive): what the canvas holds now (the
    // picture last presented), how many frames were presented and what they
    // cost on the message thread, and the input events the child window asks
    // for (X11: every client's event mask on it; -1 elsewhere), which must
    // be none so the mouse and keyboard reach the editor's own window.
    struct Stats
    {
        int framesPresented = 0;
        int acquireTimeouts = 0;
        double lastFrameMs = 0.0;   // acquire, paint, submit and present
        double totalFrameMs = 0.0;
    };

    virtual juce::Image readback() = 0;
    virtual Stats getStats() const = 0;
    virtual long getNativeInputEventMask() const = 0;
};
} // namespace ilana::vulkan
