// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/image.h"
#include "tekki/core/log.h"

#include <fmt/format.h>

// For image loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// For DDS loading
#include <cstring>
#include <fstream>

namespace tekki::asset {

// ImageSource implementation
ImageSource ImageSource::FromFile(const std::filesystem::path& path) {
    ImageSource src;
    src.source = path;
    return src;
}

ImageSource ImageSource::FromMemory(std::vector<uint8_t> data) {
    ImageSource src;
    src.source = std::move(data);
    return src;
}

// ImageLoader implementation
Result<RawImage> ImageLoader::Load(const ImageSource& source) {
    if (source.IsFile()) {
        return LoadFromFile(source.GetFilePath());
    } else {
        return LoadFromMemory(source.GetMemoryData());
    }
}

RawImage ImageLoader::CreatePlaceholder(const std::array<uint8_t, 4>& values) {
    RawRgba8Image img;
    img.data = std::vector<uint8_t>(values.begin(), values.end());
    img.width = 1;
    img.height = 1;

    RawImage raw;
    raw.data = std::move(img);
    return raw;
}

Result<RawImage> ImageLoader::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return Err<RawImage>(fmt::format("Failed to open file: {}", path.string()));
    }

    size_t fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    return LoadFromMemory(buffer);
}

Result<RawImage> ImageLoader::LoadFromMemory(const std::vector<uint8_t>& data) {
    // Try loading as DDS first
    auto ddsResult = LoadDds(data);
    if (ddsResult) {
        RawImage raw;
        raw.data = std::move(*ddsResult);
        return Ok(std::move(raw));
    }

    // Fall back to standard formats (PNG, JPEG, etc.)
    return LoadStandard(data);
}

// Simple DDS header structures
#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
};

struct DDS_HEADER {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

struct DDS_HEADER_DXT10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

Result<DdsImage> ImageLoader::LoadDds(const std::vector<uint8_t>& data) {
    if (data.size() < 4 + sizeof(DDS_HEADER)) {
        return Err<DdsImage>("Data too small for DDS");
    }

    // Check magic number
    if (data[0] != 'D' || data[1] != 'D' || data[2] != 'S' || data[3] != ' ') {
        return Err<DdsImage>("Not a DDS file");
    }

    const DDS_HEADER* header = reinterpret_cast<const DDS_HEADER*>(data.data() + 4);

    DdsImage dds;
    dds.width = header->width;
    dds.height = header->height;
    dds.depth = (header->depth > 0) ? header->depth : 1;
    dds.mipLevels = (header->mipMapCount > 0) ? header->mipMapCount : 1;

    // Determine format
    bool hasDX10Header = false;
    if (header->ddspf.fourCC == 0x30315844) { // 'DX10'
        hasDX10Header = true;
        const DDS_HEADER_DXT10* dx10 = reinterpret_cast<const DDS_HEADER_DXT10*>(
            data.data() + 4 + sizeof(DDS_HEADER)
        );

        // Map DXGI format to Vulkan format
        switch (dx10->dxgiFormat) {
            case 71: dds.format = VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;  // DXGI_FORMAT_BC1_UNORM_SRGB
            case 77: dds.format = VK_FORMAT_BC3_UNORM_BLOCK; break;     // DXGI_FORMAT_BC3_UNORM
            case 78: dds.format = VK_FORMAT_BC3_SRGB_BLOCK; break;      // DXGI_FORMAT_BC3_UNORM_SRGB
            case 83: dds.format = VK_FORMAT_BC5_UNORM_BLOCK; break;     // DXGI_FORMAT_BC5_UNORM
            case 84: dds.format = VK_FORMAT_BC5_SNORM_BLOCK; break;     // DXGI_FORMAT_BC5_SNORM
            case 98: dds.format = VK_FORMAT_BC7_UNORM_BLOCK; break;     // DXGI_FORMAT_BC7_UNORM
            case 99: dds.format = VK_FORMAT_BC7_SRGB_BLOCK; break;      // DXGI_FORMAT_BC7_UNORM_SRGB
            default:
                return Err<DdsImage>(fmt::format("Unsupported DXGI format: {}", dx10->dxgiFormat));
        }
    } else {
        // Legacy DDS format
        return Err<DdsImage>("Legacy DDS formats not supported yet");
    }

    // Copy image data
    size_t headerSize = 4 + sizeof(DDS_HEADER) + (hasDX10Header ? sizeof(DDS_HEADER_DXT10) : 0);
    dds.data.assign(data.begin() + headerSize, data.end());

    // Calculate mip offsets (simplified - assumes tightly packed)
    dds.mipOffsets.resize(dds.mipLevels);
    size_t offset = 0;
    for (uint32_t i = 0; i < dds.mipLevels; ++i) {
        dds.mipOffsets[i] = offset;
        // This is a simplified calculation - real DDS may have padding
        uint32_t mipWidth = std::max(1u, dds.width >> i);
        uint32_t mipHeight = std::max(1u, dds.height >> i);
        uint32_t blockSize = 16; // BC formats use 16 bytes per 4x4 block
        uint32_t blocksX = (mipWidth + 3) / 4;
        uint32_t blocksY = (mipHeight + 3) / 4;
        offset += blocksX * blocksY * blockSize;
    }

    TEKKI_LOG_INFO("Loaded DDS image: {}x{}x{} mips={} format={}",
        dds.width, dds.height, dds.depth, dds.mipLevels, static_cast<int>(dds.format));

    return Ok(std::move(dds));
}

Result<RawRgba8Image> ImageLoader::LoadStandard(const std::vector<uint8_t>& data) {
    int width, height, channels;
    uint8_t* pixels = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width, &height, &channels, 4  // Force RGBA
    );

    if (!pixels) {
        return Err<RawRgba8Image>(fmt::format("Failed to load image: {}", stbi_failure_reason()));
    }

    RawRgba8Image img;
    img.width = static_cast<uint32_t>(width);
    img.height = static_cast<uint32_t>(height);
    img.data.assign(pixels, pixels + (width * height * 4));

    stbi_image_free(pixels);

    TEKKI_LOG_INFO("Loaded image: {}x{} channels={}", width, height, channels);

    return Ok(std::move(img));
}

} // namespace tekki::asset
