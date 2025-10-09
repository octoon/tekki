// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-asset/src/import_gltf.rs

#pragma once

#include "tekki/core/common.h"
#include "tekki/asset/image.h"
#include <filesystem>
#include <vector>
#include <string>
#include <optional>

namespace tekki::asset {

/**
 * @brief URI scheme types
 */
enum class UriScheme {
    Data,      // data:...;base64,...
    File,      // file://...
    Relative,  // ../foo/bar
    Unsupported
};

/**
 * @brief Parsed URI information
 */
struct ParsedUri {
    UriScheme scheme;
    std::string mimeType;  // for Data scheme
    std::string content;   // base64 data or file path
};

/**
 * @brief URI parser and resolver
 */
class UriResolver {
public:
    static ParsedUri Parse(const std::string& uri);
    static Result<std::vector<uint8_t>> Read(const std::optional<std::filesystem::path>& basePath, const std::string& uri);

private:
    static Result<std::vector<uint8_t>> DecodeBase64(const std::string& base64);
    static Result<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path);
};

/**
 * @brief glTF import result
 */
struct GltfImportData {
    void* document;  // tinygltf::Model*
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<ImageSource> images;
};

/**
 * @brief glTF importer
 */
class GltfImporter {
public:
    static Result<GltfImportData> Import(const std::filesystem::path& path);

private:
    static Result<std::vector<std::vector<uint8_t>>> ImportBufferData(
        const void* document,
        const std::optional<std::filesystem::path>& basePath,
        std::optional<std::vector<uint8_t>> blob
    );

    static Result<std::vector<ImageSource>> ImportImageData(
        const void* document,
        const std::optional<std::filesystem::path>& basePath,
        const std::vector<std::vector<uint8_t>>& buffers
    );
};

} // namespace tekki::asset
