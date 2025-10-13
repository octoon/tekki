#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/asset/image.h"
#include "tekki/asset/TexParams.h"
#include "tekki/asset/GpuImage.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::renderer {

class UploadGpuImage {
public:
    UploadGpuImage(std::shared_ptr<tekki::asset::RawImage> image,
                   tekki::asset::TexParams params,
                   std::shared_ptr<tekki::backend::vulkan::Device> device);

    UploadGpuImage(const UploadGpuImage& other);
    UploadGpuImage& operator=(const UploadGpuImage& other);

    std::shared_ptr<tekki::backend::vulkan::Image> Execute();

    struct Hash {
        std::size_t operator()(const UploadGpuImage& key) const;
    };

private:
    std::shared_ptr<tekki::asset::RawImage> m_image;
    tekki::asset::TexParams m_params;
    std::shared_ptr<tekki::backend::vulkan::Device> m_device;

    std::shared_ptr<tekki::backend::vulkan::Image> CreateImageWithMipmaps();
    std::shared_ptr<tekki::backend::vulkan::Image> CreateImageWithoutMipmaps();
    std::vector<std::vector<uint8_t>> GenerateMipmaps(const tekki::asset::RawImage& src);
};

} // namespace tekki::renderer