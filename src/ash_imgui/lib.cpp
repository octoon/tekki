```cpp
#include "tekki/ash_imgui/lib.h"
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <stdexcept>
#include <cstring>

namespace tekki::ash_imgui {

VkShaderModule Renderer::LoadShaderModule(VkDevice device, const std::vector<uint8_t>& bytes) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = bytes.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

uint32_t Renderer::GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags) {
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[i];
        if ((typeFilter & (1 << i)) && (memoryType.propertyFlags & propertyFlags) == propertyFlags) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

uint32_t Renderer::AlignUp(uint32_t x, uint32_t alignment) {
    return (x + alignment - 1) & ~(alignment - 1);
}

Renderer::Renderer(VkDevice device, const VkPhysicalDeviceProperties& physicalDeviceProperties, const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, ImGuiContext* imgui) {
    try {
        // Load shaders
        std::vector<uint8_t> vertexShaderBytes; // Should be populated with actual SPIR-V data
        std::vector<uint8_t> fragmentShaderBytes; // Should be populated with actual SPIR-V data
        VertexShader = LoadShaderModule(device, vertexShaderBytes);
        FragmentShader = LoadShaderModule(device, fragmentShaderBytes);

        // Create sampler
        VkSampler sampler;
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        if (vkCreateSampler(device, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sampler");
        }

        // Create descriptor set layout
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBinding.pImmutableSamplers = &sampler;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &layoutBinding;
        if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout");
        }

        // Create pipeline layout
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = PushConstantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        // Initialize buffers and memory
        VkDeviceSize hostAllocationSize = 0;
        uint32_t hostMemoryTypeFilter = 0xFFFFFFFF;

        // Create vertex buffers
        VkBufferCreateInfo vertexBufferCreateInfo{};
        vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferCreateInfo.size = VertexCountPerFrame * sizeof(ImDrawVert);
        vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        for (size_t i = 0; i < FrameCount; i++) {
            if (vkCreateBuffer(device, &vertexBufferCreateInfo, nullptr, &VertexBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create vertex buffer");
            }
            
            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, VertexBuffers[i], &memReq);
            if (memReq.size != vertexBufferCreateInfo.size) {
                throw std::runtime_error("Vertex buffer memory requirements mismatch");
            }
            
            VertexMemOffsets[i] = hostAllocationSize;
            hostAllocationSize += vertexBufferCreateInfo.size;
            hostMemoryTypeFilter &= memReq.memoryTypeBits;
        }

        // Create index buffers
        VkBufferCreateInfo indexBufferCreateInfo{};
        indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufferCreateInfo.size = IndexCountPerFrame * sizeof(ImDrawIdx);
        indexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        for (size_t i = 0; i < FrameCount; i++) {
            if (vkCreateBuffer(device, &indexBufferCreateInfo, nullptr, &IndexBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create index buffer");
            }
            
            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, IndexBuffers[i], &memReq);
            if (memReq.size != indexBufferCreateInfo.size) {
                throw std::runtime_error("Index buffer memory requirements mismatch");
            }
            
            IndexMemOffsets[i] = hostAllocationSize;
            hostAllocationSize += indexBufferCreateInfo.size;
            hostMemoryTypeFilter &= memReq.memoryTypeBits;
        }

        // Create font texture and image buffer
        ImFontAtlas* fonts = ImGui::GetIO().Fonts;
        unsigned char* fontData;
        int fontWidth, fontHeight;
        fonts->GetTexDataAsAlpha8(&fontData, &fontWidth, &fontHeight);

        VkBufferCreateInfo imageBufferCreateInfo{};
        imageBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        imageBufferCreateInfo.size = static_cast<VkDeviceSize>(fontWidth * fontHeight);
        imageBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (vkCreateBuffer(device, &imageBufferCreateInfo, nullptr, &ImageBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image buffer");
        }

        VkMemoryRequirements imageMemReq;
        vkGetBufferMemoryRequirements(device, ImageBuffer, &imageMemReq);
        if (imageMemReq.size != imageBufferCreateInfo.size) {
            throw std::runtime_error("Image buffer memory requirements mismatch");
        }

        size_t imageMemOffset = hostAllocationSize;
        hostAllocationSize += imageBufferCreateInfo.size;
        hostMemoryTypeFilter &= imageMemReq.memoryTypeBits;

        // Allocate host memory
        uint32_t hostMemoryTypeIndex = GetMemoryTypeIndex(physicalDeviceMemoryProperties, hostMemoryTypeFilter, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        
        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = hostAllocationSize;
        memoryAllocateInfo.memoryTypeIndex = hostMemoryTypeIndex;
        
        if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &HostMem) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate host memory");
        }

        // Bind buffers to memory
        for (size_t i = 0; i < FrameCount; i++) {
            if (vkBindBufferMemory(device, VertexBuffers[i], HostMem, VertexMemOffsets[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind vertex buffer memory");
            }
            if (vkBindBufferMemory(device, IndexBuffers[i], HostMem, IndexMemOffsets[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind index buffer memory");
            }
        }
        if (vkBindBufferMemory(device, ImageBuffer, HostMem, imageMemOffset) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image buffer memory");
        }

        // Map host memory
        if (vkMapMemory(device, HostMem, 0, VK_WHOLE_SIZE, 0, &HostMapping) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map host memory");
        }

        // Create font image
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = VK_FORMAT_R8_UNORM;
        imageCreateInfo.extent = {static_cast<uint32_t>(fontWidth), static_cast<uint32_t>(fontHeight), 1};
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        if (vkCreateImage(device, &imageCreateInfo, nullptr, &Image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image");
        }

        VkMemoryRequirements imageMemoryReq;
        vkGetImageMemoryRequirements(device, Image, &imageMemoryReq);
        
        uint32_t localMemoryTypeIndex = GetMemoryTypeIndex(physicalDeviceMemoryProperties, imageMemoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkMemoryAllocateInfo localMemoryAllocateInfo{};
        localMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        localMemoryAllocateInfo.allocationSize = imageMemoryReq.size;
        localMemoryAllocateInfo.memoryTypeIndex = localMemoryTypeIndex;
        
        if (vkAllocateMemory(device, &localMemoryAllocateInfo, nullptr, &LocalMem) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate local memory");
        }
        
        if (vkBindImageMemory(device, Image, LocalMem, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind image memory");
        }

        // Create descriptor pool and set
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = 1;
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &poolSize;
        
        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
        
        if (vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &DescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }

        // Create image view
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = Image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = VK_FORMAT_R8_UNORM;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_ONE;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_ONE;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_ONE;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_R;
        
        VkImageView imageView;
        if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }

        // Update descriptor set
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = DescriptorSet;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pImageInfo = &imageInfo;
        
        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

        AtomSize = physicalDeviceProperties.limits.nonCoherentAtomSize;

        // Copy font data to mapped memory
        uint8_t* imageBase = reinterpret_cast<uint8_t*>(HostMapping) + imageMemOffset;
        std::memcpy(imageBase, fontData, fontWidth * fontHeight);

        ImageWidth = fontWidth;
        ImageHeight = fontHeight;
        FrameIndex = 0;
        ImageNeedsCopy = true;
        Pipeline = VK_NULL_HANDLE;

        // Cleanup temporary objects
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroySampler(device, sampler, nullptr);

    } catch (...) {
        // Cleanup on failure
        if (VertexShader != VK_NULL_HANDLE) vkDestroyShaderModule(device, VertexShader, nullptr);
        if (FragmentShader != VK_NULL_HANDLE) vkDestroyShaderModule(device, FragmentShader, nullptr);
        if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
        for (auto buffer : VertexBuffers) if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
        for (auto buffer : IndexBuffers) if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
        if (ImageBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, ImageBuffer, nullptr);
        if (Image != VK_NULL_HANDLE) vkDestroyImage(device, Image, nullptr);
        if (HostMem != VK_NULL_HANDLE) vkFreeMemory(device, HostMem, nullptr);
        if (LocalMem != VK_NULL_HANDLE) vkFreeMemory(device, LocalMem, nullptr);
        throw;
    }
}

Renderer::~Renderer() {
    // Destructor should be implemented to clean up resources
    // Actual cleanup would require access to VkDevice
}

void Renderer::BeginFrame(VkDevice device, VkCommandBuffer commandBuffer) {
    FrameIndex = (1 + FrameIndex) % FrameCount;

    if (ImageNeedsCopy) {
        VkImageMemoryBarrier transferFromUndef{};
        transferFromUndef.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transferFromUndef.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transferFromUndef.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transferFromUndef.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferFromUndef.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferFromUndef.image = Image;
        transferFromUndef.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        transferFromUndef.subresourceRange.levelCount = 1;
        transferFromUndef.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &transferFromUndef
        );

        VkBufferImageCopy bufferImageCopy{};
        bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferImageCopy.imageSubresource.layerCount = 1;
        bufferImageCopy.imageExtent = {ImageWidth, ImageHeight, 1};

        vkCmdCopyBufferToImage(
            commandBuffer,
            ImageBuffer,
            Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &bufferImageCopy
        );

        VkImageMemoryBarrier shaderFromTransfer{};
        shaderFromTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        shaderFromTransfer.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        shaderFromTransfer.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        shaderFromTransfer.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        shaderFromTransfer.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shaderFromTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shaderFromTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        shaderFromTransfer.image = Image;
        shaderFromTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        shaderFromTransfer.subresourceRange.levelCount = 1;
        shaderFromTransfer.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &shaderFromTransfer
        );

        ImageNeedsCopy = false;
    }
}

bool Renderer::HasPipeline() const {
    return Pipeline != VK_NULL_HANDLE;
}

VkPipeline Renderer::CreatePipeline(VkDevice device, VkRenderPass renderPass) {
    if (Pipeline != VK_NULL_HANDLE) {
        return Pipeline;
    }

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2]{};
    shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageCreateInfo[0].module = VertexShader;
    shaderStageCreateInfo[0].pName = "main";
    
    shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageCreateInfo[1].module = FragmentShader;
    shaderStageCreateInfo[1].pName = "main";

    VkVertexInputBindingDescription vertexInputBinding{};
    vertexInputBinding.binding = 0;
    vertexInputBinding.stride = sizeof(ImDrawVert);
    vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributes[3]{};
    vertexInputAttributes[0].location = 0;
    vertexInputAttributes[0].binding = 0;
    vertexInputAttributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributes[0].offset = offsetof(ImDrawVert, pos);
    
    vertexInputAttributes[1].location = 1;
    vertexInputAttributes[1].binding = 0;
    vertexInputAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributes[1].offset = offsetof(ImDrawVert, uv);
    
    vertexInputAttributes[2].location = 2;
    vertexInputAttributes[2].binding = 0;
    vertexInputAttributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertexInputAttributes[2].offset = offsetof(ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 3;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.dynamicStateCount = 2;
    pipelineDynamicStateCreateInfo.pDynamicStates