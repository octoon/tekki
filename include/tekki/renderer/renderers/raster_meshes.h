#pragma once

#include "renderers.h"
#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/render_pass.h"

#include <vulkan/vulkan.hpp>

namespace tekki::renderer {

struct UploadedTriMesh {
    uint64_t index_buffer_offset;
    uint32_t index_count;
};

struct RasterMeshesData {
    const std::vector<std::shared_ptr<UploadedTriMesh>>& meshes;
    const std::vector<MeshInstance>& instances;
    std::shared_ptr<vulkan::Buffer> vertex_buffer;
    vk::DescriptorSet bindless_descriptor_set;
};

void raster_meshes(
    render_graph::RenderGraph& rg,
    std::shared_ptr<vulkan::RenderPass> render_pass,
    GbufferDepth& gbuffer_depth,
    render_graph::Handle<vulkan::Image>& velocity_img,
    const RasterMeshesData& mesh_data
);

} // namespace tekki::renderer