#include "tekki/kajiya_imgui/imgui_backend.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <optional>

namespace tekki::kajiya_imgui {

void ImGuiBackendInner::CreateGraphicsResources(class Device* device, const glm::uvec2& surfaceResolution) {
    if (Gfx.has_value()) {
        throw std::runtime_error("Graphics resources already created");
    }

    auto imguiRenderPass = ImGuiBackend::CreateImguiRenderPass(device->GetVkDevice());
    auto [imguiFramebuffer, imguiTexture] = ImGuiBackend::CreateImguiFramebuffer(device, imguiRenderPass, surfaceResolution);

    Gfx = GfxResources{
        .ImguiRenderPass = imguiRenderPass,
        .ImguiFramebuffer = imguiFramebuffer,
        .ImguiTexture = imguiTexture
    };

    // Create pipeline for imgui renderer
    // Note: This assumes the imgui renderer has a CreatePipeline method
    // ImguiRenderer->CreatePipeline(device->GetVkDevice(), Gfx->ImguiRenderPass);
}

std::shared_ptr<class Image> ImGuiBackendInner::GetTargetImage() {
    if (Gfx.has_value()) {
        return Gfx->ImguiTexture;
    }
    return nullptr;
}

std::shared_ptr<class Image> ImGuiBackendInner::Render(const glm::uvec2& physicalSize, const ImDrawData* drawData, std::shared_ptr<class Device> device, VkCommandBuffer commandBuffer) {
    if (!Gfx.has_value()) {
        return nullptr;
    }

    auto vkDevice = device->GetVkDevice();

    // Begin imgui frame
    // Note: This assumes the imgui renderer has a BeginFrame method
    // ImguiRenderer->BeginFrame(vkDevice, commandBuffer);

    // Begin render pass
    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = Gfx->ImguiRenderPass;
    renderPassInfo.framebuffer = Gfx->ImguiFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {physicalSize.x, physicalSize.y};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Render imgui draw data
    // Note: This assumes the imgui renderer has a Render method
    // ImguiRenderer->Render(drawData, vkDevice, commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    return Gfx->ImguiTexture;
}

ImGuiBackend::ImGuiBackend(std::shared_ptr<class Device> device, GLFWwindow* window, ImGuiContext* imguiContext) 
    : Device(device), Inner(std::make_shared<ImGuiBackendInner>()) {
    
    SetupImguiStyle(imguiContext);

    // Initialize imgui platform
    ImguiPlatform = std::make_unique<WinitPlatform>();
    // Note: WinitPlatform attachment to GLFW window would need adaptation

    // Setup fonts
    float hidpiFactor = 1.0f; // Would need to get from GLFW
    float fontSize = 13.0f * hidpiFactor;
    
    ImFontConfig fontConfig{};
    fontConfig.SizePixels = fontSize;
    
    // Add default font
    ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
    
    // Add Japanese font
    // Note: Font loading would need to be adapted for C++
    // ImGui::GetIO().Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", fontSize, &fontConfig, ImGui::GetIO().Fonts->GetGlyphRangesJapanese());

    // Create imgui renderer
    // Note: This would need to be adapted from ash_imgui::Renderer
    // Inner->ImguiRenderer = std::make_shared<ImguiRenderer>(...);
}

void ImGuiBackend::CreateGraphicsResources(const glm::uvec2& surfaceResolution) {
    Inner->CreateGraphicsResources(Device.get(), surfaceResolution);
}

void ImGuiBackend::DestroyGraphicsResources() {
    auto vkDevice = Device->GetVkDevice();
    
    vkDeviceWaitIdle(vkDevice);

    // Destroy pipeline if exists
    // if (Inner->ImguiRenderer->HasPipeline()) {
    //     Inner->ImguiRenderer->DestroyPipeline(vkDevice);
    // }

    if (Inner->Gfx.has_value()) {
        vkDestroyFramebuffer(vkDevice, Inner->Gfx->ImguiFramebuffer, nullptr);
        vkDestroyRenderPass(vkDevice, Inner->Gfx->ImguiRenderPass, nullptr);
        Inner->Gfx.reset();
    }
}

void ImGuiBackend::HandleEvent(GLFWwindow* window, ImGuiContext* imguiContext, const void* event) {
    // Note: Event handling would need adaptation from winit to GLFW
    // ImguiPlatform->HandleEvent(ImGui::GetIO(), window, event);
}

ImGuiContext* ImGuiBackend::PrepareFrame(GLFWwindow* window, ImGuiContext* imguiContext, float deltaTime) {
    // Note: Frame preparation would need adaptation
    // ImguiPlatform->PrepareFrame(ImGui::GetIO(), window);
    
    ImGui::GetIO().DeltaTime = deltaTime;
    ImGui::NewFrame();
    return imguiContext;
}

void ImGuiBackend::FinishFrame(ImGuiContext* ui, GLFWwindow* window, class UiRenderer* uiRenderer) {
    ImGui::Render();
    
    auto drawData = ImGui::GetDrawData();
    auto targetImage = Inner->GetTargetImage();

    if (uiRenderer && targetImage) {
        // Note: This callback mechanism would need adaptation
        // uiRenderer->SetUIFrame([inner = Inner, device = Device, physicalSize = glm::uvec2{width, height}, drawData](VkCommandBuffer cb) {
        //     return inner->Render(physicalSize, drawData, device, cb);
        // }, targetImage);
    }
}

void ImGuiBackend::SetupImguiStyle(ImGuiContext* context) {
    auto hi = [](float v) -> ImVec4 { return {0.502f, 0.075f, 0.256f, v}; };
    auto med = [](float v) -> ImVec4 { return {0.455f, 0.198f, 0.301f, v}; };
    auto low = [](float v) -> ImVec4 { return {0.232f, 0.201f, 0.271f, v}; };
    auto bg = [](float v) -> ImVec4 { return {0.200f, 0.220f, 0.270f, v}; };
    auto text = [](float v) -> ImVec4 { return {0.860f, 0.930f, 0.890f, v}; };

    ImGuiStyle& style = ImGui::GetStyle();
    
    style.Colors[ImGuiCol_Text] = text(0.78f);
    style.Colors[ImGuiCol_TextDisabled] = text(0.28f);
    style.Colors[ImGuiCol_WindowBg] = {0.13f, 0.14f, 0.17f, 0.7f};
    style.Colors[ImGuiCol_ChildBg] = bg(0.58f);
    style.Colors[ImGuiCol_PopupBg] = bg(0.9f);
    style.Colors[ImGuiCol_Border] = {0.31f, 0.31f, 1.00f, 0.00f};
    style.Colors[ImGuiCol_BorderShadow] = {0.00f, 0.00f, 0.00f, 0.00f};
    style.Colors[ImGuiCol_FrameBg] = bg(1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = med(0.78f);
    style.Colors[ImGuiCol_FrameBgActive] = med(1.00f);
    style.Colors[ImGuiCol_TitleBg] = low(1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = hi(1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = bg(0.75f);
    style.Colors[ImGuiCol_MenuBarBg] = bg(0.47f);
    style.Colors[ImGuiCol_ScrollbarBg] = bg(1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = {0.09f, 0.15f, 0.16f, 1.00f};
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = med(0.78f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = med(1.00f);
    style.Colors[ImGuiCol_CheckMark] = {0.71f, 0.22f, 0.27f, 1.00f};
    style.Colors[ImGuiCol_SliderGrab] = {0.47f, 0.77f, 0.83f, 0.14f};
    style.Colors[ImGuiCol_SliderGrabActive] = {0.71f, 0.22f, 0.27f, 1.00f};
    style.Colors[ImGuiCol_Button] = {0.47f, 0.77f, 0.83f, 0.14f};
    style.Colors[ImGuiCol_ButtonHovered] = med(0.86f);
    style.Colors[ImGuiCol_ButtonActive] = med(1.00f);
    style.Colors[ImGuiCol_Header] = med(0.76f);
    style.Colors[ImGuiCol_HeaderHovered] = med(0.86f);
    style.Colors[ImGuiCol_HeaderActive] = hi(1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = {0.47f, 0.77f, 0.83f, 0.04f};
    style.Colors[ImGuiCol_ResizeGripHovered] = med(0.78f);
    style.Colors[ImGuiCol_ResizeGripActive] = med(1.00f);
    style.Colors[ImGuiCol_PlotLines] = text(0.63f);
    style.Colors[ImGuiCol_PlotLinesHovered] = med(1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = text(0.63f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = med(1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = med(0.43f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = bg(0.73f);

    style.WindowPadding = {6.0f, 4.0f};
    style.WindowRounding = 0.0f;
    style.FramePadding = {5.0f, 2.0f};
    style.FrameRounding = 3.0f;
    style.ItemSpacing = {7.0f, 1.0f};
    style.ItemInnerSpacing = {1.0f, 1.0f};
    style.TouchExtraPadding = {0.0f, 0.0f};
    style.IndentSpacing = 6.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 16.0f;
    style.GrabMinSize = 20.0f;
    style.GrabRounding = 2.0f;

    style.WindowTitleAlign = {0.50f, 0.50f};

    style.Colors[ImGuiCol_Border] = {0.539f, 0.479f, 0.255f, 0.162f};
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 1.0f;
}

VkRenderPass ImGuiBackend::CreateImguiRenderPass(VkDevice device) {
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachment{};
    colorAttachment.attachment = 0;
    colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachment;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create imgui render pass");
    }

    return renderPass;
}

std::pair<VkFramebuffer, std::shared_ptr<class Image>> ImGuiBackend::CreateImguiFramebuffer(class Device* device, VkRenderPass renderPass, const glm::uvec2& surfaceResolution) {
    // Create image
    // Note: This assumes Device has a CreateImage method
    // auto image = device->CreateImage(ImageDesc::new_2d(VK_FORMAT_R8G8B8A8_UNORM, surfaceResolution)
    //     .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

    // Create image view
    // Note: This assumes Image has a CreateView method
    // auto imageView = image->CreateView(device, ImageViewDesc::default());

    // Create framebuffer
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    // framebufferInfo.pAttachments = &imageView;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.width = surfaceResolution.x;
    framebufferInfo.height = surfaceResolution.y;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(device->GetVkDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create imgui framebuffer");
    }

    // return {framebuffer, image};
    return {framebuffer, nullptr}; // Placeholder - actual image creation would need Device implementation
}

} // namespace tekki::kajiya_imgui