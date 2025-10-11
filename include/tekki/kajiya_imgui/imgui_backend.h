#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include "tekki/backend/device.h"

namespace tekki::kajiya_imgui {

// Forward declarations
class Image;
class Renderer;
class UiRenderer;

struct GfxResources {
    VkRenderPass ImguiRenderPass;
    VkFramebuffer ImguiFramebuffer;
    std::shared_ptr<Image> ImguiTexture;
};

class ImGuiBackendInner {
private:
    std::shared_ptr<Renderer> ImguiRenderer;
    std::optional<GfxResources> Gfx;

public:
    void CreateGraphicsResources(tekki::backend::Device* device, const glm::uvec2& surfaceResolution);
    std::shared_ptr<Image> GetTargetImage();
    std::shared_ptr<Image> Render(const glm::uvec2& physicalSize, const ImDrawData* drawData, std::shared_ptr<tekki::backend::Device> device, VkCommandBuffer commandBuffer);

    // Getter for Gfx to fix access issues
    const std::optional<GfxResources>& GetGfx() const { return Gfx; }
};

class ImGuiBackend {
private:
    std::shared_ptr<ImGuiBackendInner> Inner;
    std::shared_ptr<tekki::backend::Device> Device;

public:
    ImGuiBackend(std::shared_ptr<tekki::backend::Device> device, GLFWwindow* window, ImGuiContext* imguiContext);

    void CreateGraphicsResources(const glm::uvec2& surfaceResolution);
    void DestroyGraphicsResources();

    void HandleEvent(GLFWwindow* window, ImGuiContext* imguiContext, const void* event);
    ImGuiContext* PrepareFrame(GLFWwindow* window, ImGuiContext* imguiContext, float deltaTime);
    void FinishFrame(ImGuiContext* ui, GLFWwindow* window, UiRenderer* uiRenderer);

    static void SetupImguiStyle(ImGuiContext* context);
    static VkRenderPass CreateImguiRenderPass(VkDevice device);
    static std::pair<VkFramebuffer, std::shared_ptr<Image>> CreateImguiFramebuffer(tekki::backend::Device* device, VkRenderPass renderPass, const glm::uvec2& surfaceResolution);
};

}