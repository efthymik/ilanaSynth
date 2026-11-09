// The only file of the Vulkan UI that sees the platform's window headers.
// volk is compiled without the platform defines (so its own translation unit
// stays free of windows.h and Xlib.h), which means it never loads the
// platform surface entry point: it is fetched here, from the instance.

#if defined (_WIN32)
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#elif defined (__linux__)
 #include <dlfcn.h>
 #include <X11/Xlib.h>
#endif

#include "VulkanSurface.h"

#if defined (_WIN32)
 #include <vulkan/vulkan_win32.h>
#elif defined (__linux__)
 #include <vulkan/vulkan_xlib.h>
#endif

namespace ilana::vulkan
{
#if defined (_WIN32)

VkSurfaceKHR createPlatformSurface (VkInstance instance, void* nativeWindowHandle, std::string& error)
{
    auto create = (PFN_vkCreateWin32SurfaceKHR) vkGetInstanceProcAddr (instance, "vkCreateWin32SurfaceKHR");

    if (create == nullptr)
    {
        error = "the driver has no VK_KHR_win32_surface";
        return VK_NULL_HANDLE;
    }

    auto hwnd = (HWND) nativeWindowHandle;
    VkWin32SurfaceCreateInfoKHR info { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    // The module that owns the window class (the plugin DLL, not the host's
    // exe): the spec asks for the HINSTANCE the window belongs to.
    info.hinstance = (HINSTANCE) GetWindowLongPtrW (hwnd, GWLP_HINSTANCE);
    info.hwnd = hwnd;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const auto result = create (instance, &info, nullptr, &surface);

    if (result != VK_SUCCESS)
    {
        error = "vkCreateWin32SurfaceKHR failed (" + std::to_string ((int) result) + ")";
        return VK_NULL_HANDLE;
    }

    return surface;
}

void setNativeChildBounds (void* nativeWindowHandle, int x, int y, int width, int height)
{
    if (nativeWindowHandle != nullptr)
        SetWindowPos ((HWND) nativeWindowHandle, nullptr, x, y, width, height,
                      SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

#elif defined (__linux__)

namespace
{
// libX11 is opened at run time, as JUCE does, so the plugin doesn't gain a
// link-time dependency on it.
struct XlibConnection
{
    XlibConnection()
    {
        library = dlopen ("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);

        if (library == nullptr)
            library = dlopen ("libX11.so", RTLD_LAZY | RTLD_LOCAL);

        if (library == nullptr)
            return;

        using OpenDisplay = Display* (*) (const char*);

        if (auto open = (OpenDisplay) dlsym (library, "XOpenDisplay"))
            display = open (nullptr);
    }

    // Kept open for the life of the process: a swapchain's present thread may
    // still use the connection while the plugin is being unloaded, and an X
    // connection costs nothing while idle.
    void* library = nullptr;
    Display* display = nullptr;
};

Display* sharedDisplay()
{
    static XlibConnection connection;
    return connection.display;
}
} // namespace

VkSurfaceKHR createPlatformSurface (VkInstance instance, void* nativeWindowHandle, std::string& error)
{
    auto create = (PFN_vkCreateXlibSurfaceKHR) vkGetInstanceProcAddr (instance, "vkCreateXlibSurfaceKHR");

    if (create == nullptr)
    {
        error = "the driver has no VK_KHR_xlib_surface";
        return VK_NULL_HANDLE;
    }

    auto* display = sharedDisplay();

    if (display == nullptr)
    {
        error = "couldn't open an X display";
        return VK_NULL_HANDLE;
    }

    VkXlibSurfaceCreateInfoKHR info { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
    info.dpy = display;
    info.window = (Window) (uintptr_t) nativeWindowHandle;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const auto result = create (instance, &info, nullptr, &surface);

    if (result != VK_SUCCESS)
    {
        error = "vkCreateXlibSurfaceKHR failed (" + std::to_string ((int) result) + ")";
        return VK_NULL_HANDLE;
    }

    return surface;
}

void setNativeChildBounds (void*, int, int, int, int) {}

#else

VkSurfaceKHR createPlatformSurface (VkInstance, void*, std::string& error)
{
    error = "no Vulkan surfaces on this platform";
    return VK_NULL_HANDLE;
}

void setNativeChildBounds (void*, int, int, int, int) {}

#endif
} // namespace ilana::vulkan
