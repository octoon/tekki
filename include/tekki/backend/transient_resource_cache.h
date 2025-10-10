#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::backend {

class TransientResourceCache {
public:
    TransientResourceCache() = default;

    std::shared_ptr<vulkan::Image> GetImage(const vulkan::ImageDesc& desc);

    void InsertImage(std::shared_ptr<vulkan::Image> image);

    std::shared_ptr<vulkan::Buffer> GetBuffer(const vulkan::BufferDesc& desc);

    void InsertBuffer(std::shared_ptr<vulkan::Buffer> buffer);

private:
    std::unordered_map<vulkan::ImageDesc, std::vector<std::shared_ptr<vulkan::Image>>, vulkan::ImageDesc::Hash> images;
    std::unordered_map<vulkan::BufferDesc, std::vector<std::shared_ptr<vulkan::Buffer>>> buffers;
};

} // namespace tekki::backend