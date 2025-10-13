#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <stdexcept>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace tekki::renderer {

class BindlessDescriptorSet {
public:
    static constexpr uint32_t BINDLESS_TEXTURES_BINDING_INDEX = 3;

    /**
     * Creates a bindless descriptor set for the given device
     * @param device The Vulkan device to create the descriptor set for
     * @return The created Vulkan descriptor set
     * @throws std::runtime_error if descriptor set creation fails
     */
    static VkDescriptorSet CreateBindlessDescriptorSet(VkDevice device, uint32_t maxBindlessDescriptorCount) {
        try {
            VkDescriptorSetLayoutBinding bindings[] = {
                // `meshes`
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .pImmutableSamplers = nullptr
                },
                // `vertices`
                {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .pImmutableSamplers = nullptr
                },
                // `bindless_texture_sizes`
                {
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .pImmutableSamplers = nullptr
                },
                // `bindless_textures`
                {
                    .binding = BINDLESS_TEXTURES_BINDING_INDEX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = maxBindlessDescriptorCount,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .pImmutableSamplers = nullptr
                }
            };

            VkDescriptorBindingFlags bindingFlags[] = {
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
            };

            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = static_cast<uint32_t>(std::size(bindingFlags)),
                .pBindingFlags = bindingFlags
            };

            VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &bindingFlagsCreateInfo,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = static_cast<uint32_t>(std::size(bindings)),
                .pBindings = bindings
            };

            VkDescriptorSetLayout descriptorSetLayout;
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptorSetLayout);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout");
            }

            VkDescriptorPoolSize poolSizes[] = {
                {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 3
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = maxBindlessDescriptorCount
                }
            };

            VkDescriptorPoolCreateInfo poolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = 1,
                .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
                .pPoolSizes = poolSizes
            };

            VkDescriptorPool descriptorPool;
            result = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
            if (result != VK_SUCCESS) {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                throw std::runtime_error("Failed to create descriptor pool");
            }

            uint32_t variableDescriptorCount = maxBindlessDescriptorCount;
            VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorSetCount = 1,
                .pDescriptorCounts = &variableDescriptorCount
            };

            VkDescriptorSetAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = &variableCountInfo,
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptorSetLayout
            };

            VkDescriptorSet descriptorSet;
            result = vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);
            if (result != VK_SUCCESS) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                throw std::runtime_error("Failed to allocate descriptor set");
            }

            return descriptorSet;
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to create bindless descriptor set: ") + e.what());
        }
    }

private:
    BindlessDescriptorSet() = delete;
    ~BindlessDescriptorSet() = delete;
};

} // namespace tekki::renderer