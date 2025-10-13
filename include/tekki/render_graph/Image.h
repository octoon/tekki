#pragma once

#include <cstdint>
#include <array>
#include <variant>
#include <memory>
#include <vulkan/vulkan.h>

namespace tekki::render_graph {

// Forward declarations
namespace AnyRenderResource {
    struct OwnedImage;
    struct ImportedImage;
    struct OwnedBuffer;
    struct ImportedBuffer;
    struct ImportedRayTracingAcceleration;
}
struct PendingRenderResourceInfo;

// Image type enum matching Rust version
enum class ImageType {
    Tex1d = 0,
    Tex1dArray = 1,
    Tex2d = 2,
    Tex2dArray = 3,
    Tex3d = 4,
    Cube = 5,
    CubeArray = 6,
};

// Image descriptor matching Rust version
struct ImageDesc {
    using Resource = Image;  // Type alias for template resolution

    ImageType image_type = ImageType::Tex2d;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::array<uint32_t, 3> extent = {0, 0, 1};
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    uint16_t mip_levels = 1;
    uint32_t array_elements = 1;

    // Constructors
    static ImageDesc New(VkFormat fmt, ImageType type, std::array<uint32_t, 3> ext) {
        ImageDesc desc;
        desc.image_type = type;
        desc.format = fmt;
        desc.extent = ext;
        return desc;
    }

    static ImageDesc New2d(VkFormat fmt, std::array<uint32_t, 2> ext) {
        return New(fmt, ImageType::Tex2d, {ext[0], ext[1], 1});
    }

    static ImageDesc NewCube(VkFormat fmt, uint32_t width) {
        return New(fmt, ImageType::Cube, {width, width, 1}).WithArrayElements(6);
    }

    // Builder methods
    ImageDesc WithUsage(VkImageUsageFlags u) const {
        ImageDesc result = *this;
        result.usage = u;
        return result;
    }

    ImageDesc WithArrayElements(uint32_t elements) const {
        ImageDesc result = *this;
        result.array_elements = elements;
        return result;
    }
};

// Image resource
class Image {
public:
    using Desc = ImageDesc;

    VkImage raw = VK_NULL_HANDLE;
    VkImage Raw = VK_NULL_HANDLE;  // Alias for compatibility
    ImageDesc desc;

    Image() = default;
    explicit Image(const ImageDesc& d) : desc(d) {}

    const ImageDesc& GetDesc() const { return desc; }

    // Helper to borrow resource from variant
    template<typename VariantType>
    static const Image* BorrowResource(const VariantType& variant) {
        if (std::holds_alternative<AnyRenderResource::OwnedImage>(variant)) {
            return std::get<AnyRenderResource::OwnedImage>(variant).resource.get();
        } else if (std::holds_alternative<AnyRenderResource::ImportedImage>(variant)) {
            return std::get<AnyRenderResource::ImportedImage>(variant).resource.get();
        }
        return nullptr;
    }
};

} // namespace tekki::render_graph
