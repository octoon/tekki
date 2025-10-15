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
class Image;  // Forward declaration

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
    using Resource = Image;  // Type alias for Handle<Image> template

    ImageType Type;
    VkImageUsageFlags Usage;
    VkImageCreateFlags Flags;
    VkFormat Format;
    glm::u32vec3 Extent;
    VkImageTiling Tiling;
    uint16_t MipLevels;
    uint32_t ArrayElements;

    // Default constructor - creates a 1x1 R8G8B8A8_UNORM texture
    ImageDesc()
        : Type(ImageType::Tex2d)
        , Usage(VK_IMAGE_USAGE_SAMPLED_BIT)
        , Flags(0)
        , Format(VK_FORMAT_R8G8B8A8_UNORM)
        , Extent(1, 1, 1)
        , Tiling(VK_IMAGE_TILING_OPTIMAL)
        , MipLevels(1)
        , ArrayElements(1)
    {}

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

    struct Hash {
        std::size_t operator()(const ImageDesc& desc) const noexcept {
            std::size_t h1 = std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Type));
            std::size_t h2 = std::hash<uint32_t>{}(desc.Usage);
            std::size_t h3 = std::hash<uint32_t>{}(desc.Flags);
            std::size_t h4 = std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Format));
            std::size_t h5 = std::hash<uint32_t>{}(desc.Extent.x) ^ (std::hash<uint32_t>{}(desc.Extent.y) << 1) ^ (std::hash<uint32_t>{}(desc.Extent.z) << 2);
            std::size_t h6 = std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Tiling));
            std::size_t h7 = std::hash<uint16_t>{}(desc.MipLevels);
            std::size_t h8 = std::hash<uint32_t>{}(desc.ArrayElements);

            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
        }
    };
};

struct ImageSubResourceData {
    const std::vector<uint8_t>& Data;
    size_t RowPitch;
    size_t SlicePitch;
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

    struct Hash {
        std::size_t operator()(const ImageViewDesc& desc) const noexcept {
            std::size_t h1 = desc.ViewType.has_value() ? std::hash<uint32_t>{}(static_cast<uint32_t>(desc.ViewType.value())) : 0;
            std::size_t h2 = desc.Format.has_value() ? std::hash<uint32_t>{}(static_cast<uint32_t>(desc.Format.value())) : 0;
            std::size_t h3 = std::hash<uint32_t>{}(desc.AspectMask);
            std::size_t h4 = std::hash<uint32_t>{}(desc.BaseMipLevel);
            std::size_t h5 = desc.LevelCount.has_value() ? std::hash<uint32_t>{}(desc.LevelCount.value()) : 0;

            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };

    class Builder;
    static Builder CreateBuilder();
};

class Image {
public:
    using Desc = ImageDesc;  // Type alias for Handle<Image> template

    VkImage Raw;
    VkImage raw;  // Alias for compatibility with render_graph
    ImageDesc desc;

    Image(VkImage raw, const ImageDesc& desc);
    ~Image();

    VkImageView GetView(Device& device, const ImageViewDesc& desc);
    VkImageViewCreateInfo GetViewDesc(const ImageViewDesc& desc) const;

    // Accessor for desc (matching Rust API)
    const ImageDesc& GetDesc() const { return desc; }

private:
    std::unordered_map<ImageViewDesc, VkImageView, ImageViewDesc::Hash> views;
    mutable std::mutex viewsMutex;

    static VkImageViewCreateInfo CreateViewDescImpl(const ImageViewDesc& desc, const ImageDesc& imageDesc);
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

} // namespace tekki::backend::vulkan

VkImageViewType ConvertImageTypeToViewType(tekki::backend::vulkan::ImageType imageType);
VkImageCreateInfo GetImageCreateInfo(const tekki::backend::vulkan::ImageDesc& desc, bool initialData);

