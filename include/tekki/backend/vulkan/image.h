// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/image.rs

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace tekki::backend::vulkan
{

// ImageType enumeration matching Rust
enum class ImageType : uint8_t
{
    Tex1d = 0,
    Tex1dArray = 1,
    Tex2d = 2,
    Tex2dArray = 3,
    Tex3d = 4,
    Cube = 5,
    CubeArray = 6,
};

// ImageDesc with builder pattern methods
struct ImageDesc
{
    ImageType image_type;
    vk::ImageUsageFlags usage;
    vk::ImageCreateFlags flags;
    vk::Format format;
    std::array<uint32_t, 3> extent;
    vk::ImageTiling tiling;
    uint16_t mip_levels;
    uint32_t array_elements;

    // Constructors
    ImageDesc() = default;

    static ImageDesc New(vk::Format format, ImageType image_type, std::array<uint32_t, 3> extent)
    {
        ImageDesc desc;
        desc.image_type = image_type;
        desc.usage = vk::ImageUsageFlags{};
        desc.flags = vk::ImageCreateFlags{};
        desc.format = format;
        desc.extent = extent;
        desc.tiling = vk::ImageTiling::eOptimal;
        desc.mip_levels = 1;
        desc.array_elements = 1;
        return desc;
    }

    static ImageDesc New1d(vk::Format format, uint32_t extent)
    {
        return New(format, ImageType::Tex1d, {extent, 1, 1});
    }

    static ImageDesc New2d(vk::Format format, std::array<uint32_t, 2> extent)
    {
        return New(format, ImageType::Tex2d, {extent[0], extent[1], 1});
    }

    static ImageDesc New3d(vk::Format format, std::array<uint32_t, 3> extent)
    {
        return New(format, ImageType::Tex3d, extent);
    }

    static ImageDesc NewCube(vk::Format format, uint32_t width)
    {
        ImageDesc desc;
        desc.image_type = ImageType::Cube;
        desc.usage = vk::ImageUsageFlags{};
        desc.flags = vk::ImageCreateFlagBits::eCubeCompatible;
        desc.format = format;
        desc.extent = {width, width, 1};
        desc.tiling = vk::ImageTiling::eOptimal;
        desc.mip_levels = 1;
        desc.array_elements = 6;
        return desc;
    }

    // Builder pattern methods (fluent interface)
    ImageDesc& SetImageType(ImageType type)
    {
        image_type = type;
        return *this;
    }

    ImageDesc& SetUsage(vk::ImageUsageFlags usage_flags)
    {
        usage = usage_flags;
        return *this;
    }

    ImageDesc& SetFlags(vk::ImageCreateFlags create_flags)
    {
        flags = create_flags;
        return *this;
    }

    ImageDesc& SetFormat(vk::Format fmt)
    {
        format = fmt;
        return *this;
    }

    ImageDesc& SetExtent(std::array<uint32_t, 3> ext)
    {
        extent = ext;
        return *this;
    }

    ImageDesc& SetTiling(vk::ImageTiling til)
    {
        tiling = til;
        return *this;
    }

    ImageDesc& SetMipLevels(uint16_t levels)
    {
        mip_levels = levels;
        return *this;
    }

    ImageDesc& SetAllMipLevels()
    {
        auto mip_count = [](uint32_t e) -> uint16_t { return static_cast<uint16_t>(32 - __builtin_clz(e)); };

        mip_levels = std::max({mip_count(extent[0]), mip_count(extent[1]), mip_count(extent[2])});
        return *this;
    }

    ImageDesc& SetArrayElements(uint32_t elements)
    {
        array_elements = elements;
        return *this;
    }

    ImageDesc& DivUpExtent(std::array<uint32_t, 3> div_extent)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            extent[i] = std::max(1u, (extent[i] + div_extent[i] - 1) / div_extent[i]);
        }
        return *this;
    }

    ImageDesc& DivExtent(std::array<uint32_t, 3> div_extent)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            extent[i] = std::max(1u, extent[i] / div_extent[i]);
        }
        return *this;
    }

    ImageDesc& HalfRes()
    {
        return DivUpExtent({2, 2, 2});
    }

    // Utility methods
    std::array<float, 4> ExtentInvExtent2d() const
    {
        return {
            static_cast<float>(extent[0]),
            static_cast<float>(extent[1]),
            1.0f / static_cast<float>(extent[0]),
            1.0f / static_cast<float>(extent[1]),
        };
    }

    std::array<uint32_t, 2> Extent2d() const { return {extent[0], extent[1]}; }
};

// Image subresource data for upload
struct ImageSubResourceData
{
    const uint8_t* data;
    size_t row_pitch;
    size_t slice_pitch;
};

// ImageViewDesc with builder pattern
struct ImageViewDesc
{
    std::optional<vk::ImageViewType> view_type;
    std::optional<vk::Format> format;
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    uint32_t base_mip_level = 0;
    std::optional<uint32_t> level_count;

    // Equality and hash for caching
    bool operator==(const ImageViewDesc& other) const
    {
        return view_type == other.view_type && format == other.format && aspect_mask == other.aspect_mask &&
               base_mip_level == other.base_mip_level && level_count == other.level_count;
    }
};

// Hash specialization for ImageViewDesc
struct ImageViewDescHash
{
    size_t operator()(const ImageViewDesc& desc) const
    {
        size_t h = 0;
        if (desc.view_type)
            h ^= std::hash<int>{}(static_cast<int>(*desc.view_type));
        if (desc.format)
            h ^= std::hash<int>{}(static_cast<int>(*desc.format)) << 1;
        h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(desc.aspect_mask)) << 2;
        h ^= std::hash<uint32_t>{}(desc.base_mip_level) << 3;
        if (desc.level_count)
            h ^= std::hash<uint32_t>{}(*desc.level_count) << 4;
        return h;
    }
};

class Device;

// Image class with view caching
class Image
{
  public:
    Image(vk::Image image, const ImageDesc& desc, VmaAllocation allocation, VmaAllocator allocator_handle);
    ~Image() = default;

    // Getters
    vk::Image raw() const { return image_; }
    const ImageDesc& desc() const { return desc_; }
    VmaAllocation allocation() const { return allocation_; }

    // View creation with caching
    vk::ImageView GetView(Device* device, const ImageViewDesc& desc);

    // Generate view desc
    vk::ImageViewCreateInfo GetViewDesc(const ImageViewDesc& desc) const;

  private:
    friend class Device;

    static vk::ImageViewCreateInfo GetViewDescImpl(const ImageViewDesc& desc, const ImageDesc& image_desc);

    vk::Image image_;
    ImageDesc desc_;
    VmaAllocation allocation_;
    VmaAllocator allocator_handle_;

    // View cache (thread-safe)
    mutable std::mutex views_mutex_;
    std::unordered_map<ImageViewDesc, vk::ImageView, ImageViewDescHash> views_;
};

// Utility functions
vk::ImageViewType ConvertImageTypeToViewType(ImageType image_type);
vk::ImageCreateInfo GetImageCreateInfo(const ImageDesc& desc, bool initial_data);

} // namespace tekki::backend::vulkan
