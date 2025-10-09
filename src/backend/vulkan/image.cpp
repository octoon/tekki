// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/image.rs

#include "../../include/tekki/backend/vulkan/image.h"
#include <spdlog/spdlog.h>
#include "../../include/tekki/backend/error.h"
#include "../../include/tekki/backend/vulkan/device.h"

namespace tekki::backend::vulkan
{

Image::Image(vk::Image image, const ImageDesc& desc, VmaAllocation allocation, VmaAllocator allocator_handle)
    : image_(image), desc_(desc), allocation_(allocation), allocator_handle_(allocator_handle)
{
}

vk::ImageView Image::GetView(Device* device, const ImageViewDesc& desc)
{
    std::lock_guard<std::mutex> lock(views_mutex_);

    // Check if view exists in cache
    auto it = views_.find(desc);
    if (it != views_.end())
    {
        return it->second;
    }

    // Create new view
    vk::ImageView view = device->CreateImageView(desc, desc_, image_);
    views_[desc] = view;

    return view;
}

vk::ImageViewCreateInfo Image::GetViewDesc(const ImageViewDesc& desc) const
{
    return GetViewDescImpl(desc, desc_);
}

vk::ImageViewCreateInfo Image::GetViewDescImpl(const ImageViewDesc& desc, const ImageDesc& image_desc)
{
    vk::ImageViewCreateInfo view_info{};
    view_info.format = desc.format.value_or(image_desc.format);
    view_info.components = vk::ComponentMapping{
        .r = vk::ComponentSwizzle::eR,
        .g = vk::ComponentSwizzle::eG,
        .b = vk::ComponentSwizzle::eB,
        .a = vk::ComponentSwizzle::eA,
    };
    view_info.viewType = desc.view_type.value_or(ConvertImageTypeToViewType(image_desc.image_type));

    // Subresource range
    view_info.subresourceRange.aspectMask = desc.aspect_mask;
    view_info.subresourceRange.baseMipLevel = desc.base_mip_level;
    view_info.subresourceRange.levelCount = desc.level_count.value_or(image_desc.mip_levels);
    view_info.subresourceRange.baseArrayLayer = 0;

    // Set layer count based on image type
    switch (image_desc.image_type)
    {
    case ImageType::Cube:
    case ImageType::CubeArray:
        view_info.subresourceRange.layerCount = 6;
        break;
    default:
        view_info.subresourceRange.layerCount = 1;
        break;
    }

    return view_info;
}

// Utility functions

vk::ImageViewType ConvertImageTypeToViewType(ImageType image_type)
{
    switch (image_type)
    {
    case ImageType::Tex1d:
        return vk::ImageViewType::e1D;
    case ImageType::Tex1dArray:
        return vk::ImageViewType::e1DArray;
    case ImageType::Tex2d:
        return vk::ImageViewType::e2D;
    case ImageType::Tex2dArray:
        return vk::ImageViewType::e2DArray;
    case ImageType::Tex3d:
        return vk::ImageViewType::e3D;
    case ImageType::Cube:
        return vk::ImageViewType::eCube;
    case ImageType::CubeArray:
        return vk::ImageViewType::eCubeArray;
    }
    return vk::ImageViewType::e2D; // Fallback
}

vk::ImageCreateInfo GetImageCreateInfo(const ImageDesc& desc, bool initial_data)
{
    // Determine Vulkan image type and dimensions
    vk::ImageType vk_image_type;
    vk::Extent3D vk_extent;
    uint32_t array_layers;

    switch (desc.image_type)
    {
    case ImageType::Tex1d:
        vk_image_type = vk::ImageType::e1D;
        vk_extent = vk::Extent3D{desc.extent[0], 1, 1};
        array_layers = 1;
        break;

    case ImageType::Tex1dArray:
        vk_image_type = vk::ImageType::e1D;
        vk_extent = vk::Extent3D{desc.extent[0], 1, 1};
        array_layers = desc.array_elements;
        break;

    case ImageType::Tex2d:
        vk_image_type = vk::ImageType::e2D;
        vk_extent = vk::Extent3D{desc.extent[0], desc.extent[1], 1};
        array_layers = 1;
        break;

    case ImageType::Tex2dArray:
        vk_image_type = vk::ImageType::e2D;
        vk_extent = vk::Extent3D{desc.extent[0], desc.extent[1], 1};
        array_layers = desc.array_elements;
        break;

    case ImageType::Tex3d:
        vk_image_type = vk::ImageType::e3D;
        vk_extent = vk::Extent3D{desc.extent[0], desc.extent[1], desc.extent[2]};
        array_layers = 1;
        break;

    case ImageType::Cube:
        vk_image_type = vk::ImageType::e2D;
        vk_extent = vk::Extent3D{desc.extent[0], desc.extent[1], 1};
        array_layers = 6;
        break;

    case ImageType::CubeArray:
        vk_image_type = vk::ImageType::e2D;
        vk_extent = vk::Extent3D{desc.extent[0], desc.extent[1], 1};
        array_layers = 6 * desc.array_elements;
        break;
    }

    // Add transfer dst usage if initial data provided
    vk::ImageUsageFlags usage = desc.usage;
    if (initial_data)
    {
        usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    // Create image info
    vk::ImageCreateInfo create_info{};
    create_info.flags = desc.flags;
    create_info.imageType = vk_image_type;
    create_info.format = desc.format;
    create_info.extent = vk_extent;
    create_info.mipLevels = desc.mip_levels;
    create_info.arrayLayers = array_layers;
    create_info.samples = vk::SampleCountFlagBits::e1; // TODO: support MSAA
    create_info.tiling = desc.tiling;
    create_info.usage = usage;
    create_info.sharingMode = vk::SharingMode::eExclusive;
    create_info.initialLayout = initial_data ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;

    return create_info;
}

} // namespace tekki::backend::vulkan
