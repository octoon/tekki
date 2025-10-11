#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace tekki::asset {

struct GpuImage {
    struct Proto {
        // Stub implementation for GPU image prototype
        glm::uvec3 extent;
        uint32_t format;
        std::vector<std::vector<uint8_t>> mips;

        void FlattenInto(std::vector<uint8_t>& writer);
    };

    struct Flat {
        // Stub implementation for flattened GPU image
        std::vector<uint8_t> data;
    };
};

template<typename T>
struct AssetRef {
    // Stub implementation for asset reference
    uint64_t id;

    AssetRef() : id(0) {}
    explicit AssetRef(uint64_t id) : id(id) {}
};

} // namespace tekki::asset