// Puts a Vulkan Canvas on screen behind a component, the way
// juce::OpenGLContext attaches (juce_OpenGLContext.cpp and the native
// juce_OpenGL_windows.h / juce_OpenGL_linux.h): a CachedComponentImage turns
// the component's repaints into dirty rectangles, a child native window that
// ignores the mouse covers the component, and a swapchain on that window
// shows the canvas. Everything runs on the message thread, at most once per
// vblank and only when something is dirty, so an idle editor costs nothing.

// JUCE's X11 declarations (Linux) and its thread DPI helper (Windows) are
// only visible when these are set before its headers, as juce_opengl does.
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#define JUCE_GUI_BASICS_INCLUDE_SCOPED_THREAD_DPI_AWARENESS_SETTER 1

#include "VulkanAttachment.h"
#include "VulkanCanvas.h"
#include "VulkanSurface.h"

#if JUCE_LINUX || JUCE_BSD
 #include <juce_gui_basics/native/juce_ScopedWindowAssociation_linux.h>
#endif

#include <optional>

#if JUCE_WINDOWS
namespace juce
{
// Defined in juce_Windowing_windows.cpp (juce_opengl declares it the same way):
// a child HWND whose WM_NCHITTEST answers HTTRANSPARENT, so clicks fall
// through to the editor's own window, and which never paints itself.
extern ComponentPeer* createNonRepaintingEmbeddedWindowsPeer (Component&, Component* parent);
} // namespace juce
#endif

namespace ilana::vulkan
{
namespace
{
using namespace juce;

//==============================================================================
// The child window the swapchain presents into, covering the component.
class NativeChild
{
public:
   #if JUCE_WINDOWS
    // The placeholder is the child peer's component: the non-repainting peer
    // calls its handleCommandMessage on WM_PAINT (the window was uncovered
    // or restored), which asks for the last frame to be shown again.
    struct Placeholder : Component
    {
        explicit Placeholder (std::function<void()> onPaint) : paintRequested (std::move (onPaint)) { setOpaque (true); }
        void handleCommandMessage (int) override { if (paintRequested) paintRequested(); }
        std::function<void()> paintRequested;
    };

    NativeChild (Component& component, std::function<void()> onExposed)
        : placeholder (std::move (onExposed))
    {
        auto* top = component.getTopLevelComponent();
        const ScopedThreadDPIAwarenessSetter dpi { top->getWindowHandle() };
        peer.reset (createNonRepaintingEmbeddedWindowsPeer (placeholder, top));
    }

    ~NativeChild()
    {
        peer.reset();
    }

    void* getHandle() const { return peer != nullptr ? peer->getNativeHandle() : nullptr; }

    void setBounds (Rectangle<int> physical)
    {
        if (peer == nullptr)
            return;

        const ScopedThreadDPIAwarenessSetter dpi { peer->getNativeHandle() };
        setNativeChildBounds (peer->getNativeHandle(), physical.getX(), physical.getY(),
                              jmax (1, physical.getWidth()), jmax (1, physical.getHeight()));
    }

    // The window stays hidden until it holds a picture, so the editor never
    // flashes an undefined swapchain image.
    void show()
    {
        if (peer != nullptr && ! shown)
        {
            peer->setVisible (true);
            shown = true;
        }
    }

    long getInputEventMask() const { return -1; }

private:
    Placeholder placeholder;
    std::unique_ptr<ComponentPeer> peer;
    bool shown = false;

   #elif JUCE_LINUX || JUCE_BSD
    // A plain X child of the peer's window. It selects no input events, so
    // the server delivers pointer and key events to the editor's window as
    // if the child were not there. It does ask for Expose: JUCE routes that
    // to the editor's peer (the association below), whose repaint reaches
    // CachedImage::paint and shows the last frame again.
    NativeChild (Component& component, std::function<void()>)
    {
        auto* componentPeer = component.getPeer();
        jassert (componentPeer != nullptr);

        display = XWindowSystem::getInstance()->getDisplay();

        if (display == nullptr || componentPeer == nullptr)
            return;

        const XWindowSystemUtilities::ScopedXLock xLock;
        auto* x = X11Symbols::getInstance();
        const auto parent = (::Window) componentPeer->getNativeHandle();

        XSetWindowAttributes attributes {};
        attributes.border_pixel = 0;
        attributes.background_pixmap = None; // (the server doesn't clear it to black on a resize: no flash)
        attributes.event_mask = eventMask;

        window = x->xCreateWindow (display, parent, 0, 0, 1, 1, 0,
                                   CopyFromParent, InputOutput, (Visual*) CopyFromParent,
                                   CWBorderPixel | CWBackPixmap | CWEventMask, &attributes);

        if (window == 0)
            return;

        association.emplace (componentPeer, window);
        x->xMapWindow (display, window);
        // The surface uses its own X connection: the server must know the
        // window before that connection names it.
        x->xSync (display, False);
    }

    ~NativeChild()
    {
        if (window == 0)
            return;

        association.reset();

        const XWindowSystemUtilities::ScopedXLock xLock;
        auto* x = X11Symbols::getInstance();
        x->xUnmapWindow (display, window);
        x->xDestroyWindow (display, window);
        x->xSync (display, False);

        // Drop events still queued for it, so none reaches a peer later.
        XEvent event;
        while (x->xCheckWindowEvent (display, window, eventMask, &event) == True) {}
    }

    void* getHandle() const { return window != 0 ? (void*) (uintptr_t) window : nullptr; }

    void setBounds (Rectangle<int> physical)
    {
        if (window == 0)
            return;

        const XWindowSystemUtilities::ScopedXLock xLock;
        auto* x = X11Symbols::getInstance();
        x->xMoveResizeWindow (display, window, physical.getX(), physical.getY(),
                              (unsigned int) jmax (1, physical.getWidth()),
                              (unsigned int) jmax (1, physical.getHeight()));
        // (So the swapchain, on the other connection, sees the new size.)
        x->xSync (display, False);
    }

    void show() {}

    long getInputEventMask() const
    {
        if (window == 0)
            return -1;

        const XWindowSystemUtilities::ScopedXLock xLock;
        XWindowAttributes attributes {};

        if (X11Symbols::getInstance()->xGetWindowAttributes (display, window, &attributes) == 0)
            return -1;

        constexpr long input = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
                             | EnterWindowMask | LeaveWindowMask | PointerMotionMask | PointerMotionHintMask
                             | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask
                             | Button5MotionMask | ButtonMotionMask | KeymapStateMask | FocusChangeMask;
        return attributes.all_event_masks & input;
    }

private:
    static constexpr long eventMask = ExposureMask;
    ::Display* display = nullptr;
    ::Window window = 0;
    std::optional<ScopedWindowAssociation> association;

   #else
    NativeChild (Component&, std::function<void()>) {}
    void* getHandle() const { return nullptr; }
    void setBounds (Rectangle<int>) {}
    void show() {}
    long getInputEventMask() const { return -1; }
   #endif

    JUCE_DECLARE_NON_COPYABLE (NativeChild)
};

const char* presentModeName (VkPresentModeKHR mode)
{
    switch (mode)
    {
        case VK_PRESENT_MODE_MAILBOX_KHR:      return "mailbox";
        case VK_PRESENT_MODE_FIFO_KHR:         return "fifo";
        case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "immediate";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "fifo relaxed";
        default:                               return "other";
    }
}

//==============================================================================
class AttachmentImpl;

// Owned by the component (setCachedComponentImage); forwards to the attachment.
struct CachedImage final : CachedComponentImage
{
    explicit CachedImage (AttachmentImpl& o) : owner (&o) {}
    ~CachedImage() override;

    void paint (Graphics&) override;
    bool invalidateAll() override;
    bool invalidate (const Rectangle<int>&) override;
    void releaseResources() override;

    AttachmentImpl* owner;
};

//==============================================================================
class AttachmentImpl final : public Attachment,
                             private ComponentMovementWatcher,
                             private Timer,
                             private AsyncUpdater
{
public:
    AttachmentImpl (Component& c, std::shared_ptr<Device> d, std::function<void()> failure)
        : ComponentMovementWatcher (&c), component (c), device (std::move (d)), onFailure (std::move (failure))
    {
    }

    ~AttachmentImpl() override
    {
        cancelPendingUpdate();
        detach (false);
    }

    // Attaches now if the component is on screen (or later, when it gets
    // there). False only when attaching was tried and failed.
    bool start()
    {
        if (canBeAttached() && ! attach())
        {
            failed = true;
            return false;
        }

        return true;
    }

    //==============================================================================
    bool isActive() const override { return cachedImage != nullptr && window != nullptr && ! failed; }

    String describe() const override
    {
        String s = device != nullptr ? device->name : String ("no device yet");

        if (swapchain != VK_NULL_HANDLE)
            s << ", " << (int) extent.width << "x" << (int) extent.height << " "
              << (format == VK_FORMAT_B8G8R8A8_UNORM ? "BGRA8" : "RGBA8") << " " << presentModeName (presentMode);
        else if (failed)
            s << ", failed: " << failure;
        else
            s << ", not on screen";

        return s;
    }

    Image readback() override { return canvas != nullptr ? canvas->readback() : Image(); }
    Stats getStats() const override { return stats; }
    long getNativeInputEventMask() const override { return window != nullptr ? window->getInputEventMask() : -1; }

    //==============================================================================
    // CachedImage calls (component coordinates).
    void invalidated (Rectangle<int> area)
    {
        if (canvas == nullptr)
            return;

        const auto physical = area.toFloat().transformedBy (transform).getSmallestIntegerContainer()
                                  .getIntersection (canvasBounds());

        if (! physical.isEmpty())
        {
            dirty.add (physical);
            requestFrame();
        }
    }

    void invalidatedAll()
    {
        if (canvas == nullptr)
            return;

        dirty = canvasBounds();
        requestFrame();
    }

    // The peer painted over the component's area (an expose, a restore):
    // the canvas still holds the picture, so it is only presented again.
    void exposed()
    {
        if (swapchain != VK_NULL_HANDLE && ! presentWanted)
        {
            presentWanted = true;
            requestFrame();
        }
    }

    // The component is leaving its peer (hidden, or its window is going):
    // the native window and everything on it must go now, before the peer.
    void releaseNative()
    {
        destroyWindowResources();
    }

    CachedImage* cachedImage = nullptr;

private:
    //==============================================================================
    bool canBeAttached() const
    {
        return ! failed && component.getWidth() > 0 && component.getHeight() > 0 && isShowingOrMinimised (component);
    }

    static bool isShowingOrMinimised (const Component& c)
    {
        if (! c.isVisible())
            return false;

        if (auto* p = c.getParentComponent())
            return isShowingOrMinimised (*p);

        return c.getPeer() != nullptr;
    }

    bool isAttached() const { return cachedImage != nullptr; }

    bool attach()
    {
        jassert (! isAttached());

        // The device is made on the first attach, so an editor that never
        // goes on screen (ilanaSnapshot's pictures, tests) never loads Vulkan.
        if (device == nullptr)
        {
            device = sharedDevice();

            if (device == nullptr)
            {
                failure = lastError();
                return false;
            }
        }

        if (! device->hasSurfaceExtensions)
        {
            failure = "the driver can't present to windows";
            return false;
        }

        if (canvas == nullptr)
            canvas = std::make_unique<Canvas> (device);

        if (! createWindowResources())
            return false;

        auto image = std::make_unique<CachedImage> (*this);
        cachedImage = image.get();
        component.setCachedComponentImage (image.release());

        vblank = VBlankAttachment (&component, [this] { vblankArrived(); });
        startTimer (400);
        invalidatedAll();
        return true;
    }

    // Stops drawing through Vulkan: the component paints through its peer again.
    void detach (bool repaintThroughPeer)
    {
        stopTimer();

        // (Never from inside the vblank callback: that would delete the
        // function that is running. It stays and does nothing until then.)
        if (! inVBlank)
            vblank = {};

        destroyWindowResources();

        if (cachedImage != nullptr)
        {
            cachedImage->owner = nullptr;
            cachedImage = nullptr;
            component.setCachedComponentImage (nullptr);
        }

        if (canvas != nullptr)
        {
            canvas->waitIdle();
            canvas.reset();
        }

        dirty.clear();

        if (repaintThroughPeer)
            component.repaint();
    }

    void fail (const String& why)
    {
        if (failed)
            return;

        failed = true;
        failure = why;
        DBG ("Vulkan UI detached: " << why);
        detach (true);

        // Asynchronous, because the caller usually deletes this attachment
        // in its handler; the token stops a call after that.
        if (onFailure != nullptr)
            MessageManager::callAsync ([weak = std::weak_ptr<int> (aliveToken), callback = onFailure]
                                       {
                                           if (! weak.expired())
                                               callback();
                                       });
    }

    //==============================================================================
    // ComponentMovementWatcher, as OpenGLContext::Attachment.
    void componentMovedOrResized (bool, bool) override
    {
        if (isAttached() != canBeAttached())
            componentVisibilityChanged();

        if (isAttached() && ! updateGeometry())
            fail (failure);
    }

    using ComponentMovementWatcher::componentMovedOrResized;

    void componentPeerChanged() override
    {
        detach (false);
        componentVisibilityChanged();
    }

    void componentVisibilityChanged() override
    {
        if (canBeAttached())
        {
            if (! isAttached())
            {
                if (! attach())
                    fail (failure.isNotEmpty() ? failure : String ("couldn't attach"));
            }
            else if (window == nullptr)
            {
                // Back after releaseResources (hidden, then shown on the same peer).
                if (! createWindowResources())
                    fail (failure);
                else
                    invalidatedAll();
            }
            else
            {
                exposed(); // (needed when windows are un-minimised)
            }
        }
        else if (isAttached())
        {
            detach (false);
        }
    }

    using ComponentMovementWatcher::componentVisibilityChanged;

    void componentBeingDeleted (Component& c) override
    {
        // The owner must delete the attachment first; if it didn't, drop the
        // GPU side while the component is still whole.
        jassertfalse;
        detach (false);
        ComponentMovementWatcher::componentBeingDeleted (c);
    }

    // The top-level window moved to another display or changed its scale
    // without resizing the component (as OpenGLContext's 400 ms check).
    void timerCallback() override
    {
        if (isAttached() && ! updateGeometry())
            fail (failure);
    }

    //==============================================================================
    // Physical pixels of the component inside its peer's window.
    std::optional<Rectangle<int>> physicalArea() const
    {
        auto* peer = component.getPeer();

        if (peer == nullptr)
            return {};

        auto& peerComponent = peer->getComponent();
        const auto inPeer = peerComponent.getLocalArea (&component, component.getLocalBounds().toFloat());
        const auto scale = (float) peer->getPlatformScaleFactor() * peerComponent.getDesktopScaleFactor();
        return (inPeer * scale).toNearestIntEdges();
    }

    // False (with the reason in `failure`) when the canvas can't take the new size.
    bool updateGeometry()
    {
        const auto area = physicalArea();

        if (! area.has_value() || area->isEmpty() || component.getWidth() <= 0 || component.getHeight() <= 0)
            return true;

        if (*area != windowArea && window != nullptr)
        {
            window->setBounds (*area);
            windowArea = *area;
        }

        const auto newTransform = AffineTransform::scale ((float) area->getWidth() / (float) component.getWidth(),
                                                          (float) area->getHeight() / (float) component.getHeight());

        if (canvas != nullptr && (area->getWidth() != canvas->getWidth() || area->getHeight() != canvas->getHeight()
                                  || newTransform != transform))
        {
            transform = newTransform;

            if (! canvas->resize (area->getWidth(), area->getHeight()))
            {
                failure = "couldn't make a " + String (area->getWidth()) + "x" + String (area->getHeight()) + " canvas";
                return false;
            }

            swapchainStale = true;
            invalidatedAll();
        }

        return true;
    }

    Rectangle<int> canvasBounds() const
    {
        return canvas != nullptr ? Rectangle<int> (canvas->getWidth(), canvas->getHeight()) : Rectangle<int>();
    }

    //==============================================================================
    bool createWindowResources()
    {
        window = std::make_unique<NativeChild> (component, [this] { exposed(); });

        if (window->getHandle() == nullptr)
        {
            failure = "couldn't create the child window";
            window.reset();
            return false;
        }

        windowArea = {};
        transform = {};

        if (! updateGeometry())
        {
            destroyWindowResources();
            return false;
        }

        std::string error;
        surface = createPlatformSurface (device->instance, window->getHandle(), error);

        if (surface == VK_NULL_HANDLE)
        {
            failure = error;
            destroyWindowResources();
            return false;
        }

        VkBool32 supported = VK_FALSE;

        if (vkGetPhysicalDeviceSurfaceSupportKHR (device->physical, device->queueFamily, surface, &supported) != VK_SUCCESS
            || supported != VK_TRUE)
        {
            failure = "the device's queue can't present to this window";
            destroyWindowResources();
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        if (vkCreateSemaphore (device->device, &semaphoreInfo, nullptr, &acquired) != VK_SUCCESS)
        {
            failure = "vkCreateSemaphore failed";
            destroyWindowResources();
            return false;
        }

        if (! createSwapchain())
        {
            destroyWindowResources();
            return false;
        }

        return true;
    }

    void destroyWindowResources()
    {
        if (device == nullptr || (surface == VK_NULL_HANDLE && window == nullptr && acquired == VK_NULL_HANDLE))
            return;

        // Nothing may still wait on a semaphore or read a swapchain image.
        if (canvas != nullptr)
            canvas->waitIdle();

        vkQueueWaitIdle (device->queue);
        destroySwapchain (swapchain);
        swapchain = VK_NULL_HANDLE;

        if (acquired != VK_NULL_HANDLE)
            vkDestroySemaphore (device->device, acquired, nullptr);

        acquired = VK_NULL_HANDLE;

        if (surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR (device->instance, surface, nullptr);

        surface = VK_NULL_HANDLE;
        window.reset();
        windowArea = {};
        presentWanted = false;
    }

    void destroySwapchain (VkSwapchainKHR old)
    {
        for (auto s : presented)
            vkDestroySemaphore (device->device, s, nullptr);

        presented.clear();
        images.clear();

        if (old != VK_NULL_HANDLE)
            vkDestroySwapchainKHR (device->device, old, nullptr);
    }

    bool createSwapchain()
    {
        swapchainStale = false;

        if (canvas == nullptr || canvas->getWidth() <= 0 || canvas->getHeight() <= 0)
            return true; // (made once the component has a size)

        VkSurfaceCapabilitiesKHR caps {};

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR (device->physical, surface, &caps) != VK_SUCCESS)
        {
            failure = "the window's surface was lost";
            return false;
        }

        VkExtent2D wanted { (uint32_t) canvas->getWidth(), (uint32_t) canvas->getHeight() };

        if (caps.currentExtent.width != UINT32_MAX)
            wanted = caps.currentExtent;

        wanted.width = jlimit (caps.minImageExtent.width, jmax (caps.minImageExtent.width, caps.maxImageExtent.width), wanted.width);
        wanted.height = jlimit (caps.minImageExtent.height, jmax (caps.minImageExtent.height, caps.maxImageExtent.height), wanted.height);

        // A minimised window has no size: no swapchain until it comes back.
        if (wanted.width == 0 || wanted.height == 0)
        {
            vkQueueWaitIdle (device->queue);
            destroySwapchain (swapchain);
            swapchain = VK_NULL_HANDLE;
            return true;
        }

        if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        {
            failure = "the swapchain can't be copied into";
            return false;
        }

        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR (device->physical, surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats (count);
        vkGetPhysicalDeviceSurfaceFormatsKHR (device->physical, surface, &count, formats.data());

        // UNORM only: JUCE blends in gamma space, and an SRGB swapchain would
        // convert the picture a second time on the way out.
        std::optional<VkSurfaceFormatKHR> chosen;

        for (auto wantedFormat : { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM })
        {
            for (const auto& f : formats)
            {
                if (f.format == wantedFormat && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    chosen = f;
                    break;
                }
            }

            if (chosen.has_value())
                break;
        }

        if (! chosen.has_value())
        {
            failure = "the window offers no 8-bit UNORM format";
            return false;
        }

        // The canvas is BGRA: the same format is a plain copy; RGBA needs a
        // blit (which swaps the channels), so its format must allow one.
        if (chosen->format != VK_FORMAT_B8G8R8A8_UNORM)
        {
            VkFormatProperties properties {};
            vkGetPhysicalDeviceFormatProperties (device->physical, chosen->format, &properties);

            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0)
            {
                failure = "the window's RGBA format can't be blitted into";
                return false;
            }
        }

        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR (device->physical, surface, &count, nullptr);
        std::vector<VkPresentModeKHR> modes (count);
        vkGetPhysicalDeviceSurfacePresentModesKHR (device->physical, surface, &count, modes.data());

        // Mailbox never blocks the message thread; FIFO is always there.
        const auto mode = std::find (modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()
                              ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;

        auto imageCount = caps.minImageCount + 1;

        if (caps.maxImageCount != 0)
            imageCount = jmin (imageCount, caps.maxImageCount);

        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        for (auto a : { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR })
        {
            if ((caps.supportedCompositeAlpha & a) != 0)
            {
                alpha = a;
                break;
            }
        }

        VkSwapchainCreateInfoKHR info { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        info.surface = surface;
        info.minImageCount = imageCount;
        info.imageFormat = chosen->format;
        info.imageColorSpace = chosen->colorSpace;
        info.imageExtent = wanted;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0
                                ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
        info.compositeAlpha = alpha;
        info.presentMode = mode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapchain;

        VkSwapchainKHR created = VK_NULL_HANDLE;
        const auto result = vkCreateSwapchainKHR (device->device, &info, nullptr, &created);

        // The old one is retired either way; nothing of it may be in flight.
        vkQueueWaitIdle (device->queue);
        destroySwapchain (swapchain);
        swapchain = VK_NULL_HANDLE;

        if (result != VK_SUCCESS)
        {
            failure = "vkCreateSwapchainKHR failed (" + String ((int) result) + ")";
            return false;
        }

        swapchain = created;
        format = chosen->format;
        extent = wanted;
        presentMode = mode;

        count = 0;
        vkGetSwapchainImagesKHR (device->device, swapchain, &count, nullptr);
        images.resize (count);
        vkGetSwapchainImagesKHR (device->device, swapchain, &count, images.data());

        // One "rendered" semaphore per image: the presentation engine may
        // still be waiting on an image's semaphore until that image comes
        // back from acquire.
        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        presented.resize (count, VK_NULL_HANDLE);

        for (auto& s : presented)
        {
            if (vkCreateSemaphore (device->device, &semaphoreInfo, nullptr, &s) != VK_SUCCESS)
            {
                failure = "vkCreateSemaphore failed";
                return false;
            }
        }

        // Swapchain images start undefined: the whole canvas goes to the
        // next one (its own contents are kept, so nothing is redrawn).
        presentWanted = true;
        return true;
    }

    //==============================================================================
    // Scheduling: a frame starts at most once per vblank. Repaints made before
    // the next vblank are gathered into one frame; one made after a vblank
    // that drew nothing starts its frame straight away (an AsyncUpdater, so
    // every repaint from the same message is in it).
    bool wantsFrame() const { return ! dirty.isEmpty() || presentWanted || swapchainStale; }

    void requestFrame()
    {
        if (frameAllowed)
            triggerAsyncUpdate();
    }

    void vblankArrived()
    {
        if (! isAttached())
            return;

        frameAllowed = true;

        if (wantsFrame())
        {
            const ScopedValueSetter<bool> inside (inVBlank, true);
            renderFrame();
        }
    }

    void handleAsyncUpdate() override
    {
        if (isAttached() && frameAllowed && wantsFrame())
            renderFrame();
    }

    void renderFrame()
    {
        if (window == nullptr || surface == VK_NULL_HANDLE || canvas == nullptr || failed)
            return;

        const auto start = juce::Time::getMillisecondCounterHiRes();

        if (swapchainStale || swapchain == VK_NULL_HANDLE)
        {
            if (! createSwapchain())
            {
                fail (failure);
                return;
            }

            if (swapchain == VK_NULL_HANDLE)
                return; // (minimised)
        }

        frameAllowed = false;

        // The last frame's submit waited on `acquired`: it must be done
        // before the semaphore is used again. At one frame per vblank this
        // is long finished.
        canvas->waitIdle();

        uint32_t index = 0;
        // A short wait: the message thread must not stall on a busy GPU or
        // a FIFO queue that is full. The region stays dirty for next vblank.
        auto result = vkAcquireNextImageKHR (device->device, swapchain, 2'000'000, acquired, VK_NULL_HANDLE, &index);

        if (result == VK_TIMEOUT || result == VK_NOT_READY)
        {
            ++stats.acquireTimeouts;
            return;
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            swapchainStale = true;
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            fail ("vkAcquireNextImageKHR failed (" + String ((int) result) + ")");
            return;
        }

        if (result == VK_SUBOPTIMAL_KHR)
            swapchainStale = true;

        const auto region = std::exchange (dirty, {});
        presentWanted = false;

        Canvas::SubmitHooks hooks;
        hooks.wait = acquired;
        hooks.waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        hooks.signal = presented[index];
        hooks.afterPaint = [this, target = images[index]] (VkCommandBuffer cmd, VkImage canvasImage)
        {
            recordCopy (cmd, canvasImage, target);
        };

        const auto drawn = canvas->paint (region, [this] (Graphics& g)
        {
            g.addTransform (transform);
            component.paintEntireComponent (g, false);
        }, hooks);

        if (! drawn)
        {
            fail ("the canvas couldn't be drawn (device lost?)");
            return;
        }

        VkPresentInfoKHR present { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &presented[index];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &index;
        result = vkQueuePresentKHR (device->queue, &present);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // The window changed under the frame: a new swapchain, and the
            // canvas shown again on it.
            swapchainStale = true;
            presentWanted = true;
        }
        else if (result != VK_SUCCESS)
        {
            fail ("vkQueuePresentKHR failed (" + String ((int) result) + ")");
            return;
        }

        window->show();

        stats.lastFrameMs = juce::Time::getMillisecondCounterHiRes() - start;
        stats.totalFrameMs += stats.lastFrameMs;
        ++stats.framesPresented;
    }

    // The whole canvas into the swapchain image (its contents are not kept
    // between presents), then the image handed to the presentation engine.
    void recordCopy (VkCommandBuffer cmd, VkImage canvasImage, VkImage target)
    {
        VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = target;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        // The acquire semaphore is waited on at TRANSFER, so a TRANSFER source
        // stage chains this layout change after the image is really free.
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier (cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &barrier);

        const auto w = (uint32_t) jmin ((int) extent.width, canvas->getWidth());
        const auto h = (uint32_t) jmin ((int) extent.height, canvas->getHeight());

        // A swapchain bigger than the canvas (the window grew this frame)
        // gets black around the picture rather than an undefined image.
        if (w < extent.width || h < extent.height)
        {
            const VkClearColorValue black { { 0.0f, 0.0f, 0.0f, 1.0f } };
            vkCmdClearColorImage (cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &barrier.subresourceRange);

            VkMemoryBarrier clearDone { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            clearDone.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            clearDone.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier (cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0, 1, &clearDone, 0, nullptr, 0, nullptr);
        }

        const VkImageSubresourceLayers layers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

        if (format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            VkImageCopy copy {};
            copy.srcSubresource = layers;
            copy.dstSubresource = layers;
            copy.extent = { w, h, 1 };
            vkCmdCopyImage (cmd, canvasImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        }
        else
        {
            VkImageBlit blit {};
            blit.srcSubresource = layers;
            blit.dstSubresource = layers;
            blit.srcOffsets[1] = { (int32_t) w, (int32_t) h, 1 };
            blit.dstOffsets[1] = { (int32_t) w, (int32_t) h, 1 };
            vkCmdBlitImage (cmd, canvasImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        }

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier (cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    //==============================================================================
    Component& component;
    std::shared_ptr<Device> device;
    std::function<void()> onFailure;
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);

    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<NativeChild> window;
    Rectangle<int> windowArea;
    AffineTransform transform;
    RectangleList<int> dirty; // physical pixels on the canvas

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent {};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    std::vector<VkImage> images;
    std::vector<VkSemaphore> presented;
    VkSemaphore acquired = VK_NULL_HANDLE;

    VBlankAttachment vblank;
    bool frameAllowed = true;
    bool inVBlank = false;
    bool presentWanted = false;
    bool swapchainStale = false;
    bool failed = false;
    String failure;
    Stats stats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AttachmentImpl)
};

//==============================================================================
CachedImage::~CachedImage()
{
    if (owner != nullptr)
        owner->cachedImage = nullptr;
}

void CachedImage::paint (Graphics&)
{
    if (owner != nullptr)
        owner->exposed();
}

bool CachedImage::invalidateAll()
{
    if (owner != nullptr)
        owner->invalidatedAll();

    return false; // (the peer doesn't repaint: the canvas does)
}

bool CachedImage::invalidate (const Rectangle<int>& area)
{
    if (owner != nullptr)
        owner->invalidated (area);

    return false;
}

void CachedImage::releaseResources()
{
    if (owner != nullptr)
        owner->releaseNative();
}
} // namespace

//==============================================================================
std::unique_ptr<Attachment> Attachment::attachTo (juce::Component& component, std::function<void()> onFailure)
{
    JUCE_ASSERT_MESSAGE_THREAD

    // A component already on screen attaches now, and a failure is the
    // caller's at once; one that isn't attaches when it gets there, and a
    // failure then comes through onFailure.
    auto attachment = std::make_unique<AttachmentImpl> (component, nullptr, std::move (onFailure));

    if (! attachment->start())
    {
        DBG ("Vulkan UI unavailable: " << attachment->describe());
        return {};
    }

    return attachment;
}
} // namespace ilana::vulkan
