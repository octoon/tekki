#pragma once

#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/Buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/buffer.h"
#include <glm/glm.hpp>

namespace tekki::render_graph {

// Convert render_graph::ImageType to backend::vulkan::ImageType
inline tekki::backend::vulkan::ImageType ConvertImageType(ImageType type) {
    return static_cast<tekki::backend::vulkan::ImageType>(static_cast<int>(type));
}

// Convert render_graph::ImageDesc to backend::vulkan::ImageDesc
inline tekki::backend::vulkan::ImageDesc ConvertImageDesc(const ImageDesc& rgDesc) {
    tekki::backend::vulkan::ImageDesc vkDesc(
        rgDesc.format,
        ConvertImageType(rgDesc.image_type),
        glm::u32vec3(rgDesc.extent[0], rgDesc.extent[1], rgDesc.extent[2])
    );
    vkDesc.WithUsage(rgDesc.usage)
          .WithFlags(rgDesc.flags)
          .WithTiling(rgDesc.tiling)
          .WithMipLevels(rgDesc.mip_levels)
          .WithArrayElements(rgDesc.array_elements);
    return vkDesc;
}

// Convert render_graph::MemoryLocation to backend::MemoryLocation
inline tekki::MemoryLocation ConvertMemoryLocation(MemoryLocation loc) {
    switch (loc) {
        case MemoryLocation::GpuOnly: return tekki::MemoryLocation::GpuOnly;
        case MemoryLocation::CpuToGpu: return tekki::MemoryLocation::CpuToGpu;
        case MemoryLocation::GpuToCpu: return tekki::MemoryLocation::GpuToCpu;
        case MemoryLocation::Unknown: return tekki::MemoryLocation::Unknown;
        default: return tekki::MemoryLocation::GpuOnly;
    }
}

// Convert render_graph::BufferDesc to backend::vulkan::BufferDesc
inline tekki::backend::vulkan::BufferDesc ConvertBufferDesc(const BufferDesc& rgDesc) {
    tekki::backend::vulkan::BufferDesc vkDesc;
    vkDesc.Size = rgDesc.size;
    vkDesc.Usage = rgDesc.usage;
    vkDesc.MemoryLocation = ConvertMemoryLocation(rgDesc.memory_location);
    if (rgDesc.alignment.has_value()) {
        vkDesc.Alignment = rgDesc.alignment.value();
    }
    return vkDesc;
}

} // namespace tekki::render_graph
