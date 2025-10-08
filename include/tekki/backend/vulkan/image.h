// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/image.rs

#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

#include "core/common.h"

namespace tekki::backend::vulkan
{

struct ImageDesc
{
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageUsageFlags usage;
    vk::ImageType image_type;
    uint32_t mip_levels;
    uint32_t array_layers;
    vk::SampleCountFlagBits samples;
    vk::ImageTiling tiling;
    vk::ImageLayout initial_layout;

    ImageDesc() = default;

    static ImageDesc new_2d(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage,
                            uint32_t mip_levels = 1, uint32_t array_layers = 1)
    {
        return ImageDesc{.extent = {width, height, 1},
                         .format = format,
                         .usage = usage,
                         .image_type = vk::ImageType::e2D,
                         .mip_levels = mip_levels,
                         .array_layers = array_layers,
                         .samples = vk::SampleCountFlagBits::e1,
                         .tiling = vk::ImageTiling::eOptimal,
                         .initial_layout = vk::ImageLayout::eUndefined};
    }

    static ImageDesc new_3d(uint32_t width, uint32_t height, uint32_t depth, vk::Format format,
                            vk::ImageUsageFlags usage, uint32_t mip_levels = 1)
    {
        return ImageDesc{.extent = {width, height, depth},
                         .format = format,
                         .usage = usage,
                         .image_type = vk::ImageType::e3D,
                         .mip_levels = mip_levels,
                         .array_layers = 1,
                         .samples = vk::SampleCountFlagBits::e1,
                         .tiling = vk::ImageTiling::eOptimal,
                         .initial_layout = vk::ImageLayout::eUndefined};
    }
};

class Image
{
public:
    Image(vk::Image image, const ImageDesc& desc, class SubAllocation allocation);
    ~Image();

    // Getters
    vk::Image raw() const { return image_; }
    const ImageDesc& desc() const { return desc_; }
    vk::Extent3D extent() const { return desc_.extent; }
    vk::Format format() const { return desc_.format; }

    // View creation
    vk::ImageView create_view(vk::Device device, vk::ImageViewType view_type,
                              vk::ImageSubresourceRange subresource_range, vk::ComponentMapping components = {}) const;

private:
    vk::Image image_;
    ImageDesc desc_;
    class SubAllocation allocation_;
};

class ImageView
{
public:
    ImageView(vk::ImageView view, vk::Device device);
    ~ImageView();

    vk::ImageView raw() const { return view_; }

private:
    vk::ImageView view_;
    vk::Device device_;
};

} // namespace tekki::backend::vulkan