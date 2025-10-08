// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/profiler.rs

#include "../../include/tekki/backend/vulkan/profiler.h"

#include <spdlog/spdlog.h>

#include "../../include/tekki/backend/vulkan/allocator.h"
#include "../../include/tekki/backend/vulkan/device.h"

namespace tekki::backend::vulkan
{

// ProfilerBuffer implementation
ProfilerBuffer::ProfilerBuffer(vk::Buffer buffer, std::shared_ptr<Allocation> allocation)
    : buffer_(buffer), allocation_(std::move(allocation))
{
}

ProfilerBuffer::~ProfilerBuffer()
{
    // Buffer and allocation cleanup handled by RAII
}

const uint8_t* ProfilerBuffer::mapped_slice() const
{
    if (allocation_ && allocation_->mapped_data())
    {
        return static_cast<const uint8_t*>(allocation_->mapped_data());
    }
    return nullptr;
}

// ProfilerBackend implementation
ProfilerBackend::ProfilerBackend(std::shared_ptr<Device> device, std::shared_ptr<Allocator> allocator,
                                 float timestamp_period)
    : device_(std::move(device)), allocator_(std::move(allocator)), timestamp_period_(timestamp_period)
{
}

std::unique_ptr<ProfilerBuffer> ProfilerBackend::create_query_result_buffer(size_t bytes)
{
    auto device = device_->raw();

    vk::BufferCreateInfo buffer_info;
    buffer_info.size = bytes;
    buffer_info.usage = vk::BufferUsageFlagBits::eTransferDst;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    auto buffer = device.createBuffer(buffer_info);

    auto requirements = device.getBufferMemoryRequirements(buffer);

    AllocationCreateDesc alloc_desc;
    alloc_desc.name = "profiler_buffer";
    alloc_desc.requirements = requirements;
    alloc_desc.location = MemoryLocation::GpuToCpu;
    alloc_desc.linear = true;

    auto allocation = allocator_->allocate(alloc_desc);

    device.bindBufferMemory(buffer, allocation->memory(), allocation->offset());

    return std::make_unique<ProfilerBuffer>(buffer, std::move(allocation));
}

// ProfilerFrame implementation
ProfilerFrame::ProfilerFrame(std::unique_ptr<ProfilerBuffer> buffer) : buffer_(std::move(buffer)) {}

ProfilerFrame::~ProfilerFrame() {}

// GpuProfiler implementation
GpuProfiler::GpuProfiler(std::shared_ptr<Device> device, std::shared_ptr<Allocator> allocator)
    : device_(std::move(device)), allocator_(std::move(allocator))
{

    // Get timestamp period from physical device
    auto physical_device = device_->physical_device();
    auto properties = physical_device.getProperties();
    float timestamp_period = properties.limits.timestampPeriod;

    backend_ = std::make_unique<ProfilerBackend>(device_, allocator_, timestamp_period);

    // Create query pool
    vk::QueryPoolCreateInfo query_pool_info;
    query_pool_info.queryType = vk::QueryType::eTimestamp;
    query_pool_info.queryCount = 1024; // Maximum queries per frame

    query_pool_ = device_->raw().createQueryPool(query_pool_info);

    query_names_.reserve(1024);
}

GpuProfiler::~GpuProfiler()
{
    if (query_pool_)
    {
        device_->raw().destroyQueryPool(query_pool_);
    }
}

void GpuProfiler::begin_frame(vk::CommandBuffer command_buffer)
{
    if (frame_active_)
    {
        spdlog::warn("GPU profiler frame already active");
        return;
    }

    // Reset query pool
    command_buffer.resetQueryPool(query_pool_, 0, 1024);

    current_query_index_ = 0;
    query_names_.clear();
    frame_active_ = true;

    // Write timestamp at frame start
    command_buffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, query_pool_, current_query_index_++);
    query_names_.push_back("frame_start");
}

void GpuProfiler::end_frame(vk::CommandBuffer command_buffer)
{
    if (!frame_active_)
    {
        spdlog::warn("GPU profiler frame not active");
        return;
    }

    // Write timestamp at frame end
    command_buffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, query_pool_, current_query_index_++);
    query_names_.push_back("frame_end");

    frame_active_ = false;
}

void GpuProfiler::begin_query(vk::CommandBuffer command_buffer, const std::string& name)
{
    if (!frame_active_)
    {
        spdlog::warn("GPU profiler frame not active");
        return;
    }

    if (current_query_index_ >= 1024)
    {
        spdlog::error("GPU profiler query limit exceeded");
        return;
    }

    command_buffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, query_pool_, current_query_index_++);
    query_names_.push_back(name + "_start");
}

void GpuProfiler::end_query(vk::CommandBuffer command_buffer, const std::string& name)
{
    if (!frame_active_)
    {
        spdlog::warn("GPU profiler frame not active");
        return;
    }

    if (current_query_index_ >= 1024)
    {
        spdlog::error("GPU profiler query limit exceeded");
        return;
    }

    command_buffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, query_pool_, current_query_index_++);
    query_names_.push_back(name + "_end");
}

void GpuProfiler::retrieve_results()
{
    if (frame_active_)
    {
        spdlog::warn("Cannot retrieve results while frame is active");
        return;
    }

    if (current_query_index_ == 0)
    {
        return;
    }

    // Create result buffer
    size_t result_size = current_query_index_ * sizeof(uint64_t);
    auto result_buffer = backend_->create_query_result_buffer(result_size);

    // Copy query results
    auto device = device_->raw();
    auto command_pool = device_->command_pool();
    auto command_buffer = device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(command_pool, vk::CommandBufferLevel::ePrimary, 1))[0];

    command_buffer.begin(vk::CommandBufferBeginInfo{});
    command_buffer.copyQueryPoolResults(query_pool_, 0, current_query_index_, result_buffer->raw(), 0, sizeof(uint64_t),
                                        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
    command_buffer.end();

    // Submit and wait
    auto queue = device_->graphics_queue();
    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    queue.submit(submit_info);
    queue.waitIdle();

    device.freeCommandBuffers(command_pool, command_buffer);

    // Store current frame
    current_frame_ = std::make_unique<ProfilerFrame>(std::move(result_buffer));
}

void GpuProfiler::print_results()
{
    if (!current_frame_ || !current_frame_->buffer())
    {
        spdlog::warn("No GPU profiler results available");
        return;
    }

    auto buffer = current_frame_->buffer();
    auto data = buffer->mapped_slice();

    if (!data)
    {
        spdlog::warn("GPU profiler buffer not mapped");
        return;
    }

    const auto* timestamps = reinterpret_cast<const uint64_t*>(data);
    float timestamp_period = backend_->timestamp_period();

    spdlog::info("GPU Profiler Results:");

    for (size_t i = 0; i < query_names_.size(); i += 2)
    {
        if (i + 1 >= query_names_.size())
        {
            break;
        }

        const std::string& start_name = query_names_[i];
        const std::string& end_name = query_names_[i + 1];

        // Extract base name (remove _start/_end suffix)
        std::string base_name = start_name.substr(0, start_name.length() - 6);

        uint64_t start_time = timestamps[i];
        uint64_t end_time = timestamps[i + 1];

        if (start_time == 0 || end_time == 0)
        {
            continue;
        }

        uint64_t duration_ns = (end_time - start_time) * static_cast<uint64_t>(timestamp_period);
        float duration_ms = static_cast<float>(duration_ns) / 1'000'000.0f;

        spdlog::info("  {}: {:.3f} ms", base_name, duration_ms);
    }
}

} // namespace tekki::backend::vulkan