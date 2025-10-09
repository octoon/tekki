// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/device.rs

#include "../../include/tekki/backend/vulkan/device.h"

#include <optional>
#include <set>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include "../../include/tekki/backend/error.h"
#include "../../include/tekki/backend/vulkan/allocator.h"

namespace tekki::backend::vulkan
{

// DeviceFrame implementation
DeviceFrame::DeviceFrame(const std::shared_ptr<PhysicalDevice>& pdevice, vk::Device device,
                         class VulkanAllocator& allocator, const class QueueFamily& queue_family)
{
    // Create command buffers
    main_command_buffer = CommandBuffer(device, queue_family);
    presentation_command_buffer = CommandBuffer(device, queue_family);

    // TODO: Initialize profiler data
    // profiler_data = VulkanProfilerFrame::new(...);
}

DeviceFrame::~DeviceFrame()
{
    // Resources will be cleaned up by Device
}

void DeviceFrame::PendingResourceReleases::release_all(vk::Device device)
{
    for (auto pool : descriptor_pools)
    {
        device.destroyDescriptorPool(pool);
    }
    descriptor_pools.clear();
}

// CommandBuffer implementation
CommandBuffer::CommandBuffer(vk::Device device, const class QueueFamily& queue_family)
{
    vk::CommandPoolCreateInfo pool_info{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                        .queueFamilyIndex = queue_family.index};

    command_pool_ = device.createCommandPool(pool_info);

    vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = command_pool_, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

    auto buffers = device.allocateCommandBuffers(alloc_info);
    command_buffer_ = buffers[0];

    vk::FenceCreateInfo fence_info{.flags = vk::FenceCreateFlagBits::eSignaled};

    submit_done_fence_ = device.createFence(fence_info);
}

CommandBuffer::~CommandBuffer()
{
    // Resources should be cleaned up by Device
}

// Device implementation
std::shared_ptr<Device> Device::create(const std::shared_ptr<PhysicalDevice>& pdevice)
{
    auto device = std::make_shared<Device>();
    device->pdevice_ = pdevice;

    // Check supported extensions
    auto supported_extensions = pdevice->instance->raw().enumerateDeviceExtensionProperties(pdevice->raw);

    std::set<std::string> supported_extension_names;
    for (const auto& ext : supported_extensions)
    {
        supported_extension_names.insert(ext.extensionName);
    }

    spdlog::debug("Supported device extensions:");
    for (const auto& ext : supported_extension_names)
    {
        spdlog::debug("  - {}", ext);
    }

    // Required device extensions
    std::vector<const char*> device_extension_names{VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                                                    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
                                                    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                                                    VK_KHR_MAINTENANCE2_EXTENSION_NAME,
                                                    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
                                                    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                                                    VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
                                                    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
                                                    VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
                                                    VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME};

    // Ray tracing extensions
    std::vector<const char*> ray_tracing_extensions{
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,      VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};

    // Check ray tracing support
    device->ray_tracing_enabled_ = true;
    for (const auto& ext : ray_tracing_extensions)
    {
        if (!supported_extension_names.contains(ext))
        {
            spdlog::info("Ray tracing extension not supported: {}", ext);
            device->ray_tracing_enabled_ = false;
            break;
        }
    }

    if (device->ray_tracing_enabled_)
    {
        spdlog::info("All ray tracing extensions are supported");
        device_extension_names.insert(device_extension_names.end(), ray_tracing_extensions.begin(),
                                      ray_tracing_extensions.end());
    }

    // Add swapchain extension if needed
    if (pdevice->presentation_requested)
    {
        device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Verify all required extensions are supported
    for (const auto& ext : device_extension_names)
    {
        if (!supported_extension_names.contains(ext))
        {
            throw std::runtime_error("Device extension not supported: " + std::string(ext));
        }
    }

    // Find universal queue
    auto universal_queue_family = pdevice->find_queue_family(vk::QueueFlagBits::eGraphics);
    if (!universal_queue_family)
    {
        throw std::runtime_error("No suitable render queue found");
    }

    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo queue_info{
        .queueFamilyIndex = universal_queue_family->index, .queueCount = 1, .pQueuePriorities = &queue_priority};

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

    if (device->ray_tracing_enabled_)
    {
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

    if (device->ray_tracing_enabled_)
    {
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
    vk::DeviceCreateInfo device_info{.queueCreateInfoCount = 1,
                                     .pQueueCreateInfos = &queue_info,
                                     .enabledExtensionCount = static_cast<uint32_t>(device_extension_names.size()),
                                     .ppEnabledExtensionNames = device_extension_names.data(),
                                     .pNext = &features2};

    device->device_ = pdevice->raw.createDevice(device_info);
    spdlog::info("Created a Vulkan device");

    // Get queue
    device->universal_queue_ =
        Queue{.raw = device->device_.getQueue(universal_queue_family->index, 0), .family = *universal_queue_family};

    // Create allocator
    device->global_allocator_ =
        VulkanAllocator::Create(pdevice->instance(), pdevice, device->device_);

    // Create frames
    for (size_t i = 0; i < FRAME_COUNT; ++i)
    {
        device->frames_[i] = std::make_shared<DeviceFrame>(pdevice, device->device_, *device->global_allocator_,
                                                            universal_queue_family.value());
    }

    // Create samplers
    device->immutable_samplers_ = create_samplers(device->device_);

    // Create setup command buffer
    device->setup_cb_ = std::make_unique<CommandBuffer>(device->device_, universal_queue_family.value());

    // Create crash tracking buffer
    // TODO: Implement

    // Load ray tracing extensions
    if (device->ray_tracing_enabled_)
    {
        // TODO: Load acceleration structure and ray tracing pipeline extensions
    }

    // Initialize GPU profiler
    if (device->global_allocator_)
    {
        device->gpu_profiler_ = std::make_shared<GpuProfiler>(device, device->global_allocator_);
        spdlog::info("GPU profiler initialized");
    }

    // Check DLSS availability
    device->dlss_available_ = DlssUtils::check_dlss_availability(device);
    if (device->dlss_available_)
    {
        spdlog::info("DLSS is available on this system");
    }
    else
    {
        spdlog::info("DLSS is not available on this system");
    }

    return device;
}

Device::~Device()
{
    if (device_)
    {
        device_.waitIdle();

        // Cleanup resources
        for (auto& [desc, sampler] : immutable_samplers_)
        {
            device_.destroySampler(sampler);
        }

        device_.destroy();
    }
}

std::shared_ptr<Buffer> Device::create_buffer_impl(vk::Device device, VulkanAllocator& allocator,
                                                   const BufferDesc& desc, const std::string& name)
{
    // Create buffer
    vk::BufferCreateInfo buffer_info{
        .size = desc.size, .usage = desc.usage, .sharingMode = vk::SharingMode::eExclusive};

    vk::Buffer buffer = device.createBuffer(buffer_info);

    // Get memory requirements
    vk::MemoryRequirements requirements = device.getBufferMemoryRequirements(buffer);

    if (desc.alignment.has_value())
    {
        requirements.alignment = std::max(requirements.alignment, desc.alignment.value());
    }

    // Handle shader binding table alignment quirk
    if (desc.usage & vk::BufferUsageFlagBits::eShaderBindingTableKHR)
    {
        // AMD requires at least 64 byte alignment
        requirements.alignment = std::max(requirements.alignment, static_cast<vk::DeviceSize>(64));
    }

    // Allocate memory
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    switch (desc.memory_location)
    {
    case MemoryLocation::GpuOnly:
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.requiredFlags = 0;
        break;
    case MemoryLocation::CpuToGpu:
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    case MemoryLocation::GpuToCpu:
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    }

    VmaAllocation allocation;
    VkResult result = vmaAllocateMemoryForBuffer(allocator.raw(), buffer, &alloc_info, &allocation, nullptr);

    if (result != VK_SUCCESS)
    {
        device.destroyBuffer(buffer);
        throw AllocationError(name, "vmaAllocateMemoryForBuffer failed");
    }

    // Bind memory
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(allocator.raw(), allocation, &allocation_info);

    result = vkBindBufferMemory(device, buffer, allocation_info.deviceMemory, allocation_info.offset);
    if (result != VK_SUCCESS)
    {
        vmaFreeMemory(allocator.raw(), allocation);
        device.destroyBuffer(buffer);
        throw VulkanError(static_cast<vk::Result>(result), "vkBindBufferMemory");
    }

    return std::make_shared<Buffer>(buffer, desc, allocation, allocator.raw());
}

std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash> Device::create_samplers(vk::Device device)
{
    std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash> result;

    std::vector<vk::Filter> texel_filters{vk::Filter::eNearest, vk::Filter::eLinear};

    std::vector<vk::SamplerMipmapMode> mipmap_modes{vk::SamplerMipmapMode::eNearest, vk::SamplerMipmapMode::eLinear};

    std::vector<vk::SamplerAddressMode> address_modes{vk::SamplerAddressMode::eRepeat,
                                                      vk::SamplerAddressMode::eClampToEdge};

    for (auto texel_filter : texel_filters)
    {
        for (auto mipmap_mode : mipmap_modes)
        {
            for (auto address_mode : address_modes)
            {
                bool anisotropy_enable = (texel_filter == vk::Filter::eLinear);

                vk::SamplerCreateInfo sampler_info{.magFilter = texel_filter,
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
                                                   .unnormalizedCoordinates = VK_FALSE};

                auto sampler = device.createSampler(sampler_info);
                result[SamplerDesc{texel_filter, mipmap_mode, address_mode}] = sampler;
            }
        }
    }

    return result;
}

vk::Sampler Device::get_sampler(const SamplerDesc& desc)
{
    auto it = immutable_samplers_.find(desc);
    if (it != immutable_samplers_.end())
    {
        return it->second;
    }
    throw std::runtime_error("Sampler not found");
}

uint32_t Device::max_bindless_descriptor_count() const
{
    return std::min(512u * 1024u,
                    pdevice_->properties.limits.maxPerStageDescriptorSampledImages - RESERVED_DESCRIPTOR_COUNT);
}

// TODO: Implement remaining methods
std::shared_ptr<DeviceFrame> Device::begin_frame()
{
    auto& frame = frames_[0];

    // Wait for the GPU to be done with the previously submitted frame
    std::vector<vk::Fence> fences = {frame->main_command_buffer.submit_done_fence(),
                                     frame->presentation_command_buffer.submit_done_fence()};

    auto result = device_.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess)
    {
        throw VulkanError(result, "waitForFences in begin_frame");
    }

    // Release pending resources
    {
        std::lock_guard<std::mutex> lock(frame->pending_resource_releases_mutex);
        frame->pending_resource_releases.release_all(device_);
    }

    return frame;
}

void Device::finish_frame(const std::shared_ptr<DeviceFrame>& frame)
{
    // Swap frames for double buffering
    std::swap(frames_[0], frames_[1]);
}

void Device::with_setup_cb(std::function<void(vk::CommandBuffer)> callback)
{
    if (!setup_cb_)
    {
        throw std::runtime_error("Setup command buffer not initialized");
    }

    // Begin command buffer
    vk::CommandBufferBeginInfo begin_info{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

    setup_cb_->raw().begin(begin_info);

    // Execute callback
    callback(setup_cb_->raw());

    // End command buffer
    setup_cb_->raw().end();

    // Submit to queue
    vk::SubmitInfo submit_info{.commandBufferCount = 1, .pCommandBuffers = &setup_cb_->raw()};

    universal_queue_.raw.submit(1, &submit_info, nullptr);

    // Wait for completion
    device_.waitIdle();
}

std::shared_ptr<Buffer> Device::create_buffer(const BufferDesc& desc, const std::string& name,
                                              const std::vector<uint8_t>* initial_data)
{
    BufferDesc actual_desc = desc;

    if (initial_data)
    {
        actual_desc.usage |= vk::BufferUsageFlagBits::eTransferDst;
    }

    auto buffer = create_buffer_impl(device_, *global_allocator_, actual_desc, name);

    if (initial_data && !initial_data->empty())
    {
        // Create staging buffer
        BufferDesc staging_desc = BufferDesc::new_cpu_to_gpu(actual_desc.size, vk::BufferUsageFlagBits::eTransferSrc);

        auto staging_buffer = create_buffer_impl(device_, *global_allocator_, staging_desc, name + " (staging)");

        // Copy data to staging buffer
        void* mapped = staging_buffer->mapped_data();
        if (!mapped)
        {
            throw std::runtime_error("Failed to map staging buffer");
        }

        std::memcpy(mapped, initial_data->data(), initial_data->size());

        // Copy from staging to final buffer
        with_setup_cb([&](vk::CommandBuffer cb)
                      {
                          vk::BufferCopy copy_region{.srcOffset = 0, .dstOffset = 0, .size = actual_desc.size};
                          cb.copyBuffer(staging_buffer->raw(), buffer->raw(), 1, &copy_region);
                      });

        // Staging buffer will be automatically destroyed
    }

    return buffer;
}

void Device::immediate_destroy_buffer(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer)
    {
        device_.destroyBuffer(buffer->raw());
        vmaFreeMemory(global_allocator_->raw(), buffer->allocation());
    }
}

std::shared_ptr<DlssRenderer> Device::create_dlss_renderer(const glm::uvec2& input_resolution,
                                                           const glm::uvec2& target_resolution)
{
    if (!dlss_available_)
    {
        spdlog::error("Cannot create DLSS renderer: DLSS not available");
        return nullptr;
    }

    try
    {
        auto renderer = std::make_shared<DlssRenderer>(shared_from_this(), input_resolution, target_resolution);
        spdlog::info("DLSS renderer created for resolution [{}, {}] -> [{}, {}]", input_resolution.x,
                     input_resolution.y, target_resolution.x, target_resolution.y);
        return renderer;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to create DLSS renderer: {}", e.what());
        return nullptr;
    }
}

// Crash marker tracking implementation
uint32_t Device::CrashMarkerNames::insert_name(const std::string& name)
{
    const uint32_t idx = next_idx;
    const uint32_t small_idx = idx % 4096;

    next_idx = (next_idx + 1);
    names[small_idx] = {idx, name};

    return idx;
}

std::optional<std::string> Device::CrashMarkerNames::get_name(uint32_t marker) const
{
    const uint32_t small_idx = marker % 4096;
    auto it = names.find(small_idx);
    if (it != names.end() && it->second.first == marker)
    {
        return it->second.second;
    }
    return std::nullopt;
}

void Device::record_crash_marker(const CommandBuffer& cb, const std::string& name)
{
    if (!crash_tracking_buffer_)
    {
        return; // Crash tracking not initialized
    }

    std::lock_guard<std::mutex> lock(crash_marker_mutex_);
    const uint32_t idx = crash_marker_names_.insert_name(name);

    // Write the marker index to the crash tracking buffer
    device_.cmdFillBuffer(cb.raw(), crash_tracking_buffer_->raw, 0, 4, idx);
}

void Device::report_error(const std::exception& err)
{
    // Check if this is a device lost error
    const auto* vulkan_err = dynamic_cast<const tekki::backend::VulkanError*>(&err);
    if (vulkan_err && vulkan_err->GetResult() == vk::Result::eErrorDeviceLost)
    {
        if (crash_tracking_buffer_)
        {
            // Read the last marker written to the crash tracking buffer
            // TODO: This requires mapped memory access - need to implement
            std::lock_guard<std::mutex> lock(crash_marker_mutex_);

            // For now, just log a generic message
            spdlog::error("The GPU device has been lost. This is usually due to an infinite loop in a shader.");
        }
    }

    // Log the error
    spdlog::error("Backend error: {}", err.what());
}

} // namespace tekki::backend::vulkan