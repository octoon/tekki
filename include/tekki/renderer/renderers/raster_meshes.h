#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/renderer/renderers/gbuffer_depth.h"

namespace tekki::renderer::renderers {

namespace rg = tekki::render_graph;
using Image = tekki::backend::vulkan::Image;

struct UploadedTriMesh {
    uint64_t IndexBufferOffset;
    uint32_t IndexCount;
};

struct MeshInstance {
    // Placeholder - TODO: Define proper mesh instance structure
    uint32_t MeshIndex;
    glm::mat4 Transform;
};

struct RasterMeshesData {
    const std::vector<UploadedTriMesh>* Meshes;
    const std::vector<MeshInstance>* Instances;
    std::shared_ptr<tekki::backend::vulkan::Buffer> VertexBuffer;
    VkDescriptorSet BindlessDescriptorSet;
};

class RasterMeshes {
public:
    static void Render(
        rg::RenderGraph& renderGraph,
        rg::PassBuilder& renderPass,
        GbufferDepth& gbufferDepth,
        rg::Handle<Image>& velocityImage,
        const RasterMeshesData& meshData
    );
};

} // namespace tekki::renderer::renderers
