// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/device.rs

#include "backend/vulkan/device.h"
#include "backend/vulkan/allocator.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>
#include <set>

namespace tekki::backend::vulkan {

// DeviceFrame implementation
DeviceFrame::DeviceFrame(
    const std::shared_ptr<PhysicalDevice>& pdevice,
    vk::Device device,
    class VulkanAllocator& allocator,
    const class QueueFamily& queue_family
) {
    // Create command buffers
    main_command_buffer = CommandBuffer(device, queue_family);
    presentation_command_buffer = CommandBuffer(device, queue_family);

    // TODO: Initialize profiler data
    // profiler_data = VulkanProfilerFrame::new(...);
}

DeviceFrame::~DeviceFrame() {
    // Resources will be cleaned up by Device
}

void DeviceFrame::PendingResourceReleases::release_all(vk::Device device) {
    for (auto pool : descriptor_pools) {
        device.destroyDescriptorPool(pool);
    }
    descriptor_pools.clear();
}

// CommandBuffer implementation
CommandBuffer::CommandBuffer(vk::Device device, const class QueueFamily& queue_family) {
    vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_family.index
    };

    command_pool_ = device.createCommandPool(pool_info);

    vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = command_pool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    auto buffers = device.allocateCommandBuffers(alloc_info);
    command_buffer_ = buffers[0];

    vk::FenceCreateInfo fence_info{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    submit_done_fence_ = device.createFence(fence_info);
}

CommandBuffer::~CommandBuffer() {
    // Resources should be cleaned up by Device
}

// Device implementation
std::shared_ptr<Device> Device::create(const std::shared_ptr<PhysicalDevice>& pdevice) {
    auto device = std::make_shared<Device>();
    device->pdevice_ = pdevice;

    // Check supported extensions
    auto supported_extensions = pdevice->instance->raw().enumerateDeviceExtensionProperties(pdevice->raw);

    std::set<std::string> supported_extension_names;
    for (const auto& ext : supported_extensions) {
        supported_extension_names.insert(ext.extensionName);
    }

    spdlog::debug("Supported device extensions:");
    for (const auto& ext : supported_extension_names) {
        spdlog::debug("  - {}", ext);
    }

    // Required device extensions
    std::vector<const char*> device_extension_names{
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME
    };

    // Ray tracing extensions
    std::vector<const char*> ray_tracing_extensions{
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
    };

    // Check ray tracing support
    device->ray_tracing_enabled_ = true;
    for (const auto& ext : ray_tracing_extensions) {
        if (!supported_extension_names.contains(ext)) {
            spdlog::info("Ray tracing extension not supported: {}", ext);
            device->ray_tracing_enabled_ = false;
            break;
        }
    }

    if (device->ray_tracing_enabled_) {
        spdlog::info("All ray tracing extensions are supported");
        device_extension_names.insert(
            device_extension_names.end(),
            ray_tracing_extensions.begin(),
            ray_tracing_extensions.end()
        );
    }

    // Add swapchain extension if needed
    if (pdevice->presentation_requested) {
        device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Verify all required extensions are supported
    for (const auto& ext : device_extension_names) {
        if (!supported_extension_names.contains(ext)) {
            throw std::runtime_error("Device extension not supported: " + std::string(ext));
        }
    }

    // Find universal queue
    auto universal_queue_family = pdevice->find_queue_family(vk::QueueFlagBits::eGraphics);
    if (!universal_queue_family) {
        throw std::runtime_error("No suitable render queue found");
    }

    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo queue_info{
        .queueFamilyIndex = universal_queue_family->index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    // Device features
    vk::PhysicalDeviceFeatures2 features2;
    vk::PhysicalDeviceScalarBlockLayoutFeaturesEXT scalar_block;
    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing;
    vk::PhysicalDeviceImagelessFramebufferFeaturesKHR imageless_framebuffer;
    vk::PhysicalDeviceShaderFloat16Int8Features shader_float16_int8;
    vk::PhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model;
    vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address;

    features2.pNext = &scalar_block;
    scalar_block.pNext = &descriptor_indexing;
    descriptor_indexing.pNext = &imageless_framebuffer;
    imageless_framebuffer.pNext = &shader_float16_int8;
    shader_float16_int8.pNext = &vulkan_memory_model;
    vulkan_memory_model.pNext = &buffer_device_address;

    // Ray tracing features
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features;

    if (device->ray_tracing_enabled_) {
        buffer_device_address.pNext = &acceleration_structure_features;
        acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
    }

    // Get features
    pdevice->raw.getFeatures2(&features2);

    // Validate required features
    assert(scalar_block.scalarBlockLayout);
    assert(descriptor_indexing.shaderUniformTexelBufferArrayDynamicIndexing);
    assert(descriptor_indexing.shaderStorageTexelBufferArrayDynamicIndexing);
    assert(descriptor_indexing.shaderSampledImageArrayNonUniformIndexing);
    assert(descriptor_indexing.shaderStorageImageArrayNonUniformIndexing);
    assert(descriptor_indexing.shaderUniformTexelBufferArrayNonUniformIndexing);
    assert(descriptor_indexing.shaderStorageTexelBufferArrayNonUniformIndexing);
    assert(descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind);
    assert(descriptor_indexing.descriptorBindingUpdateUnusedWhilePending);
    assert(descriptor_indexing.descriptorBindingPartiallyBound);
    assert(descriptor_indexing.descriptorBindingVariableDescriptorCount);
    assert(descriptor_indexing.runtimeDescriptorArray);
    assert(imageless_framebuffer.imagelessFramebuffer);
    assert(shader_float16_int8.shaderInt8);

    if (device->ray_tracing_enabled_) {
        assert(descriptor_indexing.shaderUniformBufferArrayNonUniformIndexing);
        assert(descriptor_indexing.shaderStorageBufferArrayNonUniformIndexing);
        assert(vulkan_memory_model.vulkanMemoryModel);
        assert(acceleration_structure_features.accelerationStructure);
        assert(acceleration_structure_features.descriptorBindingAccelerationStructureUpdateAfterBind);
        assert(ray_tracing_pipeline_features.rayTracingPipeline);
        assert(ray_tracing_pipeline_features.rayTracingPipelineTraceRaysIndirect);
        assert(buffer_device_address.bufferDeviceAddress);
    }

    // Create device
    vk::DeviceCreateInfo device_info{
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(device_extension_names.size()),
        .ppEnabledExtensionNames = device_extension_names.data(),
        .pNext = &features2
    };

    device->device_ = pdevice->raw.createDevice(device_info);
    spdlog::info("Created a Vulkan device");

    // Get queue
    device->universal_queue_ = Queue{
        .raw = device->device_.getQueue(universal_queue_family->index, 0),
        .family = *universal_queue_family
    };

    // Create allocator
    // TODO: Initialize VulkanAllocator
    // device->global_allocator_ = std::make_shared<VulkanAllocator>(...);

    // Create frames
    for (size_t i = 0; i < FRAME_COUNT; ++i) {
        // TODO: Initialize DeviceFrame
        // device->frames_[i] = std::make_shared<DeviceFrame>(...);
    }

    // Create samplers
    device->immutable_samplers_ = create_samplers(device->device_);

    // Create setup command buffer
    device->setup_cb_ = std::make_unique<CommandBuffer>(device->device_, universal_queue_family.value());

    // Create crash tracking buffer
    // TODO: Implement

    // Load ray tracing extensions
    if (device->ray_tracing_enabled_) {
        // TODO: Load acceleration structure and ray tracing pipeline extensions
    }

    // Initialize GPU profiler
    if (device->global_allocator_) {
        device->gpu_profiler_ = std::make_shared<GpuProfiler>(device, device->global_allocator_);
        spdlog::info("GPU profiler initialized");
    }

    // Check DLSS availability
    device->dlss_available_ = DlssUtils::check_dlss_availability(device);
    if (device->dlss_available_) {
        spdlog::info("DLSS is available on this system");
    } else {
        spdlog::info("DLSS is not available on this system");
    }

    return device;
}

Device::~Device() {
    if (device_) {
        device_.waitIdle();

        // Cleanup resources
        for (auto& [desc, sampler] : immutable_samplers_) {
            device_.destroySampler(sampler);
        }

        device_.destroy();
    }
}

std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash>
Device::create_samplers(vk::Device device) {
    std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash> result;

    std::vector<vk::Filter> texel_filters{
        vk::Filter::eNearest,
        vk::Filter::eLinear
    };

    std::vector<vk::SamplerMipmapMode> mipmap_modes{
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerMipmapMode::eLinear
    };

    std::vector<vk::SamplerAddressMode> address_modes{
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eClampToEdge
    };

    for (auto texel_filter : texel_filters) {
        for (auto mipmap_mode : mipmap_modes) {
            for (auto address_mode : address_modes) {
                bool anisotropy_enable = (texel_filter == vk::Filter::eLinear);

                vk::SamplerCreateInfo sampler_info{
                    .magFilter = texel_filter,
                    .minFilter = texel_filter,
                    .mipmapMode = mipmap_mode,
                    .addressModeU = address_mode,
                    .addressModeV = address_mode,
                    .addressModeW = address_mode,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = anisotropy_enable,
                    .maxAnisotropy = 16.0f,
                    .compareEnable = VK_FALSE,
                    .compareOp = vk::CompareOp::eAlways,
                    .minLod = 0.0f,
                    .maxLod = VK_LOD_CLAMP_NONE,
                    .borderColor = vk::BorderColor::eFloatTransparentBlack,
                    .unnormalizedCoordinates = VK_FALSE
                };

                auto sampler = device.createSampler(sampler_info);
                result[SamplerDesc{texel_filter, mipmap_mode, address_mode}] = sampler;
            }
        }
    }

    return result;
}

vk::Sampler Device::get_sampler(const SamplerDesc& desc) {
    auto it = immutable_samplers_.find(desc);
    if (it != immutable_samplers_.end()) {
        return it->second;
    }
    throw std::runtime_error("Sampler not found");
}

uint32_t Device::max_bindless_descriptor_count() const {
    return std::min(
        512u * 1024u,
        pdevice_->properties.limits.maxPerStageDescriptorSampledImages - RESERVED_DESCRIPTOR_COUNT
    );
}

// TODO: Implement remaining methods
std::shared_ptr<DeviceFrame> Device::begin_frame() {
    // TODO: Implement frame begin logic
    return frames_[0];
}

void Device::finish_frame(const std::shared_ptr<DeviceFrame>& frame) {
    // TODO: Implement frame finish logic
}

void Device::with_setup_cb(std::function<void(vk::CommandBuffer)> callback) {
    // TODO: Implement setup command buffer logic
}

std::shared_ptr<Buffer> Device::create_buffer(
    const BufferDesc& desc,
    const std::string& name,
    const std::vector<uint8_t>* initial_data
) {
    // TODO: Implement buffer creation
    return nullptr;
}

void Device::immediate_destroy_buffer(const std::shared_ptr<Buffer>& buffer) {
    // TODO: Implement buffer destruction
}

std::shared_ptr<DlssRenderer> Device::create_dlss_renderer(
    const glm::uvec2& input_resolution,
    const glm::uvec2& target_resolution
) {
    if (!dlss_available_) {
        spdlog::error("Cannot create DLSS renderer: DLSS not available");
        return nullptr;
    }

    try {
        auto renderer = std::make_shared<DlssRenderer>(
            shared_from_this(),
            input_resolution,
            target_resolution
        );
        spdlog::info("DLSS renderer created for resolution [{}, {}] -> [{}, {}]",
                     input_resolution.x, input_resolution.y,
                     target_resolution.x, target_resolution.y);
        return renderer;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create DLSS renderer: {}", e.what());
        return nullptr;
    }
}

} // namespace tekki::backend::vulkan