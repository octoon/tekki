#pragma once

#include "device.h"

#include <vulkan/vulkan.hpp>

namespace tekki::vulkan {

constexpr uint32_t BINDLESS_TEXURES_BINDING_INDEX = 3;

// Bindless descriptor set layout
inline vk::DescriptorSetLayout create_bindless_descriptor_set_layout(std::shared_ptr<Device> device) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // Mesh buffer
        vk::DescriptorSetLayoutBinding{
            0,
            vk::DescriptorType::eStorageBuffer,
            1,
            vk::ShaderStageFlagBits::eAll,
            nullptr
        },
        // Vertex buffer
        vk::DescriptorSetLayoutBinding{
            1,
            vk::DescriptorType::eStorageBuffer,
            1,
            vk::ShaderStageFlagBits::eAll,
            nullptr
        },
        // Bindless texture sizes
        vk::DescriptorSetLayoutBinding{
            2,
            vk::DescriptorType::eStorageBuffer,
            1,
            vk::ShaderStageFlagBits::eAll,
            nullptr
        },
        // Bindless textures
        vk::DescriptorSetLayoutBinding{
            BINDLESS_TEXURES_BINDING_INDEX,
            vk::DescriptorType::eSampledImage,
            device->max_bindless_descriptor_count(),
            vk::ShaderStageFlagBits::eAll,
            nullptr
        }
    };

    vk::DescriptorSetLayoutCreateInfo layout_info{
        vk::DescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data()
    };

    return device->raw().createDescriptorSetLayout(layout_info);
}

// Create bindless descriptor set
inline vk::DescriptorSet create_bindless_descriptor_set(Device* device) {
    auto layout = create_bindless_descriptor_set_layout(std::shared_ptr<Device>(device, [](auto*){}));

    vk::DescriptorSetAllocateInfo alloc_info{
        device->descriptor_pool(),
        1,
        &layout
    };

    auto sets = device->raw().allocateDescriptorSets(alloc_info);
    return sets[0];
}

// Write descriptor set for buffer
inline void write_descriptor_set_buffer(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t dst_binding,
    Buffer* buffer
) {
    vk::DescriptorBufferInfo buffer_info{
        buffer->raw(),
        0,
        vk::WholeSize
    };

    vk::WriteDescriptorSet write_descriptor{
        set,
        dst_binding,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &buffer_info,
        nullptr
    };

    device.updateDescriptorSets(1, &write_descriptor, 0, nullptr);
}

// Write descriptor set for image
inline void write_descriptor_set_image(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t dst_binding,
    uint32_t dst_array_element,
    vk::ImageView image_view
) {
    vk::DescriptorImageInfo image_info{
        vk::Sampler{},
        image_view,
        vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write_descriptor{
        set,
        dst_binding,
        dst_array_element,
        1,
        vk::DescriptorType::eSampledImage,
        &image_info,
        nullptr,
        nullptr
    };

    device.updateDescriptorSets(1, &write_descriptor, 0, nullptr);
}

} // namespace tekki::vulkan