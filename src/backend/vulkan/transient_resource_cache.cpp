// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/transient_resource_cache.rs

#include "backend/vulkan/transient_resource_cache.h"

#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan {

TransientBuffer::TransientBuffer(std::shared_ptr<class Buffer> buffer, uint64_t frame_id)
    : buffer_(std::move(buffer)), frame_id_(frame_id) {
}

TransientBuffer::~TransientBuffer() {
    // Buffer will be destroyed by the cache
}

TransientImage::TransientImage(std::shared_ptr<class Image> image, uint64_t frame_id)
    : image_(std::move(image)), frame_id_(frame_id) {
}

TransientImage::~TransientImage() {
    // Image will be destroyed by the cache
}

TransientResourceCache::TransientResourceCache(const std::shared_ptr<class Device>& device)
    : device_(device) {
}

TransientResourceCache::~TransientResourceCache() {
    clear();
}

std::shared_ptr<TransientBuffer> TransientResourceCache::get_or_create_buffer(
    const TransientBufferDesc& desc,
    const std::string& name
) {
    // First try to find an available buffer
    auto buffer = find_available_buffer(desc);
    if (buffer) {
        buffer->frame_id_ = current_frame_id_;
        active_buffers_.push_back(buffer);
        return buffer;
    }

    // Create new buffer
    BufferDesc buffer_desc{
        .size = desc.size,
        .usage = desc.usage,
        .memory_location = MemoryLocation::GpuOnly,
        .alignment = std::nullopt
    };

    auto new_buffer = device_->create_buffer(buffer_desc, name);
    if (!new_buffer) {
        throw std::runtime_error("Failed to create transient buffer: " + name);
    }

    auto transient_buffer = std::make_shared<TransientBuffer>(new_buffer, current_frame_id_);
    buffer_cache_[desc].push_back(transient_buffer);
    active_buffers_.push_back(transient_buffer);

    return transient_buffer;
}

std::shared_ptr<TransientImage> TransientResourceCache::get_or_create_image(
    const TransientImageDesc& desc,
    const std::string& name
) {
    // First try to find an available image
    auto image = find_available_image(desc);
    if (image) {
        image->frame_id_ = current_frame_id_;
        active_images_.push_back(image);
        return image;
    }

    // Create new image
    ImageDesc image_desc{
        .extent = desc.extent,
        .format = desc.format,
        .usage = desc.usage,
        .image_type = vk::ImageType::e2D,
        .mip_levels = desc.mip_levels,
        .array_layers = desc.array_layers,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .initial_layout = vk::ImageLayout::eUndefined
    };

    // TODO: Implement image creation
    // auto new_image = device_->create_image(image_desc, name);
    // if (!new_image) {
    //     throw std::runtime_error("Failed to create transient image: " + name);
    // }

    // auto transient_image = std::make_shared<TransientImage>(new_image, current_frame_id_);
    // image_cache_[desc].push_back(transient_image);
    // active_images_.push_back(transient_image);

    // return transient_image;

    spdlog::warn("Transient image creation not yet implemented");
    return nullptr;
}

void TransientResourceCache::begin_frame(uint64_t frame_id) {
    current_frame_id_ = frame_id;
    active_buffers_.clear();
    active_images_.clear();
}

void TransientResourceCache::end_frame() {
    // Cleanup old resources that haven't been used for a while
    cleanup_old_resources();
}

void TransientResourceCache::maintain() {
    // Cleanup resources that are too old
    const uint64_t MAX_FRAME_AGE = 3;

    for (auto& [desc, buffers] : buffer_cache_) {
        buffers.erase(
            std::remove_if(buffers.begin(), buffers.end(),
                [this, MAX_FRAME_AGE](const std::shared_ptr<TransientBuffer>& buffer) {
                    return (current_frame_id_ - buffer->frame_id()) > MAX_FRAME_AGE;
                }),
            buffers.end()
        );
    }

    for (auto& [desc, images] : image_cache_) {
        images.erase(
            std::remove_if(images.begin(), images.end(),
                [this, MAX_FRAME_AGE](const std::shared_ptr<TransientImage>& image) {
                    return (current_frame_id_ - image->frame_id()) > MAX_FRAME_AGE;
                }),
            images.end()
        );
    }

    // Remove empty entries
    for (auto it = buffer_cache_.begin(); it != buffer_cache_.end(); ) {
        if (it->second.empty()) {
            it = buffer_cache_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = image_cache_.begin(); it != image_cache_.end(); ) {
        if (it->second.empty()) {
            it = image_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void TransientResourceCache::clear() {
    buffer_cache_.clear();
    image_cache_.clear();
    active_buffers_.clear();
    active_images_.clear();
}

void TransientResourceCache::cleanup_old_resources() {
    // Cleanup resources that haven't been used for more than 10 frames
    const uint64_t CLEANUP_THRESHOLD = 10;

    for (auto& [desc, buffers] : buffer_cache_) {
        buffers.erase(
            std::remove_if(buffers.begin(), buffers.end(),
                [this, CLEANUP_THRESHOLD](const std::shared_ptr<TransientBuffer>& buffer) {
                    return (current_frame_id_ - buffer->frame_id()) > CLEANUP_THRESHOLD;
                }),
            buffers.end()
        );
    }

    for (auto& [desc, images] : image_cache_) {
        images.erase(
            std::remove_if(images.begin(), images.end(),
                [this, CLEANUP_THRESHOLD](const std::shared_ptr<TransientImage>& image) {
                    return (current_frame_id_ - image->frame_id()) > CLEANUP_THRESHOLD;
                }),
            images.end()
        );
    }
}

std::shared_ptr<TransientBuffer> TransientResourceCache::find_available_buffer(const TransientBufferDesc& desc) {
    auto it = buffer_cache_.find(desc);
    if (it == buffer_cache_.end()) {
        return nullptr;
    }

    // Find a buffer that's not currently in use
    for (auto& buffer : it->second) {
        if (std::find(active_buffers_.begin(), active_buffers_.end(), buffer) == active_buffers_.end()) {
            return buffer;
        }
    }

    return nullptr;
}

std::shared_ptr<TransientImage> TransientResourceCache::find_available_image(const TransientImageDesc& desc) {
    auto it = image_cache_.find(desc);
    if (it == image_cache_.end()) {
        return nullptr;
    }

    // Find an image that's not currently in use
    for (auto& image : it->second) {
        if (std::find(active_images_.begin(), active_images_.end(), image) == active_images_.end()) {
            return image;
        }
    }

    return nullptr;
}

} // namespace tekki::backend::vulkan