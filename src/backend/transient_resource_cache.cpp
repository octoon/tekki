#include "tekki/backend/transient_resource_cache.h"
#include <stdexcept>
#include <memory>

namespace tekki::backend {

std::shared_ptr<Image> TransientResourceCache::GetImage(const ImageDesc& desc) {
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

void TransientResourceCache::InsertImage(std::shared_ptr<Image> image) {
    try {
        if (!image) {
            throw std::invalid_argument("Cannot insert null image");
        }
        auto& entry = images[image->desc];
        entry.push_back(image);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to insert image into cache: " + std::string(e.what()));
    }
}

std::shared_ptr<Buffer> TransientResourceCache::GetBuffer(const BufferDesc& desc) {
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

void TransientResourceCache::InsertBuffer(std::shared_ptr<Buffer> buffer) {
    try {
        if (!buffer) {
            throw std::invalid_argument("Cannot insert null buffer");
        }
        auto& entry = buffers[buffer->desc];
        entry.push_back(buffer);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to insert buffer into cache: " + std::string(e.what()));
    }
}

} // namespace tekki::backend