#include "tekki/backend/transient_resource_cache.h"
#include <stdexcept>
#include <memory>

namespace tekki::backend {

std::shared_ptr<vulkan::Image> TransientResourceCache::GetImage(const vulkan::ImageDesc& desc) {
    try {
        auto it = images.find(desc);
        if (it != images.end() && !it->second.empty()) {
            auto image = it->second.back();
            it->second.pop_back();
            return image;
        }
        return nullptr;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get image from cache: " + std::string(e.what()));
    }
}

void TransientResourceCache::InsertImage(std::shared_ptr<vulkan::Image> image) {
    try {
        if (!image) {
            throw std::invalid_argument("Cannot insert null image");
        }
        auto& entry = images[image->Desc];
        entry.push_back(image);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to insert image into cache: " + std::string(e.what()));
    }
}

std::shared_ptr<vulkan::Buffer> TransientResourceCache::GetBuffer(const vulkan::BufferDesc& desc) {
    try {
        auto it = buffers.find(desc);
        if (it != buffers.end() && !it->second.empty()) {
            auto buffer = it->second.back();
            it->second.pop_back();
            return buffer;
        }
        return nullptr;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get buffer from cache: " + std::string(e.what()));
    }
}

void TransientResourceCache::InsertBuffer(std::shared_ptr<vulkan::Buffer> buffer) {
    try {
        if (!buffer) {
            throw std::invalid_argument("Cannot insert null buffer");
        }
        auto& entry = buffers[buffer->Desc];
        entry.push_back(buffer);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to insert buffer into cache: " + std::string(e.what()));
    }
}

} // namespace tekki::backend