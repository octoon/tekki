// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-asset/src/lib.rs

#pragma once

#include "tekki/core/common.h"

// Asset loading and management
#include "tekki/asset/image.h"
#include "tekki/asset/mesh.h"
#include "tekki/asset/gltf_importer.h"
#include "tekki/asset/asset_pipeline.h"

namespace tekki::asset
{

/**
 * @brief Asset loading and management for tekki renderer
 *
 * This module handles loading and processing of assets
 * including meshes, textures, and materials.
 *
 * Architecture:
 * - image.h: Image loading, compression, and processing
 * - mesh.h: Mesh data structures and packing
 * - gltf_importer.h: glTF scene loading
 * - asset_pipeline.h: Async loading, caching, and serialization
 *
 * Translating from kajiya-asset and kajiya-asset-pipe Rust crates.
 */

} // namespace tekki::asset
