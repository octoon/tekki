#pragma once

#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <imgui.h>

namespace tekki::ash_imgui {

class Renderer {
private:
    static constexpr size_t QuadCountPerFrame = 64 * 1024;
    static constexpr size_t VertexCountPerFrame = 4 * QuadCountPerFrame;
    static constexpr size_t IndexCountPerFrame = 6 * QuadCountPerFrame;
    static constexpr size_t PushConstantSize = 8;
    static constexpr size_t FrameCount = 2;

    VkPipelineLayout PipelineLayout;
    VkShaderModule VertexShader;
    VkShaderModule FragmentShader;
    VkPipeline Pipeline;
    std::array<VkBuffer, FrameCount> VertexBuffers;
    std::array<size_t, FrameCount> VertexMemOffsets;
    std::array<VkBuffer, FrameCount> IndexBuffers;
    std::array<size_t, FrameCount> IndexMemOffsets;
    VkBuffer ImageBuffer;
    VkDeviceMemory HostMem;
    void* HostMapping;
    uint32_t ImageWidth;
    uint32_t ImageHeight;
    VkImage Image;
    VkDeviceMemory LocalMem;
    VkDescriptorSet DescriptorSet;
    uint32_t AtomSize;
    size_t FrameIndex;
    bool ImageNeedsCopy;

    static VkShaderModule LoadShaderModule(VkDevice device, const std::vector<uint8_t>& bytes);
    static uint32_t GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags);
    static uint32_t AlignUp(uint32_t x, uint32_t alignment);

public:
    Renderer(VkDevice device, const VkPhysicalDeviceProperties& physicalDeviceProperties, const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, ImGuiContext* imgui);
    ~Renderer();

    void BeginFrame(VkDevice device, VkCommandBuffer commandBuffer);
    bool HasPipeline() const;
    VkPipeline CreatePipeline(VkDevice device, VkRenderPass renderPass);
    void DestroyPipeline(VkDevice device);
    void Render(const ImDrawData* drawData, VkDevice device, VkCommandBuffer commandBuffer);
};

}