#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/asset/TexParams.h"
#include "tekki/asset/GpuImage.h"

namespace tekki::asset {

enum class ImageSourceType {
    File,
    Memory
};

class ImageSource {
public:
    ImageSource(const std::filesystem::path& path);
    ImageSource(const std::vector<uint8_t>& data);
    
    ImageSourceType GetType() const;
    std::filesystem::path GetPath() const;
    const std::vector<uint8_t>& GetData() const;

private:
    ImageSourceType type_;
    std::filesystem::path path_;
    std::vector<uint8_t> data_;
};

struct RawRgba8Image {
    std::vector<uint8_t> data;
    glm::u32vec2 dimensions;
};

enum class RawImageType {
    Rgba8,
    Dds
};

class RawImage {
public:
    RawImage(const RawRgba8Image& rgba8Image);
    RawImage(void* ddsData); // Placeholder for DDS data
    
    RawImageType GetType() const;
    const RawRgba8Image& GetRgba8Image() const;
    void* GetDdsData() const;

private:
    RawImageType type_;
    RawRgba8Image rgba8Image_;
    void* ddsData_;
};

enum class LoadImageType {
    Lazy,
    Immediate
};

class LoadImage {
public:
    static LoadImage FromPath(const std::filesystem::path& path);
    static LoadImage FromMemory(const std::vector<uint8_t>& data);
    
    LoadImageType GetType() const;
    std::vector<uint8_t> Evaluate();

private:
    LoadImageType type_;
    std::filesystem::path lazyPath_;
    std::vector<uint8_t> immediateData_;
};

class CreatePlaceholderImage {
public:
    explicit CreatePlaceholderImage(const std::array<uint8_t, 4>& values);
    RawImage Create();

private:
    std::array<uint8_t, 4> values_;
};

enum class BcMode {
    Bc5,
    Bc7
};

class CreateGpuImage {
public:
    CreateGpuImage(const std::shared_ptr<RawImage>& image, const tekki::asset::TexParams& params);

    tekki::asset::GpuImage::Proto Create();

private:
    tekki::asset::GpuImage::Proto ProcessRgba8(const RawRgba8Image& src);
    tekki::asset::GpuImage::Proto ProcessDds(void* ddsData);

    std::vector<uint8_t> CompressMip(const std::vector<uint8_t>& mipData, uint32_t width, uint32_t height, BcMode bcMode, bool needsAlpha);
    void SwizzleChannels(std::vector<uint8_t>& mipData, const std::array<uint8_t, 4>& swizzle);
    std::vector<uint8_t> ProcessMip(const std::vector<uint8_t>& mipData, uint32_t width, uint32_t height, bool shouldCompress, BcMode bcMode, const std::array<uint8_t, 4>& swizzle);
    uint32_t RoundUpToBlock(uint32_t value, uint32_t minImgDim);

    std::shared_ptr<RawImage> image_;
    tekki::asset::TexParams params_;
};

namespace dds_util {
    uint32_t GetTextureSize(uint32_t pitch, uint32_t pitchHeight, uint32_t height, uint32_t depth);
    uint32_t GetPitch(void* dds, uint32_t width);
}

} // namespace tekki::asset