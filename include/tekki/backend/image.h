// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

// Forward header for backend image types
// This header provides the Vulkan types needed by asset module
// without pulling in the full backend/vulkan implementation

namespace tekki::backend {
    // VkFormat is already defined by vulkan.h
    // This file exists to provide a consistent include path
    // for asset modules that need basic Vulkan types
}
