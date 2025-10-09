#pragma once

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

namespace tekki::renderer {

class UploadGpuImage {
public:
    UploadGpuImage(std::shared_ptr<tekki::asset::RawImage> image, 
                   tekki::mesh::TexParams params,
                   std::shared_ptr<tekki::backend::Device> device);
    
    UploadGpuImage(const UploadGpuImage& other);
    UploadGpuImage& operator=(const UploadGpuImage& other);
    
    std::shared_ptr<tekki::backend::Image> Execute();
    
    struct Hash {
        std::size_t operator()(const UploadGpuImage& key) const;
    };

private:
    std::shared_ptr<tekki::asset::RawImage> m_image;
    tekki::mesh::TexParams m_params;
    std::shared_ptr<tekki::backend::Device> m_device;
    
    std::shared_ptr<tekki::backend::Image> CreateImageWithMipmaps();
    std::shared_ptr<tekki::backend::Image> CreateImageWithoutMipmaps();
    std::vector<std::vector<uint8_t>> GenerateMipmaps(const tekki::asset::RawImage& src);
};

} // namespace tekki::renderer