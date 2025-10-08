// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/transient_resource_cache.rs

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "core/common.h"

namespace tekki::backend::vulkan
{

struct TransientBufferDesc
{
    size_t size;
    vk::BufferUsageFlags usage;

    bool operator==(const TransientBufferDesc& other) const { return size == other.size && usage == other.usage; }
};

struct TransientBufferDescHash
{
    std::size_t operator()(const TransientBufferDesc& desc) const
    {
        return std::hash<size_t>{}(desc.size) ^ std::hash<uint32_t>{}(static_cast<uint32_t>(desc.usage));
    }
};

struct TransientImageDesc
{
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageUsageFlags usage;
    uint32_t mip_levels;
    uint32_t array_layers;

    bool operator==(const TransientImageDesc& other) const
    {
        return extent.width == other.extent.width && extent.height == other.extent.height &&
               extent.depth == other.extent.depth && format == other.format && usage == other.usage &&
               mip_levels == other.mip_levels && array_layers == other.array_layers;
    }
};

struct TransientImageDescHash
{
    std::size_t operator()(const TransientImageDesc& desc) const
    {
        return std::hash<uint32_t>{}(desc.extent.width) ^ std::hash<uint32_t>{}(desc.extent.height) ^
               std::hash<uint32_t>{}(desc.extent.depth) ^ std::hash<int>{}(static_cast<int>(desc.format)) ^
               std::hash<uint32_t>{}(static_cast<uint32_t>(desc.usage)) ^ std::hash<uint32_t>{}(desc.mip_levels) ^
               std::hash<uint32_t>{}(desc.array_layers);
    }
};

class TransientBuffer
{
public:
    TransientBuffer(std::shared_ptr<class Buffer> buffer, uint64_t frame_id);
    ~TransientBuffer();

    std::shared_ptr<class Buffer> buffer() const { return buffer_; }
    uint64_t frame_id() const { return frame_id_; }

private:
    std::shared_ptr<class Buffer> buffer_;
    uint64_t frame_id_;
};

class TransientImage
{
public:
    TransientImage(std::shared_ptr<class Image> image, uint64_t frame_id);
    ~TransientImage();

    std::shared_ptr<class Image> image() const { return image_; }
    uint64_t frame_id() const { return frame_id_; }

private:
    std::shared_ptr<class Image> image_;
    uint64_t frame_id_;
};

class TransientResourceCache
{
public:
    TransientResourceCache(const std::shared_ptr<class Device>& device);
    ~TransientResourceCache();

    // Buffer management
    std::shared_ptr<TransientBuffer> get_or_create_buffer(const TransientBufferDesc& desc, const std::string& name);

    // Image management
    std::shared_ptr<TransientImage> get_or_create_image(const TransientImageDesc& desc, const std::string& name);

    // Frame management
    void begin_frame(uint64_t frame_id);
    void end_frame();

    // Cache maintenance
    void maintain();
    void clear();

private:
    std::shared_ptr<class Device> device_;
    uint64_t current_frame_id_{0};

    // Buffer cache
    std::unordered_map<TransientBufferDesc, std::vector<std::shared_ptr<TransientBuffer>>, TransientBufferDescHash>
        buffer_cache_;

    // Image cache
    std::unordered_map<TransientImageDesc, std::vector<std::shared_ptr<TransientImage>>, TransientImageDescHash>
        image_cache_;

    // Active resources for current frame
    std::vector<std::shared_ptr<TransientBuffer>> active_buffers_;
    std::vector<std::shared_ptr<TransientImage>> active_images_;

    // Helper methods
    void cleanup_old_resources();
    std::shared_ptr<TransientBuffer> find_available_buffer(const TransientBufferDesc& desc);
    std::shared_ptr<TransientImage> find_available_image(const TransientImageDesc& desc);
};

} // namespace tekki::backend::vulkan