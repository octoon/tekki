#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "vulkan/buffer.h"
#include "vulkan/image.h"

namespace tekki::backend {

class TransientResourceCache {
public:
    TransientResourceCache() = default;
    
    std::shared_ptr<Image> GetImage(const ImageDesc& desc) {
        auto it = images.find(desc);
        if (it != images.end() && !it->second.empty()) {
            auto image = it->second.back();
            it->second.pop_back();
            return image;
        }
        return nullptr;
    }

    void InsertImage(std::shared_ptr<Image> image) {
        auto& entry = images[image->desc];
        entry.push_back(image);
    }

    std::shared_ptr<Buffer> GetBuffer(const BufferDesc& desc) {
        auto it = buffers.find(desc);
        if (it != buffers.end() && !it->second.empty()) {
            auto buffer = it->second.back();
            it->second.pop_back();
            return buffer;
        }
        return nullptr;
    }

    void InsertBuffer(std::shared_ptr<Buffer> buffer) {
        auto& entry = buffers[buffer->desc];
        entry.push_back(buffer);
    }

private:
    std::unordered_map<ImageDesc, std::vector<std::shared_ptr<Image>>> images;
    std::unordered_map<BufferDesc, std::vector<std::shared_ptr<Buffer>>> buffers;
};

} // namespace tekki::backend