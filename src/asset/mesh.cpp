#include "tekki/asset/mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <optional>

namespace tekki::asset {

uint32_t PackUnitDirection11_10_11(float x, float y, float z) {
    float clamped_x = std::max(-1.0f, std::min(1.0f, x));
    float clamped_y = std::max(-1.0f, std::min(1.0f, y));
    float clamped_z = std::max(-1.0f, std::min(1.0f, z));
    
    uint32_t packed_x = static_cast<uint32_t>((clamped_x * 0.5f + 0.5f) * ((1u << 11u) - 1u));
    uint32_t packed_y = static_cast<uint32_t>((clamped_y * 0.5f + 0.5f) * ((1u << 10u) - 1u));
    uint32_t packed_z = static_cast<uint32_t>((clamped_z * 0.5f + 0.5f) * ((1u << 11u) - 1u));
    
    return (packed_z << 21) | (packed_y << 11) | packed_x;
}

void FlattenCtx::AllocateSectionIndices() {
    size_t counter = 0;
    AllocateSectionIndicesImpl(counter);
}

void FlattenCtx::AllocateSectionIndicesImpl(size_t& counter) {
    SectionIdx = counter;
    counter++;
    
    for (auto& deferred : Deferred) {
        deferred.Nested.AllocateSectionIndicesImpl(counter);
    }
}

void FlattenCtx::Finish(std::vector<uint8_t>& writer) {
    AllocateSectionIndices();
    
    struct Section {
        std::vector<uint8_t> Bytes;
        std::vector<std::pair<size_t, size_t>> Fixups;
    };
    
    std::vector<Section> sections;
    std::vector<FlattenCtx> ctx_list;
    ctx_list.push_back(std::move(*this));
    
    while (!ctx_list.empty()) {
        std::vector<FlattenCtx> next_ctx_list;
        
        for (auto& ctx : ctx_list) {
            Section section;
            section.Bytes = std::move(ctx.Bytes);
            
            for (const auto& deferred : ctx.Deferred) {
                section.Fixups.emplace_back(deferred.FixupAddr, deferred.Nested.SectionIdx.value());
            }
            
            sections.push_back(std::move(section));
            
            for (auto& deferred : ctx.Deferred) {
                next_ctx_list.push_back(std::move(deferred.Nested));
            }
        }
        
        ctx_list = std::move(next_ctx_list);
    }
    
    size_t total_bytes = 0;
    std::vector<size_t> section_base_addr;
    for (const auto& section : sections) {
        section_base_addr.push_back(total_bytes);
        total_bytes += section.Bytes.size();
    }
    
    for (size_t i = 0; i < sections.size(); ++i) {
        auto& section = sections[i];
        size_t section_addr = section_base_addr[i];
        
        for (const auto& fixup : section.Fixups) {
            size_t fixup_addr = fixup.first;
            size_t target_section = fixup.second;
            uint64_t fixup_target = static_cast<uint64_t>(section_base_addr[target_section]);
            uint64_t fixup_relative = fixup_target - (fixup_addr + section_addr);
            
            std::memcpy(&section.Bytes[fixup_addr], &fixup_relative, sizeof(fixup_relative));
        }
    }
    
    for (const auto& section : sections) {
        writer.insert(writer.end(), section.Bytes.begin(), section.Bytes.end());
    }
}

void GpuImage::Proto::FlattenInto(std::vector<uint8_t>& writer) {
    FlattenCtx output;
    
    try {
        // Flatten format
        // flatten_plain_field(output.Bytes, Format);
        
        // Flatten extent
        std::memcpy(output.Bytes.data() + output.Bytes.size(), Extent.data(), sizeof(Extent));
        output.Bytes.resize(output.Bytes.size() + sizeof(Extent));
        
        // Flatten mips
        size_t fixup_addr = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16); // len + offset
        
        FlattenCtx nested;
        for (const auto& mip : Mips) {
            size_t mip_fixup_addr = nested.Bytes.size();
            nested.Bytes.resize(nested.Bytes.size() + 16);
            
            std::memcpy(nested.Bytes.data() + mip_fixup_addr, &mip.size(), sizeof(uint64_t));
            
            FlattenCtx mip_nested;
            mip_nested.Bytes.insert(mip_nested.Bytes.end(), mip.begin(), mip.end());
            nested.Deferred.push_back(DeferredBlob{mip_fixup_addr + 8, std::move(mip_nested)});
        }
        
        output.Deferred.push_back(DeferredBlob{fixup_addr + 8, std::move(nested)});
        
        output.Finish(writer);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to flatten GpuImage: " + std::string(e.what()));
    }
}

void PackedTriMesh::Proto::FlattenInto(std::vector<uint8_t>& writer) {
    FlattenCtx output;
    
    try {
        // Flatten verts
        size_t verts_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx verts_nested;
        for (const auto& vert : Verts) {
            std::memcpy(verts_nested.Bytes.data() + verts_nested.Bytes.size(), &vert, sizeof(PackedVertex));
            verts_nested.Bytes.resize(verts_nested.Bytes.size() + sizeof(PackedVertex));
        }
        output.Deferred.push_back(DeferredBlob{verts_fixup + 8, std::move(verts_nested)});
        
        // Flatten uvs
        size_t uvs_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx uvs_nested;
        for (const auto& uv : Uvs) {
            std::memcpy(uvs_nested.Bytes.data() + uvs_nested.Bytes.size(), uv.data(), sizeof(std::array<float, 2>));
            uvs_nested.Bytes.resize(uvs_nested.Bytes.size() + sizeof(std::array<float, 2>));
        }
        output.Deferred.push_back(DeferredBlob{uvs_fixup + 8, std::move(uvs_nested)});
        
        // Flatten tangents
        size_t tangents_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx tangents_nested;
        for (const auto& tangent : Tangents) {
            std::memcpy(tangents_nested.Bytes.data() + tangents_nested.Bytes.size(), tangent.data(), sizeof(std::array<float, 4>));
            tangents_nested.Bytes.resize(tangents_nested.Bytes.size() + sizeof(std::array<float, 4>));
        }
        output.Deferred.push_back(DeferredBlob{tangents_fixup + 8, std::move(tangents_nested)});
        
        // Flatten colors
        size_t colors_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx colors_nested;
        for (const auto& color : Colors) {
            std::memcpy(colors_nested.Bytes.data() + colors_nested.Bytes.size(), color.data(), sizeof(std::array<float, 4>));
            colors_nested.Bytes.resize(colors_nested.Bytes.size() + sizeof(std::array<float, 4>));
        }
        output.Deferred.push_back(DeferredBlob{colors_fixup + 8, std::move(colors_nested)});
        
        // Flatten indices
        size_t indices_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx indices_nested;
        for (const auto& index : Indices) {
            std::memcpy(indices_nested.Bytes.data() + indices_nested.Bytes.size(), &index, sizeof(uint32_t));
            indices_nested.Bytes.resize(indices_nested.Bytes.size() + sizeof(uint32_t));
        }
        output.Deferred.push_back(DeferredBlob{indices_fixup + 8, std::move(indices_nested)});
        
        // Flatten material_ids
        size_t material_ids_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx material_ids_nested;
        for (const auto& material_id : MaterialIds) {
            std::memcpy(material_ids_nested.Bytes.data() + material_ids_nested.Bytes.size(), &material_id, sizeof(uint32_t));
            material_ids_nested.Bytes.resize(material_ids_nested.Bytes.size() + sizeof(uint32_t));
        }
        output.Deferred.push_back(DeferredBlob{material_ids_fixup + 8, std::move(material_ids_nested)});
        
        // Flatten materials
        size_t materials_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx materials_nested;
        for (const auto& material : Materials) {
            std::memcpy(materials_nested.Bytes.data() + materials_nested.Bytes.size(), &material, sizeof(MeshMaterial));
            materials_nested.Bytes.resize(materials_nested.Bytes.size() + sizeof(MeshMaterial));
        }
        output.Deferred.push_back(DeferredBlob{materials_fixup + 8, std::move(materials_nested)});
        
        // Flatten maps
        size_t maps_fixup = output.Bytes.size();
        output.Bytes.resize(output.Bytes.size() + 16);
        
        FlattenCtx maps_nested;
        for (const auto& map : Maps) {
            std::memcpy(maps_nested.Bytes.data() + maps_nested.Bytes.size(), &map, sizeof(AssetRef<GpuImage::Flat>));
            maps_nested.Bytes.resize(maps_nested.Bytes.size() + sizeof(AssetRef<GpuImage::Flat>));
        }
        output.Deferred.push_back(DeferredBlob{maps_fixup + 8, std::move(maps_nested)});
        
        output.Finish(writer);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to flatten PackedTriMesh: " + std::string(e.what()));
    }
}

PackedTriangleMesh PackTriangleMesh(const TriangleMesh& mesh) {
    PackedTriangleMesh result;
    
    try {
        result.Verts.reserve(mesh.Positions.size());
        
        for (size_t i = 0; i < mesh.Positions.size(); ++i) {
            const auto& pos = mesh.Positions[i];
            const auto& normal = mesh.Normals[i];
            
            PackedVertex vert;
            vert.Pos = pos;
            vert.Normal = PackUnitDirection11_10_11(normal[0], normal[1], normal[2]);
            
            result.Verts.push_back(vert);
        }
        
        result.Uvs = mesh.Uvs;
        result.Tangents = mesh.Tangents;
        result.Colors = mesh.Colors;
        result.Indices = mesh.Indices;
        result.MaterialIds = mesh.MaterialIds;
        result.Materials = mesh.Materials;
        
        // Note: Maps conversion from MeshMaterialMap to AssetRef<GpuImage::Flat> would require
        // additional image loading infrastructure not provided in the header
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to pack triangle mesh: " + std::string(e.what()));
    }
    
    return result;
}

void TangentCalcContext::GenerateTangents() {
    try {
        // MikkTSpace tangent generation implementation would go here
        // This is a placeholder for the actual implementation
        
        for (size_t i = 0; i < Tangents.size(); ++i) {
            Tangents[i] = {1.0f, 0.0f, 0.0f, 1.0f};
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to generate tangents: " + std::string(e.what()));
    }
}

TriangleMesh LoadGltfScene::Run() {
    TriangleMesh result;
    
    try {
        // GLTF loading implementation would go here
        // This is a placeholder for the actual implementation
        
        // For now, return an empty mesh since we don't have the GLTF loading infrastructure
        // In a real implementation, this would load the GLTF file, parse nodes, materials,
        // and populate the TriangleMesh structure
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load GLTF scene from " + Path + ": " + std::string(e.what()));
    }
    
    return result;
}

} // namespace tekki::asset