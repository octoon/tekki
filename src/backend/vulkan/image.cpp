#include "tekki/backend/vulkan/image.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/barrier.h"

namespace tekki::backend::vulkan {

namespace {
    uint16_t mip_count_1d(uint32_t extent) {
        return static_cast<uint16_t>(32 - std::countl_zero(extent));
    }
}

ImageDesc::ImageDesc(VkFormat format, ImageType imageType, const glm::u32vec3& extent)
    : Type(imageType)
    , Usage(VkImageUsageFlags(0))
    , Flags(VkImageCreateFlags(0))
    , Format(format)
    , Extent(extent)
    , Tiling(VK_IMAGE_TILING_OPTIMAL)
    , MipLevels(1)
    , ArrayElements(1) {
}

ImageDesc ImageDesc::New1d(VkFormat format, uint32_t extent) {
    return ImageDesc(format, ImageType::Tex1d, glm::u32vec3(extent, 1, 1));
}

ImageDesc ImageDesc::New2d(VkFormat format, const glm::u32vec2& extent) {
    return ImageDesc(format, ImageType::Tex2d, glm::u32vec3(extent, 1));
}

ImageDesc ImageDesc::New3d(VkFormat format, const glm::u32vec3& extent) {
    return ImageDesc(format, ImageType::Tex3d, extent);
}

ImageDesc ImageDesc::NewCube(VkFormat format, uint32_t width) {
    ImageDesc desc(format, ImageType::Cube, glm::u32vec3(width, width, 1));
    desc.Flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    desc.ArrayElements = 6;
    return desc;
}

ImageDesc& ImageDesc::WithImageType(ImageType imageType) {
    Type = imageType;
    return *this;
}

ImageDesc& ImageDesc::WithUsage(VkImageUsageFlags usage) {
    Usage = usage;
    return *this;
}

ImageDesc& ImageDesc::WithFlags(VkImageCreateFlags flags) {
    Flags = flags;
    return *this;
}

ImageDesc& ImageDesc::WithFormat(VkFormat format) {
    Format = format;
    return *this;
}

ImageDesc& ImageDesc::WithExtent(const glm::u32vec3& extent) {
    Extent = extent;
    return *this;
}

ImageDesc& ImageDesc::WithTiling(VkImageTiling tiling) {
    Tiling = tiling;
    return *this;
}

ImageDesc& ImageDesc::WithMipLevels(uint16_t mipLevels) {
    MipLevels = mipLevels;
    return *this;
}

ImageDesc& ImageDesc::WithAllMipLevels() {
    MipLevels = std::max(mip_count_1d(Extent.x), std::max(mip_count_1d(Extent.y), mip_count_1d(Extent.z)));
    return *this;
}

ImageDesc& ImageDesc::WithArrayElements(uint32_t arrayElements) {
    ArrayElements = arrayElements;
    return *this;
}

ImageDesc& ImageDesc::WithDivUpExtent(const glm::u32vec3& divExtent) {
    Extent.x = std::max((Extent.x + divExtent.x - 1) / divExtent.x, 1u);
    Extent.y = std::max((Extent.y + divExtent.y - 1) / divExtent.y, 1u);
    Extent.z = std::max((Extent.z + divExtent.z - 1) / divExtent.z, 1u);
    return *this;
}

ImageDesc& ImageDesc::WithDivExtent(const glm::u32vec3& divExtent) {
    Extent.x = std::max(Extent.x / divExtent.x, 1u);
    Extent.y = std::max(Extent.y / divExtent.y, 1u);
    Extent.z = std::max(Extent.z / divExtent.z, 1u);
    return *this;
}

ImageDesc& ImageDesc::WithHalfRes() {
    return WithDivUpExtent(glm::u32vec3(2, 2, 2));
}

glm::vec4 ImageDesc::GetExtentInvExtent2d() const {
    return glm::vec4(
        static_cast<float>(Extent.x),
        static_cast<float>(Extent.y),
        1.0f / static_cast<float>(Extent.x),
        1.0f / static_cast<float>(Extent.y)
    );
}

glm::u32vec2 ImageDesc::GetExtent2d() const {
    return glm::u32vec2(Extent.x, Extent.y);
}

Image::Image(VkImage raw, const ImageDesc& desc) 
    : Raw(raw)
    , Desc(desc) {
}

Image::~Image() {
    std::lock_guard<std::mutex> lock(viewsMutex);
    for (auto& [_, view] : views) {
        // Views should be destroyed by the device
    }
}

VkImageView Image::GetView(Device& device, const ImageViewDesc& desc) {
    std::lock_guard<std::mutex> lock(viewsMutex);
    
    auto it = views.find(desc);
    if (it != views.end()) {
        return it->second;
    }
    
    try {
        VkImageViewCreateInfo createInfo = GetViewDesc(desc);
        createInfo.image = Raw;
        
        VkImageView view;
        VkResult result = vkCreateImageView(device.GetRaw(), &createInfo, nullptr, &view);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
        
        views[desc] = view;
        return view;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create image view: ") + e.what());
    }
}

VkImageViewCreateInfo Image::GetViewDesc(const ImageViewDesc& desc) const {
    return CreateViewDescImpl(desc, Desc);
}

VkImageViewCreateInfo Image::CreateViewDescImpl(const ImageViewDesc& desc, const ImageDesc& imageDesc) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.format = desc.Format.value_or(imageDesc.Format);
    createInfo.components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A
    };
    createInfo.viewType = desc.ViewType.value_or(ConvertImageTypeToViewType(imageDesc.Type));

    createInfo.subresourceRange.aspectMask = desc.AspectMask;
    createInfo.subresourceRange.baseMipLevel = desc.BaseMipLevel;
    createInfo.subresourceRange.levelCount = desc.LevelCount.value_or(imageDesc.MipLevels);
    createInfo.subresourceRange.baseArrayLayer = 0;

    switch (imageDesc.Type) {
        case ImageType::Cube:
        case ImageType::CubeArray:
            createInfo.subresourceRange.layerCount = 6;
            break;
        default:
            createInfo.subresourceRange.layerCount = 1;
            break;
    }
    
    return createInfo;
}

ImageViewDesc::ImageViewDesc()
    : ViewType(std::nullopt)
    , Format(std::nullopt)
    , AspectMask(VK_IMAGE_ASPECT_COLOR_BIT)
    , BaseMipLevel(0)
    , LevelCount(std::nullopt) {
}

ImageViewDesc::ImageViewDesc(const ImageViewDesc& other)
    : ViewType(other.ViewType)
    , Format(other.Format)
    , AspectMask(other.AspectMask)
    , BaseMipLevel(other.BaseMipLevel)
    , LevelCount(other.LevelCount) {
}

ImageViewDesc& ImageViewDesc::operator=(const ImageViewDesc& other) {
    if (this != &other) {
        ViewType = other.ViewType;
        Format = other.Format;
        AspectMask = other.AspectMask;
        BaseMipLevel = other.BaseMipLevel;
        LevelCount = other.LevelCount;
    }
    return *this;
}

bool ImageViewDesc::operator==(const ImageViewDesc& other) const {
    return ViewType == other.ViewType &&
           Format == other.Format &&
           AspectMask == other.AspectMask &&
           BaseMipLevel == other.BaseMipLevel &&
           LevelCount == other.LevelCount;
}

ImageViewDesc::Builder::Builder() {
    desc = ImageViewDesc();
}

ImageViewDesc::Builder& ImageViewDesc::Builder::WithViewType(std::optional<VkImageViewType> viewType) {
    desc.ViewType = viewType;
    return *this;
}

ImageViewDesc::Builder& ImageViewDesc::Builder::WithFormat(std::optional<VkFormat> format) {
    desc.Format = format;
    return *this;
}

ImageViewDesc::Builder& ImageViewDesc::Builder::WithAspectMask(VkImageAspectFlags aspectMask) {
    desc.AspectMask = aspectMask;
    return *this;
}

ImageViewDesc::Builder& ImageViewDesc::Builder::WithBaseMipLevel(uint32_t baseMipLevel) {
    desc.BaseMipLevel = baseMipLevel;
    return *this;
}

ImageViewDesc::Builder& ImageViewDesc::Builder::WithLevelCount(std::optional<uint32_t> levelCount) {
    desc.LevelCount = levelCount;
    return *this;
}

ImageViewDesc ImageViewDesc::Builder::Build() const {
    return desc;
}

ImageViewDesc::Builder ImageViewDesc::CreateBuilder() {
    return Builder();
}

VkImageViewType ConvertImageTypeToViewType(ImageType imageType) {
    switch (imageType) {
        case ImageType::Tex1d: return VK_IMAGE_VIEW_TYPE_1D;
        case ImageType::Tex1dArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case ImageType::Tex2d: return VK_IMAGE_VIEW_TYPE_2D;
        case ImageType::Tex2dArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ImageType::Tex3d: return VK_IMAGE_VIEW_TYPE_3D;
        case ImageType::Cube: return VK_IMAGE_VIEW_TYPE_CUBE;
        case ImageType::CubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default: return VK_IMAGE_VIEW_TYPE_2D;
    }
}

VkImageCreateInfo GetImageCreateInfo(const ImageDesc& desc, bool initialData) {
    VkImageType imageType;
    VkExtent3D imageExtent{};
    uint32_t imageLayers;

    switch (desc.Type) {
        case ImageType::Tex1d:
            imageType = VK_IMAGE_TYPE_1D;
            imageExtent = { desc.Extent.x, 1, 1 };
            imageLayers = 1;
            break;
        case ImageType::Tex1dArray:
            imageType = VK_IMAGE_TYPE_1D;
            imageExtent = { desc.Extent.x, 1, 1 };
            imageLayers = desc.ArrayElements;
            break;
        case ImageType::Tex2d:
            imageType = VK_IMAGE_TYPE_2D;
            imageExtent = { desc.Extent.x, desc.Extent.y, 1 };
            imageLayers = 1;
            break;
        case ImageType::Tex2dArray:
            imageType = VK_IMAGE_TYPE_2D;
            imageExtent = { desc.Extent.x, desc.Extent.y, 1 };
            imageLayers = desc.ArrayElements;
            break;
        case ImageType::Tex3d:
            imageType = VK_IMAGE_TYPE_3D;
            imageExtent = { desc.Extent.x, desc.Extent.y, desc.Extent.z };
            imageLayers = 1;
            break;
        case ImageType::Cube:
            imageType = VK_IMAGE_TYPE_2D;
            imageExtent = { desc.Extent.x, desc.Extent.y, 1 };
            imageLayers = 6;
            break;
        case ImageType::CubeArray:
            imageType = VK_IMAGE_TYPE_2D;
            imageExtent = { desc.Extent.x, desc.Extent.y, 1 };
            imageLayers = 6 * desc.ArrayElements;
            break;
        default:
            imageType = VK_IMAGE_TYPE_2D;
            imageExtent = { desc.Extent.x, desc.Extent.y, 1 };
            imageLayers = 1;
            break;
    }

    VkImageUsageFlags imageUsage = desc.Usage;
    if (initialData) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = desc.Flags;
    createInfo.imageType = imageType;
    createInfo.format = desc.Format;
    createInfo.extent = imageExtent;
    createInfo.mipLevels = desc.MipLevels;
    createInfo.arrayLayers = imageLayers;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = desc.Tiling;
    createInfo.usage = imageUsage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = initialData ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;

    return createInfo;
}

} // namespace tekki::backend::vulkan