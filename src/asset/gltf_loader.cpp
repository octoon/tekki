// tekki - C++ port of kajiya renderer
#include <fmt/format.h>
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/mesh.h"
#include "tekki/asset/gltf_importer.h"
#include "tekki/core/log.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <queue>

namespace tekki::asset {

// GltfData structure to hold tinygltf model
struct GltfLoader::GltfData {
    tinygltf::Model model;
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<ImageSource> images;
};

Result<std::unique_ptr<GltfLoader::GltfData>> GltfLoader::LoadGltfFile(
    const std::filesystem::path& path
) {
    auto data = std::make_unique<GltfData>();

    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = false;
    if (path.extension() == ".gltf") {
        ret = loader.LoadASCIIFromFile(&data->model, &err, &warn, path.string());
    } else if (path.extension() == ".glb") {
        ret = loader.LoadBinaryFromFile(&data->model, &err, &warn, path.string());
    } else {
        return Err<std::unique_ptr<GltfData>>("Unsupported file extension");
    }

    if (!warn.empty()) {
        TEKKI_LOG_WARN("glTF warning: {}", warn);
    }

    if (!err.empty()) {
        return Err<std::unique_ptr<GltfData>>(fmt::format("glTF error: {}", err));
    }

    if (!ret) {
        return Err<std::unique_ptr<GltfData>>("Failed to load glTF file");
    }

    // Load buffers
    data->buffers.reserve(data->model.buffers.size());
    for (const auto& buffer : data->model.buffers) {
        data->buffers.push_back(buffer.data);
    }

    // Load image sources
    std::filesystem::path basePath = path.parent_path();
    data->images.reserve(data->model.images.size());

    for (const auto& image : data->model.images) {
        if (!image.uri.empty()) {
            // Image from URI
            ParsedUri parsed = UriResolver::Parse(image.uri);
            if (parsed.scheme == UriScheme::Data) {
                auto decoded = UriResolver::DecodeBase64(parsed.content);
                if (decoded) {
                    data->images.push_back(ImageSource::FromMemory(std::move(*decoded)));
                } else {
                    return Err<std::unique_ptr<GltfData>>("Failed to decode base64 image");
                }
            } else if (parsed.scheme == UriScheme::Relative) {
                data->images.push_back(ImageSource::FromFile(basePath / parsed.content));
            } else {
                data->images.push_back(ImageSource::FromFile(parsed.content));
            }
        } else if (image.bufferView >= 0) {
            // Image from buffer view
            const auto& bufferView = data->model.bufferViews[image.bufferView];
            const auto& buffer = data->buffers[bufferView.buffer];

            std::vector<uint8_t> imageData(
                buffer.begin() + bufferView.byteOffset,
                buffer.begin() + bufferView.byteOffset + bufferView.byteLength
            );
            data->images.push_back(ImageSource::FromMemory(std::move(imageData)));
        } else {
            return Err<std::unique_ptr<GltfData>>("Invalid image source");
        }
    }

    return Ok(std::move(data));
}

static std::array<float, 6> TextureTransformToMatrix(
    const tinygltf::Value& transform
) {
    std::array<float, 6> result = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    if (!transform.IsObject()) return result;

    float rotation = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f;
    float offsetX = 0.0f, offsetY = 0.0f;

    if (transform.Has("rotation")) {
        rotation = static_cast<float>(transform.Get("rotation").GetNumberAsDouble());
    }
    if (transform.Has("scale")) {
        auto scale = transform.Get("scale");
        scaleX = static_cast<float>(scale.Get(0).GetNumberAsDouble());
        scaleY = static_cast<float>(scale.Get(1).GetNumberAsDouble());
    }
    if (transform.Has("offset")) {
        auto offset = transform.Get("offset");
        offsetX = static_cast<float>(offset.Get(0).GetNumberAsDouble());
        offsetY = static_cast<float>(offset.Get(1).GetNumberAsDouble());
    }

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

    result[0] = cosR * scaleX;
    result[1] = sinR * scaleY;
    result[2] = -sinR * scaleX;
    result[3] = cosR * scaleY;
    result[4] = offsetX;
    result[5] = offsetY;

    return result;
}

MeshMaterial GltfLoader::LoadMaterial(
    const void* materialPtr,
    const GltfData& data,
    std::vector<MeshMaterialMap>& outMaps
) {
    const auto& mat = *static_cast<const tinygltf::Material*>(materialPtr);

    MeshMaterial result;
    constexpr std::array<float, 6> DEFAULT_TRANSFORM = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    // Base color / Albedo
    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
        int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        int imageIndex = data.model.textures[texIndex].source;

        TexParams params;
        params.gamma = TexGamma::Srgb;
        params.useMips = true;
        params.compression = TexCompressionMode::Rgba;

        outMaps.push_back(MeshMaterialMap::FromImage(data.images[imageIndex], params));

        if (mat.pbrMetallicRoughness.baseColorTexture.extensions.count("KHR_texture_transform")) {
            result.mapTransforms[0] = TextureTransformToMatrix(
                mat.pbrMetallicRoughness.baseColorTexture.extensions.at("KHR_texture_transform")
            );
        }
    } else {
        outMaps.push_back(MeshMaterialMap::FromPlaceholder({255, 255, 255, 255}));
    }

    // Normal map
    if (mat.normalTexture.index >= 0) {
        int texIndex = mat.normalTexture.index;
        int imageIndex = data.model.textures[texIndex].source;

        TexParams params;
        params.gamma = TexGamma::Linear;
        params.useMips = true;
        params.compression = TexCompressionMode::Rg;

        outMaps.push_back(MeshMaterialMap::FromImage(data.images[imageIndex], params));
    } else {
        outMaps.push_back(MeshMaterialMap::FromPlaceholder({127, 127, 255, 255}));
    }

    // Metallic-Roughness
    if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
        int texIndex = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
        int imageIndex = data.model.textures[texIndex].source;

        TexParams params;
        params.gamma = TexGamma::Linear;
        params.useMips = true;
        params.compression = TexCompressionMode::Rg;
        params.channelSwizzle = std::array<size_t, 4>{1, 2, 0, 3};  // Swizzle G and B channels

        outMaps.push_back(MeshMaterialMap::FromImage(data.images[imageIndex], params));

        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.extensions.count("KHR_texture_transform")) {
            result.mapTransforms[2] = TextureTransformToMatrix(
                mat.pbrMetallicRoughness.metallicRoughnessTexture.extensions.at("KHR_texture_transform")
            );
        }
    } else {
        outMaps.push_back(MeshMaterialMap::FromPlaceholder({255, 255, 127, 255}));
    }

    // Emissive
    if (mat.emissiveTexture.index >= 0) {
        int texIndex = mat.emissiveTexture.index;
        int imageIndex = data.model.textures[texIndex].source;

        TexParams params;
        params.gamma = TexGamma::Srgb;
        params.useMips = true;
        params.compression = TexCompressionMode::Rgba;

        outMaps.push_back(MeshMaterialMap::FromImage(data.images[imageIndex], params));

        if (mat.emissiveTexture.extensions.count("KHR_texture_transform")) {
            result.mapTransforms[3] = TextureTransformToMatrix(
                mat.emissiveTexture.extensions.at("KHR_texture_transform")
            );
        }
    } else {
        outMaps.push_back(MeshMaterialMap::FromPlaceholder({255, 255, 255, 255}));
    }

    // Material properties
    const auto& pbr = mat.pbrMetallicRoughness;
    result.baseColorMult = glm::vec4(
        pbr.baseColorFactor[0],
        pbr.baseColorFactor[1],
        pbr.baseColorFactor[2],
        pbr.baseColorFactor[3]
    );
    result.roughnessMult = static_cast<float>(pbr.roughnessFactor);
    result.metalnessFactor = static_cast<float>(pbr.metallicFactor);
    result.emissive = glm::vec3(
        mat.emissiveFactor[0],
        mat.emissiveFactor[1],
        mat.emissiveFactor[2]
    );

    return result;
}

// Continue in next file...
