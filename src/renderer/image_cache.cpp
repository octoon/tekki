#include "tekki/renderer/image_cache.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/asset/image.h"
#include "tekki/mesh/TexParams.h"
#include "tekki/mesh/GpuImage.h"
#include "tekki/backend/lib.h"
#include <stdexcept>
#include <algorithm>

namespace tekki::renderer {

UploadGpuImage::UploadGpuImage(std::shared_ptr<tekki::asset::RawImage> image,
                               tekki::mesh::TexParams params,
                               std::shared_ptr<tekki::backend::Device> device)
    : m_image(image), m_params(params), m_device(device) {}

UploadGpuImage::UploadGpuImage(const UploadGpuImage& other)
    : m_image(other.m_image), m_params(other.m_params), m_device(other.m_device) {}

UploadGpuImage& UploadGpuImage::operator=(const UploadGpuImage& other) {
    if (this != &other) {
        m_image = other.m_image;
        m_params = other.m_params;
        m_device = other.m_device;
    }
    return *this;
}

std::shared_ptr<tekki::backend::Image> UploadGpuImage::Execute() {
    if (m_params.use_mips) {
        return CreateImageWithMipmaps();
    } else {
        return CreateImageWithoutMipmaps();
    }
}

std::size_t UploadGpuImage::Hash::operator()(const UploadGpuImage& key) const {
    std::size_t seed = 0;
    auto hash_combine = [&seed](const auto& v) {
        seed ^= std::hash<std::decay_t<decltype(v)>>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };
    
    if (key.m_image) {
        hash_combine(key.m_image->GetWidth());
        hash_combine(key.m_image->GetHeight());
        hash_combine(key.m_image->GetFormat());
        hash_combine(key.m_image->GetData().size());
    }
    
    hash_combine(key.m_params.gamma);
    hash_combine(key.m_params.use_mips);
    
    return seed;
}

std::shared_ptr<tekki::backend::Image> UploadGpuImage::CreateImageWithMipmaps() {
    if (!m_image) {
        throw std::runtime_error("UploadGpuImage: null image");
    }
    
    auto mipmaps = GenerateMipmaps(*m_image);
    
    VkFormat format;
    switch (m_params.gamma) {
        case tekki::mesh::TexGamma::Linear:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case tekki::mesh::TexGamma::Srgb:
            format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        default:
            throw std::runtime_error("UploadGpuImage: unsupported gamma format");
    }
    
    uint32_t width = m_image->GetWidth();
    uint32_t height = m_image->GetHeight();
    uint32_t mip_levels = static_cast<uint32_t>(mipmaps.size() + 1);
    
    std::vector<tekki::backend::ImageSubResourceData> initial_data;
    
    auto& src_data = m_image->GetData();
    initial_data.emplace_back(tekki::backend::ImageSubResourceData{
        .data = src_data.data(),
        .row_pitch = width * 4,
        .slice_pitch = 0
    });
    
    for (uint32_t level = 1; level < mip_levels; level++) {
        auto& mip_data = mipmaps[level - 1];
        uint32_t mip_width = std::max(width >> level, 1u);
        initial_data.emplace_back(tekki::backend::ImageSubResourceData{
            .data = mip_data.data(),
            .row_pitch = mip_width * 4,
            .slice_pitch = 0
        });
    }
    
    tekki::backend::ImageDesc desc;
    desc.format = format;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.mip_levels = mip_levels;
    desc.array_layers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.flags = 0;
    
    return m_device->CreateImage(desc, initial_data);
}

std::shared_ptr<tekki::backend::Image> UploadGpuImage::CreateImageWithoutMipmaps() {
    if (!m_image) {
        throw std::runtime_error("UploadGpuImage: null image");
    }
    
    VkFormat format;
    switch (m_params.gamma) {
        case tekki::mesh::TexGamma::Linear:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case tekki::mesh::TexGamma::Srgb:
            format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        default:
            throw std::runtime_error("UploadGpuImage: unsupported gamma format");
    }
    
    uint32_t width = m_image->GetWidth();
    uint32_t height = m_image->GetHeight();
    
    auto& src_data = m_image->GetData();
    std::vector<tekki::backend::ImageSubResourceData> initial_data = {
        tekki::backend::ImageSubResourceData{
            .data = src_data.data(),
            .row_pitch = width * 4,
            .slice_pitch = 0
        }
    };
    
    tekki::backend::ImageDesc desc;
    desc.format = format;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.flags = 0;
    
    return m_device->CreateImage(desc, initial_data);
}

std::vector<std::vector<uint8_t>> UploadGpuImage::GenerateMipmaps(const tekki::asset::RawImage& src) {
    std::vector<std::vector<uint8_t>> mipmaps;
    
    uint32_t width = src.GetWidth();
    uint32_t height = src.GetHeight();
    auto& src_data = src.GetData();
    
    if (src_data.size() != width * height * 4) {
        throw std::runtime_error("UploadGpuImage: invalid image data size");
    }
    
    std::vector<uint8_t> current_level(src_data.begin(), src_data.end());
    uint32_t current_width = width;
    uint32_t current_height = height;
    
    while (current_width > 1 || current_height > 1) {
        uint32_t next_width = std::max(current_width / 2, 1u);
        uint32_t next_height = std::max(current_height / 2, 1u);
        
        std::vector<uint8_t> next_level(next_width * next_height * 4);
        
        for (uint32_t y = 0; y < next_height; y++) {
            for (uint32_t x = 0; x < next_width; x++) {
                uint32_t src_x0 = x * 2;
                uint32_t src_y0 = y * 2;
                uint32_t src_x1 = std::min(src_x0 + 1, current_width - 1);
                uint32_t src_y1 = std::min(src_y0 + 1, current_height - 1);
                
                for (int channel = 0; channel < 4; channel++) {
                    uint32_t idx00 = (src_y0 * current_width + src_x0) * 4 + channel;
                    uint32_t idx01 = (src_y0 * current_width + src_x1) * 4 + channel;
                    uint32_t idx10 = (src_y1 * current_width + src_x0) * 4 + channel;
                    uint32_t idx11 = (src_y1 * current_width + src_x1) * 4 + channel;
                    
                    uint32_t sum = current_level[idx00] + current_level[idx01] + 
                                  current_level[idx10] + current_level[idx11];
                    uint8_t avg = static_cast<uint8_t>(sum / 4);
                    
                    uint32_t dst_idx = (y * next_width + x) * 4 + channel;
                    next_level[dst_idx] = avg;
                }
            }
        }
        
        mipmaps.push_back(std::move(next_level));
        current_level = mipmaps.back();
        current_width = next_width;
        current_height = next_height;
    }
    
    return mipmaps;
}

} // namespace tekki::renderer