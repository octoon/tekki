#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <tekki/gpu_allocator/vulkan/allocator.h>

namespace tekki::backend::vulkan {

class Device;

enum class ImageType {
    Tex1d = 0,
    Tex1dArray = 1,
    Tex2d = 2,
    Tex2dArray = 3,
    Tex3d = 4,
    Cube = 5,
    CubeArray = 6
};

struct ImageDesc {
    ImageType Type;
    VkImageUsageFlags Usage;
    VkImageCreateFlags Flags;
    VkFormat Format;
    glm::u32vec3 Extent;
    VkImageTiling Tiling;
    uint16_t MipLevels;
    uint32_t ArrayElements;

    ImageDesc(VkFormat format, ImageType imageType, const glm::u32vec3& extent);
    static ImageDesc New1d(VkFormat format, uint32_t extent);
    static ImageDesc New2d(VkFormat format, const glm::u32vec2& extent);
    static ImageDesc New3d(VkFormat format, const glm::u32vec3& extent);
    static ImageDesc NewCube(VkFormat format, uint32_t width);

    ImageDesc& WithImageType(ImageType imageType);
    ImageDesc& WithUsage(VkImageUsageFlags usage);
    ImageDesc& WithFlags(VkImageCreateFlags flags);
    ImageDesc& WithFormat(VkFormat format);
    ImageDesc& WithExtent(const glm::u32vec3& extent);
    ImageDesc& WithTiling(VkImageTiling tiling);
    ImageDesc& WithMipLevels(uint16_t mipLevels);
    ImageDesc& WithAllMipLevels();
    ImageDesc& WithArrayElements(uint32_t arrayElements);
    ImageDesc& WithDivUpExtent(const glm::u32vec3& divExtent);
    ImageDesc& WithDivExtent(const glm::u32vec3& divExtent);
    ImageDesc& WithHalfRes();
    glm::vec4 GetExtentInvExtent2d() const;
    glm::u32vec2 GetExtent2d() const;

    bool operator==(const ImageDesc& other) const {
        return Type == other.Type &&
               Usage == other.Usage &&
               Flags == other.Flags &&
               Format == other.Format &&
               Extent == other.Extent &&
               Tiling == other.Tiling &&
               MipLevels == other.MipLevels &&
               ArrayElements == other.ArrayElements;
    }
};

struct ImageSubResourceData {
    const std::vector<uint8_t>& Data;
    size_t RowPitch;
    size_t SlicePitch;
};

class Image {
public:
    VkImage Raw;
    ImageDesc Desc;
    
    Image(VkImage raw, const ImageDesc& desc);
    ~Image();
    
    VkImageView GetView(Device& device, const struct ImageViewDesc& desc);
    VkImageViewCreateInfo GetViewDesc(const struct ImageViewDesc& desc) const;

private:
    std::unordered_map<struct ImageViewDesc, VkImageView> views;
    mutable std::mutex viewsMutex;
    
    static VkImageViewCreateInfo CreateViewDescImpl(const struct ImageViewDesc& desc, const ImageDesc& imageDesc);
};

struct ImageViewDesc {
    std::optional<VkImageViewType> ViewType;
    std::optional<VkFormat> Format;
    VkImageAspectFlags AspectMask;
    uint32_t BaseMipLevel;
    std::optional<uint32_t> LevelCount;

    ImageViewDesc();
    ImageViewDesc(const ImageViewDesc& other);
    ImageViewDesc& operator=(const ImageViewDesc& other);

    bool operator==(const ImageViewDesc& other) const;

    class Builder;
    static Builder CreateBuilder();
};

class ImageViewDesc::Builder {
public:
    Builder();
    Builder& WithViewType(std::optional<VkImageViewType> viewType);
    Builder& WithFormat(std::optional<VkFormat> format);
    Builder& WithAspectMask(VkImageAspectFlags aspectMask);
    Builder& WithBaseMipLevel(uint32_t baseMipLevel);
    Builder& WithLevelCount(std::optional<uint32_t> levelCount);
    ImageViewDesc Build() const;

private:
    ImageViewDesc desc;
};

VkImageViewType ConvertImageTypeToViewType(ImageType imageType);
VkImageCreateInfo GetImageCreateInfo(const ImageDesc& desc, bool initialData);

} // namespace tekki::backend::vulkan

namespace std {
    template<>
    struct hash<tekki::backend::vulkan::ImageDesc> {
        size_t operator()(const tekki::backend::vulkan::ImageDesc& desc) const;
    };

    template<>
    struct hash<tekki::backend::vulkan::ImageViewDesc> {
        size_t operator()(const tekki::backend::vulkan::ImageViewDesc& desc) const;
    };
}