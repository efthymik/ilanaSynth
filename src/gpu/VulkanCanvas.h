#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "VulkanDevice.h"

namespace ilana::vulkan
{
struct FrameResources;

// A persistent BGRA8 picture on the GPU that JUCE Graphics paints into. Each
// paint() redraws only the dirty rectangles (cleared to transparent first)
// with JUCE's own rasteriser semantics: clip regions, paths, strokes and
// glyphs become edge tables on the CPU exactly as in the software renderer,
// and the GPU fills their spans with solid colours, gradients and images,
// and composites transparency layers. Message thread only.
class Canvas
{
public:
    explicit Canvas (std::shared_ptr<Device>);
    ~Canvas();

    Device& getDevice() const { return *device; }

    // Physical pixels. Returns false if the GPU picture couldn't be made.
    // After a resize the contents are undefined: repaint everything.
    bool resize (int width, int height);
    int getWidth() const  { return target.width; }
    int getHeight() const { return target.height; }

    // Hooks for presenting: the paint's submit waits on `wait` (at
    // waitStage), signals `signal`, and afterPaint records extra commands
    // after the picture is finished, when its image is in
    // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL (copy it to a swapchain image).
    struct SubmitHooks
    {
        VkSemaphore wait = VK_NULL_HANDLE;
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSemaphore signal = VK_NULL_HANDLE;
        std::function<void (VkCommandBuffer, VkImage canvasImage)> afterPaint;
    };

    // Paints `dirty` (physical pixels; clipped to the canvas): draw gets a
    // Graphics whose origin is the canvas's top-left, in physical pixels,
    // clipped to the dirty area, which was cleared to transparent black.
    // An empty `dirty` still submits (so hooks can present). Returns false
    // when recording or submitting failed (the device may be lost).
    bool paint (const juce::RectangleList<int>& dirty,
                const std::function<void (juce::Graphics&)>& draw,
                const SubmitHooks& hooks = {});

    // Waits for the GPU, then copies the picture back (ARGB, premultiplied).
    juce::Image readback();

    VkImage getImage() const { return target.image; }

    // Waits until the last paint finished on the GPU.
    void waitIdle();

private:
    std::shared_ptr<Device> device;
    Texture target;
    std::unique_ptr<FrameResources> frame;
};

// Renders a component offscreen through Vulkan, like
// Component::createComponentSnapshot (area, true, scale). Returns an invalid
// Image when Vulkan is unavailable. Used by the parity tests.
juce::Image renderComponent (juce::Component& component, juce::Rectangle<int> area, float scale);

} // namespace ilana::vulkan
