#pragma once

// The platform half of the Vulkan presenting code: making a VkSurfaceKHR for
// a native window. It lives in its own file so that windows.h and Xlib.h
// stay out of every file that includes JUCE (their macros clash with JUCE's
// names), and so this header includes neither.

#include <string>
#include <volk.h>

namespace ilana::vulkan
{
// A surface for a native window: an HWND on Windows, an X11 Window (cast to
// a pointer) on Linux. Returns VK_NULL_HANDLE and says why in `error` when
// the platform's surface extension is missing or the call failed. On Linux
// the surface talks to the X server through a Display connection of its own,
// opened once and kept for the process, so the presenting engine never
// shares JUCE's connection (and its locking) with the message thread.
VkSurfaceKHR createPlatformSurface (VkInstance, void* nativeWindowHandle, std::string& error);

// Places a child window in its parent, in physical pixels (Windows: an HWND,
// moved without activating it or changing its z-order). Does nothing on
// other platforms, where the caller moves its window through JUCE's own
// X connection.
void setNativeChildBounds (void* nativeWindowHandle, int x, int y, int width, int height);
} // namespace ilana::vulkan
