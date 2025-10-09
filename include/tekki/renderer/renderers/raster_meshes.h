#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/shader.h"

namespace tekki::renderer::renderers {

struct UploadedTriMesh {
    uint64_t IndexBufferOffset;
    uint32_t IndexCount;
};

struct RasterMeshesData {
    const std::vector<UploadedTriMesh>* Meshes;
    const std::vector<class MeshInstance>* Instances;
    std::shared_ptr<tekki::backend::vulkan::Buffer> VertexBuffer;
    VkDescriptorSet BindlessDescriptorSet;
};

class RasterMeshes {
public:
    static void RasterMeshes(
        class RenderGraph& renderGraph,
        std::shared_ptr<class RenderPass> renderPass,
        class GbufferDepth& gbufferDepth,
        class Handle<class Image>& velocityImage,
        const RasterMeshesData& meshData
    );
};

} // namespace tekki::renderer::renderers