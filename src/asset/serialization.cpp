// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/asset_pipeline.h"
#include "tekki/core/log.h"
#include <cstring>

namespace tekki::asset {

// FlattenContext implementation
void FlattenContext::WritePlainField(const void* data, size_t size) {
    size_t offset = bytes_.size();
    bytes_.resize(offset + size);
    std::memcpy(bytes_.data() + offset, data, size);
}

void FlattenContext::WriteBytes(const void* data, size_t size) {
    WritePlainField(data, size);
}

struct Section {
    std::vector<uint8_t> bytes;
    std::vector<std::pair<size_t, size_t>> fixups;  // (fixupAddr, targetSectionIdx)
};

void FlattenContext::AllocateSectionIndices() {
    size_t counter = 0;
    AllocateSectionIndicesImpl(counter);
}

void FlattenContext::AllocateSectionIndicesImpl(size_t& counter) {
    sectionIdx_ = counter;
    counter++;

    for (auto& deferred : deferred_) {
        deferred.nested->AllocateSectionIndicesImpl(counter);
    }
}

void FlattenContext::Finish(std::vector<uint8_t>& output) {
    AllocateSectionIndices();

    // Build flattened sections
    std::vector<Section> sections;

    std::vector<std::unique_ptr<FlattenContext>> ctxList;
    ctxList.push_back(std::make_unique<FlattenContext>(std::move(*this)));

    while (!ctxList.empty()) {
        std::vector<std::unique_ptr<FlattenContext>> nextCtxList;

        for (auto& ctx : ctxList) {
            Section section;
            section.bytes = std::move(ctx->bytes_);

            for (auto& deferred : ctx->deferred_) {
                section.fixups.emplace_back(
                    deferred.fixupAddr,
                    deferred.nested->sectionIdx_.value()
                );
                nextCtxList.push_back(std::move(deferred.nested));
            }

            sections.push_back(std::move(section));
        }

        ctxList = std::move(nextCtxList);
    }

    // Calculate section addresses
    std::vector<size_t> sectionBaseAddr;
    size_t totalBytes = 0;

    for (const auto& section : sections) {
        sectionBaseAddr.push_back(totalBytes);
        totalBytes += section.bytes.size();
    }

    // Apply fixups
    for (size_t i = 0; i < sections.size(); ++i) {
        auto& section = sections[i];
        size_t sectionAddr = sectionBaseAddr[i];

        for (const auto& [fixupAddr, targetSection] : section.fixups) {
            uint64_t fixupTarget = sectionBaseAddr[targetSection];
            uint64_t fixupRelative = fixupTarget - (fixupAddr + sectionAddr);

            std::memcpy(
                section.bytes.data() + fixupAddr,
                &fixupRelative,
                sizeof(uint64_t)
            );
        }
    }

    // Write to output
    output.clear();
    output.reserve(totalBytes);

    for (const auto& section : sections) {
        output.insert(output.end(), section.bytes.begin(), section.bytes.end());
    }
}

// Serialization helpers
static uint64_t ComputeHash(const void* data, size_t size) {
    // Simple FNV-1a hash
    uint64_t hash = 0xcbf29ce484222325;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3;
    }

    return hash;
}

// Serialize GPU image
std::vector<uint8_t> SerializeGpuImage(const GpuImageProto& image) {
    FlattenContext ctx;

    // Write format
    ctx.WritePlainField(&image.format, sizeof(VkFormat));

    // Write extent
    ctx.WritePlainField(image.extent.data(), sizeof(uint32_t) * 3);

    // Write mips vector header
    size_t mipsFixupAddr = ctx.WriteVecHeader<std::vector<uint8_t>>(image.mips.size());

    // Create nested context for mips
    auto nestedMips = std::make_unique<FlattenContext>();

    for (const auto& mip : image.mips) {
        size_t mipFixupAddr = nestedMips->WriteVecHeader<uint8_t>(mip.size());

        auto nestedMip = std::make_unique<FlattenContext>();
        nestedMip->WriteBytes(mip.data(), mip.size());

        nestedMips->deferred_.push_back({mipFixupAddr, std::move(nestedMip)});
    }

    ctx.deferred_.push_back({mipsFixupAddr, std::move(nestedMips)});

    std::vector<uint8_t> output;
    ctx.Finish(output);

    return output;
}

// Serialize packed mesh
std::vector<uint8_t> SerializePackedMesh(const PackedTriMesh& mesh) {
    FlattenContext ctx;

    // Helper to write vector
    auto writeVec = [&ctx](const auto& vec, const char* name) {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        size_t fixupAddr = ctx.WriteVecHeader<T>(vec.size());

        auto nested = std::make_unique<FlattenContext>();
        nested->WriteBytes(vec.data(), vec.size() * sizeof(T));

        ctx.deferred_.push_back({fixupAddr, std::move(nested)});
    };

    writeVec(mesh.verts, "verts");
    writeVec(mesh.uvs, "uvs");
    writeVec(mesh.tangents, "tangents");
    writeVec(mesh.colors, "colors");
    writeVec(mesh.indices, "indices");
    writeVec(mesh.materialIds, "materialIds");
    writeVec(mesh.materials, "materials");

    // Write maps as AssetRef vector
    size_t mapsFixupAddr = ctx.WriteVecHeader<AssetRef<SerializedGpuImage>>(mesh.maps.size());
    auto nestedMaps = std::make_unique<FlattenContext>();

    for (const auto& map : mesh.maps) {
        // Compute identity hash for the map
        uint64_t identity = 0;

        if (map.IsImage()) {
            const auto& imgData = std::get<MeshMaterialMap::ImageData>(map.data);
            if (imgData.source.IsFile()) {
                const auto& path = imgData.source.GetFilePath();
                identity = ComputeHash(path.string().c_str(), path.string().size());
            } else {
                const auto& data = imgData.source.GetMemoryData();
                identity = ComputeHash(data.data(), data.size());
            }
        }

        AssetRef<SerializedGpuImage> ref(identity);
        nestedMaps->WritePlainField(&ref, sizeof(ref));
    }

    ctx.deferred_.push_back({mapsFixupAddr, std::move(nestedMaps)});

    std::vector<uint8_t> output;
    ctx.Finish(output);

    return output;
}

// Deserialization
const SerializedGpuImage* DeserializeGpuImage(const void* data) {
    return static_cast<const SerializedGpuImage*>(data);
}

const SerializedPackedMesh* DeserializePackedMesh(const void* data) {
    return static_cast<const SerializedPackedMesh*>(data);
}

} // namespace tekki::asset
