// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/image.h"
#include "tekki/core/log.h"

// For BC compression - we'll use intel_tex_2 library or implement simple version
// For now, placeholder implementations

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

namespace tekki::asset {

namespace {

// Helper to check if image needs compression
bool ShouldCompress(const TexParams& params, uint32_t width, uint32_t height) {
    return params.compression != TexCompressionMode::None
        && width >= 4 && height >= 4;
}

// Helper to round up to multiple
uint32_t RoundUpToBlock(uint32_t x, uint32_t blockSize) {
    return ((x + blockSize - 1) / blockSize) * blockSize;
}

// Resize image
std::vector<uint8_t> ResizeImage(
    const std::vector<uint8_t>& src,
    uint32_t srcW, uint32_t srcH,
    uint32_t dstW, uint32_t dstH
) {
    std::vector<uint8_t> dst(dstW * dstH * 4);
    stbir_resize_uint8(
        src.data(), srcW, srcH, srcW * 4,
        dst.data(), dstW, dstH, dstW * 4,
        4  // RGBA = 4 channels
    );
    return dst;
}

} // anonymous namespace

Result<GpuImageProto> GpuImageCreator::Create(const RawImage& rawImage) {
    if (rawImage.IsRgba8()) {
        return ProcessRgba8(rawImage.GetRgba8());
    } else {
        return ProcessDds(rawImage.GetDds());
    }
}

Result<GpuImageProto> GpuImageCreator::ProcessRgba8(const RawRgba8Image& src) {
    GpuImageProto proto;

    // Start with base format
    proto.format = (params_.gamma == TexGamma::Srgb)
        ? VK_FORMAT_R8G8B8A8_SRGB
        : VK_FORMAT_R8G8B8A8_UNORM;

    std::vector<uint8_t> imageData = src.data;
    uint32_t width = src.width;
    uint32_t height = src.height;

    // Resize if too large
    constexpr uint32_t MAX_SIZE = 2048;
    if (width > MAX_SIZE || height > MAX_SIZE) {
        uint32_t newWidth = std::min(width, MAX_SIZE);
        uint32_t newHeight = std::min(height, MAX_SIZE);
        imageData = ResizeImage(imageData, width, height, newWidth, newHeight);
        width = newWidth;
        height = newHeight;
    }

    bool shouldCompress = ShouldCompress(params_, width, height);
    uint32_t minImgDim = shouldCompress ? 4u : 1u;

    // Round dimensions for compression
    if (shouldCompress && (width % 4 != 0 || height % 4 != 0)) {
        uint32_t newWidth = RoundUpToBlock(width, 4);
        uint32_t newHeight = RoundUpToBlock(height, 4);
        imageData = ResizeImage(imageData, width, height, newWidth, newHeight);
        width = newWidth;
        height = newHeight;
    }

    // Apply channel swizzle if needed
    if (params_.channelSwizzle) {
        ApplyChannelSwizzle(imageData);
    }

    proto.extent = {width, height, 1};

    // Process mips
    auto processMip = [&](std::vector<uint8_t> mipData, uint32_t mipW, uint32_t mipH) -> std::vector<uint8_t> {
        if (shouldCompress) {
            // Check if image has alpha
            bool hasAlpha = false;
            if (SupportsAlpha(params_.compression)) {
                for (size_t i = 3; i < mipData.size(); i += 4) {
                    if (mipData[i] != 255) {
                        hasAlpha = true;
                        break;
                    }
                }
            }

            if (params_.compression == TexCompressionMode::Rg) {
                proto.format = (params_.gamma == TexGamma::Srgb)
                    ? VK_FORMAT_BC5_UNORM_BLOCK  // BC5 doesn't have SRGB variant
                    : VK_FORMAT_BC5_UNORM_BLOCK;
                return CompressBC5(mipData, mipW, mipH);
            } else if (params_.compression == TexCompressionMode::Rgba) {
                proto.format = (params_.gamma == TexGamma::Srgb)
                    ? VK_FORMAT_BC7_SRGB_BLOCK
                    : VK_FORMAT_BC7_UNORM_BLOCK;
                return CompressBC7(mipData, mipW, mipH, hasAlpha);
            }
        }
        return mipData;
    };

    if (params_.useMips) {
        // Generate mipmaps
        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

        uint32_t mipW = width;
        uint32_t mipH = height;
        std::vector<uint8_t> mipData = imageData;

        for (uint32_t i = 0; i < mipLevels; ++i) {
            proto.mips.push_back(processMip(mipData, mipW, mipH));

            // Generate next mip
            uint32_t nextW = std::max(RoundUpToBlock(mipW / 2, minImgDim), minImgDim);
            uint32_t nextH = std::max(RoundUpToBlock(mipH / 2, minImgDim), minImgDim);

            if (nextW == mipW && nextH == mipH) break;

            mipData = ResizeImage(mipData, mipW, mipH, nextW, nextH);
            mipW = nextW;
            mipH = nextH;
        }
    } else {
        proto.mips.push_back(processMip(imageData, width, height));
    }

    return Ok(std::move(proto));
}

Result<GpuImageProto> GpuImageCreator::ProcessDds(const DdsImage& src) {
    GpuImageProto proto;
    proto.format = src.format;
    proto.extent = {src.width, src.height, src.depth};

    // Extract mip levels from DDS data
    for (uint32_t i = 0; i < src.mipLevels; ++i) {
        size_t mipStart = src.mipOffsets[i];
        size_t mipEnd = (i + 1 < src.mipLevels)
            ? src.mipOffsets[i + 1]
            : src.data.size();

        proto.mips.emplace_back(
            src.data.begin() + mipStart,
            src.data.begin() + mipEnd
        );
    }

    return Ok(std::move(proto));
}

void GpuImageCreator::ApplyChannelSwizzle(std::vector<uint8_t>& rgba) {
    if (!params_.channelSwizzle) return;

    const auto& swizzle = *params_.channelSwizzle;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint8_t temp[4] = {rgba[i], rgba[i+1], rgba[i+2], rgba[i+3]};
        rgba[i+0] = temp[swizzle[0]];
        rgba[i+1] = temp[swizzle[1]];
        rgba[i+2] = temp[swizzle[2]];
        rgba[i+3] = temp[swizzle[3]];
    }
}

// Placeholder BC5/BC7 compression implementations
// In real implementation, use intel_tex_2 or similar library

std::vector<uint8_t> GpuImageCreator::CompressBC5(
    [[maybe_unused]] const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height
) {
    TEKKI_LOG_INFO("Compressing to BC5 (placeholder)...");

    // BC5 is 16 bytes per 4x4 block
    uint32_t blocksX = (width + 3) / 4;
    uint32_t blocksY = (height + 3) / 4;
    size_t compressedSize = blocksX * blocksY * 16;

    std::vector<uint8_t> compressed(compressedSize);

    // TODO: Implement actual BC5 compression
    // For now, just fill with placeholder data
    // Real implementation would use intel_tex_2::bc5::compress_blocks_into()

    return compressed;
}

std::vector<uint8_t> GpuImageCreator::CompressBC7(
    [[maybe_unused]] const std::vector<uint8_t>& rgba,
    uint32_t width,
    uint32_t height,
    bool hasAlpha
) {
    TEKKI_LOG_INFO("Compressing to BC7 (hasAlpha={}, placeholder)...", hasAlpha);

    // BC7 is 16 bytes per 4x4 block
    uint32_t blocksX = (width + 3) / 4;
    uint32_t blocksY = (height + 3) / 4;
    size_t compressedSize = blocksX * blocksY * 16;

    std::vector<uint8_t> compressed(compressedSize);

    // TODO: Implement actual BC7 compression
    // For now, just fill with placeholder data
    // Real implementation would use intel_tex_2::bc7::compress_blocks_into()

    return compressed;
}

} // namespace tekki::asset
