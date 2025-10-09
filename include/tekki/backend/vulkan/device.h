// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/device.rs

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "tekki/core/common.h"
#include "profiler.h"

namespace tekki::backend::vulkan
{

class PhysicalDevice;
class Instance;
class CommandBuffer;

struct Queue
{
    vk::Queue raw;
    QueueFamily family;
};

struct SamplerDesc
{
    vk::Filter texel_filter;
    vk::SamplerMipmapMode mipmap_mode;
    vk::SamplerAddressMode address_modes;

    bool operator==(const SamplerDesc& other) const
    {
        return texel_filter == other.texel_filter && mipmap_mode == other.mipmap_mode &&
               address_modes == other.address_modes;
    }
};

struct SamplerDescHash
{
    std::size_t operator()(const SamplerDesc& desc) const
    {
        return std::hash<int>{}(static_cast<int>(desc.texel_filter)) ^
               std::hash<int>{}(static_cast<int>(desc.mipmap_mode)) ^
               std::hash<int>{}(static_cast<int>(desc.address_modes));
    }
};

class CommandBuffer
{
public:
    CommandBuffer(vk::Device device, const class QueueFamily& queue_family);
    ~CommandBuffer();

    vk::CommandBuffer raw() const { return command_buffer_; }
    vk::Fence submit_done_fence() const { return submit_done_fence_; }

private:
    vk::CommandBuffer command_buffer_;
    vk::CommandPool command_pool_;
    vk::Fence submit_done_fence_;
};

class DeviceFrame
{
public:
    DeviceFrame(const std::shared_ptr<PhysicalDevice>& pdevice, vk::Device device, class VulkanAllocator& allocator,
                const class QueueFamily& queue_family);

    ~DeviceFrame();

    std::optional<vk::Semaphore> swapchain_acquired_semaphore;
    std::optional<vk::Semaphore> rendering_complete_semaphore;
    class CommandBuffer main_command_buffer;
    class CommandBuffer presentation_command_buffer;

    // Resource release tracking
    std::mutex pending_resource_releases_mutex;
    struct PendingResourceReleases
    {
        std::vector<vk::DescriptorPool> descriptor_pools;

        void release_all(vk::Device device);
    } pending_resource_releases;

    // Profiler data
    std::unique_ptr<GpuProfiler> profiler;
};

class Device : public std::enable_shared_from_this<Device>
{
public:
    static std::shared_ptr<Device> create(const std::shared_ptr<PhysicalDevice>& pdevice);

    ~Device();

    // Getters
    vk::Device raw() const { return device_; }
    const Queue& universal_queue() const { return universal_queue_; }
    const std::shared_ptr<PhysicalDevice>& physical_device() const { return pdevice_; }

    // Resource management
    std::shared_ptr<Buffer> create_buffer(const BufferDesc& desc, const std::string& name,
                                          const std::vector<uint8_t>* initial_data = nullptr);

    void immediate_destroy_buffer(const std::shared_ptr<Buffer>& buffer);

    std::shared_ptr<class Image> create_image(const struct ImageDesc& desc,
                                              const std::vector<struct ImageSubResourceData>& initial_data = {});

    void immediate_destroy_image(const std::shared_ptr<class Image>& image);

    vk::ImageView CreateImageView(const struct ImageViewDesc& desc, const struct ImageDesc& image_desc,
                                   vk::Image image);

    // Frame management
    std::shared_ptr<DeviceFrame> begin_frame();
    void finish_frame(const std::shared_ptr<DeviceFrame>& frame);

    // Setup command buffer
    void with_setup_cb(std::function<void(vk::CommandBuffer)> callback);

    // Sampler management
    vk::Sampler get_sampler(const SamplerDesc& desc);

    // Ray tracing
    bool ray_tracing_enabled() const { return ray_tracing_enabled_; }
    uint32_t max_bindless_descriptor_count() const;

    // GPU Profiler
    std::shared_ptr<GpuProfiler> gpu_profiler() const { return gpu_profiler_; }

    // Crash marker tracking (for debugging device lost errors)
    void record_crash_marker(const CommandBuffer& cb, const std::string& name);
    void report_error(const std::exception& err);

private:
    Device() = default;

    static std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash> create_samplers(vk::Device device);

    static std::shared_ptr<Buffer> create_buffer_impl(vk::Device device, class VulkanAllocator& allocator,
                                                      const BufferDesc& desc, const std::string& name);

    std::shared_ptr<PhysicalDevice> pdevice_;
    std::shared_ptr<Instance> instance_;
    vk::Device device_;
    Queue universal_queue_;

    std::shared_ptr<class VulkanAllocator> global_allocator_;
    std::unordered_map<SamplerDesc, vk::Sampler, SamplerDescHash> immutable_samplers_;

    std::unique_ptr<CommandBuffer> setup_cb_;
    std::shared_ptr<Buffer> crash_tracking_buffer_;

    // Crash marker tracking
    struct CrashMarkerNames
    {
        uint32_t next_idx{0};
        std::unordered_map<uint32_t, std::pair<uint32_t, std::string>> names;

        uint32_t insert_name(const std::string& name);
        std::optional<std::string> get_name(uint32_t marker) const;
    };
    mutable std::mutex crash_marker_mutex_;
    CrashMarkerNames crash_marker_names_;

    // Ray tracing extensions
    vk::DispatchLoaderDynamic acceleration_structure_ext_;
    vk::DispatchLoaderDynamic ray_tracing_pipeline_ext_;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties_;

    // Frame management
    static constexpr size_t FRAME_COUNT = 2;
    std::array<std::shared_ptr<DeviceFrame>, FRAME_COUNT> frames_;

    bool ray_tracing_enabled_{false};

    // GPU Profiler
    std::shared_ptr<GpuProfiler> gpu_profiler_;

    static constexpr uint32_t RESERVED_DESCRIPTOR_COUNT = 32;
};

} // namespace tekki::backend::vulkan