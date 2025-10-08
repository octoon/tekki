// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/profiler.rs

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "tekki/core/common.h"

namespace tekki::backend::vulkan
{

class Device;

// GPU Profiler buffer wrapper
class ProfilerBuffer
{
public:
    ProfilerBuffer(vk::Buffer buffer, std::shared_ptr<class Allocation> allocation);
    ~ProfilerBuffer();

    vk::Buffer raw() const { return buffer_; }
    const uint8_t* mapped_slice() const;

private:
    vk::Buffer buffer_;
    std::shared_ptr<class Allocation> allocation_;
};

// GPU Profiler backend
class ProfilerBackend
{
public:
    ProfilerBackend(std::shared_ptr<Device> device, std::shared_ptr<class Allocator> allocator, float timestamp_period);

    std::unique_ptr<ProfilerBuffer> create_query_result_buffer(size_t bytes);
    float timestamp_period() const { return timestamp_period_; }

private:
    std::shared_ptr<Device> device_;
    std::shared_ptr<class Allocator> allocator_;
    float timestamp_period_;
};

// GPU Profiler frame data
class ProfilerFrame
{
public:
    ProfilerFrame(std::unique_ptr<ProfilerBuffer> buffer);
    ~ProfilerFrame();

    const ProfilerBuffer* buffer() const { return buffer_.get(); }

private:
    std::unique_ptr<ProfilerBuffer> buffer_;
};

// Main GPU profiler interface
class GpuProfiler
{
public:
    GpuProfiler(std::shared_ptr<Device> device, std::shared_ptr<class Allocator> allocator);
    ~GpuProfiler();

    // Frame management
    void begin_frame(vk::CommandBuffer command_buffer);
    void end_frame(vk::CommandBuffer command_buffer);

    // Query management
    void begin_query(vk::CommandBuffer command_buffer, const std::string& name);
    void end_query(vk::CommandBuffer command_buffer, const std::string& name);

    // Results
    void retrieve_results();
    void print_results();

private:
    std::shared_ptr<Device> device_;
    std::shared_ptr<class Allocator> allocator_;
    std::unique_ptr<ProfilerBackend> backend_;
    std::unique_ptr<ProfilerFrame> current_frame_;

    vk::QueryPool query_pool_;
    std::vector<std::string> query_names_;
    uint32_t current_query_index_{0};
    bool frame_active_{false};
};

} // namespace tekki::backend::vulkan