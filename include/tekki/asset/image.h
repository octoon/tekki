// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-asset/src/image.rs

#pragma once

#include "tekki/core/common.h"
#include "tekki/backend/image.h"
#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <filesystem>
#include <array>

namespace tekki::asset {

// Forward declarations
struct TexParams;
struct RawImage;
struct GpuImageProto;

/**
 * @brief Image source - either from file or memory
 */
struct ImageSource {
    std::variant<std::filesystem::path, std::vector<uint8_t>> source;

    static ImageSource FromFile(const std::filesystem::path& path);
    static ImageSource FromMemory(std::vector<uint8_t> data);

    bool IsFile() const { return std::holds_alternative<std::filesystem::path>(source); }
    bool IsMemory() const { return std::holds_alternative<std::vector<uint8_t>>(source); }

    const std::filesystem::path& GetFilePath() const { return std::get<std::filesystem::path>(source); }
    const std::vector<uint8_t>& GetMemoryData() const { return std::get<std::vector<uint8_t>>(source); }
};

/**
 * @brief Raw RGBA8 image data
 */
struct RawRgba8Image {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
};

/**
 * @brief DDS image data
 */
struct DdsImage {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipLevels;
    VkFormat format;
    std::vector<size_t> mipOffsets;  // Byte offsets for each mip level
};

/**
 * @brief Raw image - either RGBA8 or DDS
 */
struct RawImage {
    std::variant<RawRgba8Image, DdsImage> data;

    bool IsRgba8() const { return std::holds_alternative<RawRgba8Image>(data); }
    bool IsDds() const { return std::holds_alternative<DdsImage>(data); }

    const RawRgba8Image& GetRgba8() const { return std::get<RawRgba8Image>(data); }
    const DdsImage& GetDds() const { return std::get<DdsImage>(data); }
};

/**
 * @brief Texture gamma/color space
 */
enum class TexGamma {
    Linear,
    Srgb
};

/**
 * @brief Texture compression mode
 */
enum class TexCompressionMode {
    None,
    Rgba,  // BC7
    Rg     // BC5
};

inline bool SupportsAlpha(TexCompressionMode mode) {
    return mode == TexCompressionMode::None || mode == TexCompressionMode::Rgba;
}

/**
 * @brief Texture parameters for loading/processing
 */
struct TexParams {
    TexGamma gamma = TexGamma::Linear;
    bool useMips = false;
    TexCompressionMode compression = TexCompressionMode::None;
    std::optional<std::array<size_t, 4>> channelSwizzle = std::nullopt;  // RGBA channel mapping
};

/**
 * @brief GPU image prototype (before upload to GPU)
 */
struct GpuImageProto {
    VkFormat format;
    std::array<uint32_t, 3> extent;
    std::vector<std::vector<uint8_t>> mips;
};

/**
 * @brief Image loader - loads image from source
 */
class ImageLoader {
public:
    ImageLoader() = default;

    // Load image from source
    static Result<RawImage> Load(const ImageSource& source);

    // Create placeholder image
    static RawImage CreatePlaceholder(const std::array<uint8_t, 4>& values);

private:
    static Result<RawImage> LoadFromFile(const std::filesystem::path& path);
    static Result<RawImage> LoadFromMemory(const std::vector<uint8_t>& data);
    static Result<DdsImage> LoadDds(const std::vector<uint8_t>& data);
    static Result<RawRgba8Image> LoadStandard(const std::vector<uint8_t>& data);
};

/**
 * @brief GPU image creator - processes raw image into GPU-ready format
 */
class GpuImageCreator {
public:
    GpuImageCreator(const TexParams& params) : params_(params) {}

    Result<GpuImageProto> Create(const RawImage& rawImage);

private:
    Result<GpuImageProto> ProcessRgba8(const RawRgba8Image& src);
    Result<GpuImageProto> ProcessDds(const DdsImage& src);

    std::vector<uint8_t> CompressBC5(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);
    std::vector<uint8_t> CompressBC7(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, bool hasAlpha);

    void ApplyChannelSwizzle(std::vector<uint8_t>& rgba);

    TexParams params_;
};

} // namespace tekki::asset
