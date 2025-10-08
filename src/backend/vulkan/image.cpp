// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/image.rs

#include "backend/vulkan/image.h"

#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan {

Image::Image(vk::Image image, const ImageDesc& desc, class SubAllocation allocation)
    : image_(image), desc_(desc), allocation_(std::move(allocation)) {
}

Image::~Image() {
    // Image destruction should be handled by Device
}

vk::ImageView Image::create_view(
    vk::Device device,
    vk::ImageViewType view_type,
    vk::ImageSubresourceRange subresource_range,
    vk::ComponentMapping components
) const {
    vk::ImageViewCreateInfo view_info{
        .image = image_,
        .viewType = view_type,
        .format = desc_.format,
        .components = components,
        .subresourceRange = subresource_range
    };

    return device.createImageView(view_info);
}

ImageView::ImageView(vk::ImageView view, vk::Device device)
    : view_(view), device_(device) {
}

ImageView::~ImageView() {
    if (view_) {
        device_.destroyImageView(view_);
    }
}

} // namespace tekki::backend::vulkan