#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

namespace tekki::kajiya_imgui {

struct GfxResources {
    VkRenderPass ImguiRenderPass;
    VkFramebuffer ImguiFramebuffer;
    std::shared_ptr<class Image> ImguiTexture;
};

class ImGuiBackendInner {
private:
    std::shared_ptr<class Renderer> ImguiRenderer;
    std::optional<GfxResources> Gfx;

public:
    void CreateGraphicsResources(class Device* device, const glm::uvec2& surfaceResolution);
    std::shared_ptr<class Image> GetTargetImage();
    std::shared_ptr<class Image> Render(const glm::uvec2& physicalSize, const ImDrawData* drawData, std::shared_ptr<class Device> device, VkCommandBuffer commandBuffer);
};

class ImGuiBackend {
private:
    std::shared_ptr<ImGuiBackendInner> Inner;
    std::shared_ptr<class Device> Device;
    std::unique_ptr<class WinitPlatform> ImguiPlatform;

public:
    ImGuiBackend(std::shared_ptr<class Device> device, GLFWwindow* window, ImGuiContext* imguiContext);
    
    void CreateGraphicsResources(const glm::uvec2& surfaceResolution);
    void DestroyGraphicsResources();
    
    void HandleEvent(GLFWwindow* window, ImGuiContext* imguiContext, const void* event);
    ImGuiContext* PrepareFrame(GLFWwindow* window, ImGuiContext* imguiContext, float deltaTime);
    void FinishFrame(ImGuiContext* ui, GLFWwindow* window, class UiRenderer* uiRenderer);
    
    static void SetupImguiStyle(ImGuiContext* context);
    static VkRenderPass CreateImguiRenderPass(VkDevice device);
    static std::pair<VkFramebuffer, std::shared_ptr<class Image>> CreateImguiFramebuffer(class Device* device, VkRenderPass renderPass, const glm::uvec2& surfaceResolution);
};

}