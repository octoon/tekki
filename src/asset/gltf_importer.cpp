// tekki - C++ port of kajiya renderer
#include <fmt/format.h>
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/gltf_importer.h"
#include "tekki/core/log.h"
#include <algorithm>
#include <sstream>
#include <fstream>

namespace tekki::asset {

// ParsedUri Parse implementation
ParsedUri UriResolver::Parse(const std::string& uri) {
    ParsedUri result;

    if (uri.find(':') != std::string::npos) {
        if (uri.starts_with("data:")) {
            // data:[<mime>];base64,<data>
            size_t base64Pos = uri.find(";base64,");
            if (base64Pos != std::string::npos) {
                result.scheme = UriScheme::Data;
                result.mimeType = uri.substr(5, base64Pos - 5);
                result.content = uri.substr(base64Pos + 8);
            } else {
                size_t commaPos = uri.find(',');
                if (commaPos != std::string::npos) {
                    result.scheme = UriScheme::Data;
                    result.mimeType = "";
                    result.content = uri.substr(commaPos + 1);
                } else {
                    result.scheme = UriScheme::Unsupported;
                }
            }
        } else if (uri.starts_with("file://")) {
            result.scheme = UriScheme::File;
            result.content = uri.substr(7);
        } else if (uri.starts_with("file:")) {
            result.scheme = UriScheme::File;
            result.content = uri.substr(5);
        } else {
            result.scheme = UriScheme::Unsupported;
        }
    } else {
        result.scheme = UriScheme::Relative;
        result.content = uri;
    }

    return result;
}

// Base64 decode table
static constexpr uint8_t kBase64DecodeTable[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

Result<std::vector<uint8_t>> UriResolver::DecodeBase64(const std::string& base64) {
    std::vector<uint8_t> result;
    result.reserve((base64.size() * 3) / 4);

    uint32_t val = 0;
    int valBits = -8;

    for (unsigned char c : base64) {
        if (kBase64DecodeTable[c] == 64) break;
        val = (val << 6) | kBase64DecodeTable[c];
        valBits += 6;
        if (valBits >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valBits) & 0xFF));
            valBits -= 8;
        }
    }

    return Ok(std::move(result));
}

Result<std::vector<uint8_t>> UriResolver::ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return Err<std::vector<uint8_t>>(fmt::format("Failed to open file: {}", path.string()));
    }

    size_t fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    return Ok(std::move(buffer));
}

Result<std::vector<uint8_t>> UriResolver::Read(
    const std::optional<std::filesystem::path>& basePath,
    const std::string& uri
) {
    ParsedUri parsed = Parse(uri);

    switch (parsed.scheme) {
        case UriScheme::Data:
            return DecodeBase64(parsed.content);

        case UriScheme::File:
            if (basePath) {
                return ReadFile(basePath.value());
            }
            return ReadFile(parsed.content);

        case UriScheme::Relative:
            if (basePath) {
                return ReadFile(basePath.value() / parsed.content);
            }
            return Err<std::vector<uint8_t>>("No base path for relative URI");

        case UriScheme::Unsupported:
        default:
            return Err<std::vector<uint8_t>>("Unsupported URI scheme");
    }
}

} // namespace tekki::asset
