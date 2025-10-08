#include "WorldRenderer.h"
#include "../renderers/post.h"
#include "../renderers/ssgi.h"
#include "../renderers/rtr.h"
#include "../renderers/lighting.h"
#include "../renderers/ircache.h"
#include "../renderers/rtdgi.h"
#include "../renderers/taa.h"
#include "../renderers/shadow_denoise.h"
#include "../renderers/ibl.h"

#include "../../backend/vulkan/render_pass.h"
#include "../../backend/vulkan/bindless_descriptor_set.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace tekki::renderer::world {

const InstanceHandle InstanceHandle::INVALID = InstanceHandle(~0);

// Helper function for radical inverse (used for supersampling offsets)
float radical_inverse(uint32_t n, uint32_t base) {
    float val = 0.0f;
    float inv_base = 1.0f / static_cast<float>(base);
    float inv_bi = inv_base;

    while (n > 0) {
        uint32_t d_i = n % base;
        val += static_cast<float>(d_i) * inv_bi;
        n = static_cast<uint32_t>(static_cast<float>(n) * inv_base);
        inv_bi *= inv_base;
    }

    return val;
}

WorldRenderer::WorldRenderer(
    const std::array<uint32_t, 2>& render_extent,
    const std::array<uint32_t, 2>& temporal_upscale_extent,
    std::shared_ptr<vulkan::Device> device
) : device_(device), temporal_upscale_extent_(temporal_upscale_extent) {
    initialize_default_resources();
}

void WorldRenderer::initialize_default_resources() {
    // Create raster simple render pass
    vulkan::RenderPassDesc render_pass_desc{
        .color_attachments = {
            // view-space geometry normal; * 2 - 1 to decode
            vulkan::RenderPassAttachmentDesc::new_(vk::Format::eA2R10G10B10UnormPack32)
                .garbage_input(),
            // gbuffer
            vulkan::RenderPassAttachmentDesc::new_(vk::Format::eR32G32B32A32Sfloat).garbage_input(),
            // velocity
            vulkan::RenderPassAttachmentDesc::new_(vk::Format::eR16G16B16A16Sfloat).garbage_input(),
        },
        .depth_attachment = vulkan::RenderPassAttachmentDesc::new_(vk::Format::eD32Sfloat)
    };

    raster_simple_render_pass_ = device_->create_render_pass(render_pass_desc);

    // Create mesh buffer
    constexpr size_t MAX_GPU_MESHES = 1024;
    constexpr size_t VERTEX_BUFFER_CAPACITY = 1024 * 1024 * 1024;

    mesh_buffer_ = device_->create_buffer(
        vulkan::BufferDesc::new_cpu_to_gpu(
            MAX_GPU_MESHES * sizeof(GpuMesh),
            vk::BufferUsageFlagBits::eStorageBuffer
        ),
        "mesh buffer"
    );

    vertex_buffer_ = device_->create_buffer(
        vulkan::BufferDesc::new_gpu_only(
            VERTEX_BUFFER_CAPACITY,
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eIndexBuffer |
            vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
        ),
        "vertex buffer"
    );

    // Create bindless descriptor set
    bindless_descriptor_set_ = vulkan::create_bindless_descriptor_set(device_.get());

    // Create bindless texture sizes buffer
    bindless_texture_sizes_ = device_->create_buffer(
        vulkan::BufferDesc::new_cpu_to_gpu(
            device_->max_bindless_descriptor_count() * sizeof(std::array<float, 4>),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
        ),
        "bindless_texture_sizes"
    );

    // Write descriptor sets
    vulkan::write_descriptor_set_buffer(
        device_->raw(),
        bindless_descriptor_set_,
        0,
        mesh_buffer_.get()
    );

    vulkan::write_descriptor_set_buffer(
        device_->raw(),
        bindless_descriptor_set_,
        1,
        vertex_buffer_.get()
    );

    vulkan::write_descriptor_set_buffer(
        device_->raw(),
        bindless_descriptor_set_,
        2,
        bindless_texture_sizes_.get()
    );

    // Initialize supersample offsets
    constexpr size_t supersample_count = 128;
    supersample_offsets_.reserve(supersample_count);
    for (size_t i = 1; i <= supersample_count; ++i) {
        float x = radical_inverse(static_cast<uint32_t>(i), 2) - 0.5f;
        float y = radical_inverse(static_cast<uint32_t>(i), 3) - 0.5f;
        supersample_offsets_.emplace_back(x, y);
    }

    // Initialize ray tracing scratch buffer
    accel_scratch_ = device_->create_ray_tracing_acceleration_scratch_buffer();

    // Initialize renderers
    post_ = PostProcessRenderer();
    ssgi_ = SsgiRenderer();
    rtr_ = RtrRenderer();
    lighting_ = LightingRenderer();
    ircache_ = IrcacheRenderer();
    rtdgi_ = RtdgiRenderer();
    taa_ = TaaRenderer();
    shadow_denoise_ = ShadowDenoiseRenderer();
    ibl_ = IblRenderer();

    // Initialize exposure state
    exposure_state_[0] = ExposureState();
    exposure_state_[1] = ExposureState();
}

MeshHandle WorldRenderer::add_mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices) {
    // TODO: Implement mesh loading and processing
    // This is a simplified version - the full implementation would need to handle
    // materials, textures, and mesh processing similar to the Rust version

    size_t mesh_idx = meshes_.size();

    // Create a simple uploaded mesh
    auto uploaded_mesh = std::make_shared<vulkan::UploadedTriMesh>();
    uploaded_mesh->index_buffer_offset = 0; // TODO: Calculate actual offset
    uploaded_mesh->index_count = indices.size();

    meshes_.push_back(uploaded_mesh);

    // Create mesh lights (empty for now)
    mesh_lights_.emplace_back();

    return MeshHandle(mesh_idx);
}

void WorldRenderer::remove_mesh(MeshHandle handle) {
    if (handle.id >= meshes_.size()) {
        return;
    }

    // Mark mesh as removed (in a real implementation, we'd need to handle
    // references from instances and clean up GPU resources)
    meshes_[handle.id] = nullptr;
    mesh_lights_[handle.id] = MeshLightSet();
}

InstanceHandle WorldRenderer::add_instance(const MeshInstance& instance) {
    size_t handle_id = next_instance_handle_++;
    InstanceHandle handle(handle_id);

    size_t index = instances_.size();

    instances_.push_back(instance);
    instance_handles_.push_back(handle);
    instance_handle_to_index_[handle] = index;

    return handle;
}

void WorldRenderer::remove_instance(InstanceHandle handle) {
    auto it = instance_handle_to_index_.find(handle);
    if (it == instance_handle_to_index_.end()) {
        return;
    }

    size_t index = it->second;
    instance_handle_to_index_.erase(it);

    // Swap remove from instances and handles
    instances_[index] = instances_.back();
    instances_.pop_back();

    instance_handles_[index] = instance_handles_.back();
    instance_handles_.pop_back();

    // Update the mapping for the instance that was moved
    if (index < instance_handles_.size()) {
        instance_handle_to_index_[instance_handles_[index]] = index;
    }
}

void WorldRenderer::update_instance_transform(InstanceHandle handle, const glm::mat4& transform) {
    auto it = instance_handle_to_index_.find(handle);
    if (it != instance_handle_to_index_.end()) {
        instances_[it->second].transform = transform;
    }
}

BindlessImageHandle WorldRenderer::add_image(std::shared_ptr<vulkan::Image> image) {
    uint32_t handle_id = static_cast<uint32_t>(next_bindless_image_id_++);
    BindlessImageHandle handle(handle_id);

    // Get image size for bindless texture sizes buffer
    auto image_size = image->desc().extent_inv_extent_2d();

    // Add image view to bindless descriptor set
    auto image_view = image->view(device_.get(), vulkan::ImageViewDesc{});

    vulkan::write_descriptor_set_image(
        device_->raw(),
        bindless_descriptor_set_,
        vulkan::BINDLESS_TEXURES_BINDING_INDEX,
        handle_id,
        image_view.get()
    );

    bindless_images_.push_back(image);

    // Update bindless texture sizes buffer
    auto mapped_data = bindless_texture_sizes_->mapped_data<std::array<float, 4>>();
    if (mapped_data && handle_id < mapped_data->size()) {
        (*mapped_data)[handle_id] = image_size;
    }

    return handle;
}

void WorldRenderer::remove_image(BindlessImageHandle handle) {
    // In a real implementation, we'd need to handle descriptor set updates
    // and potentially re-use slots. For now, just mark as removed.
    if (handle.id < bindless_images_.size()) {
        bindless_images_[handle.id] = nullptr;
    }
}

void WorldRenderer::render(
    render_graph::RenderGraph& rg,
    const CameraMatrices& camera,
    const std::array<uint32_t, 2>& output_extent
) {
    // Update frame constants
    update_frame_constants(camera, output_extent);

    // Update instance transforms
    update_instance_transforms();

    // Build ray tracing acceleration structures if needed
    build_ray_tracing_top_level_acceleration();

    // TODO: Implement main rendering logic
    // This would involve setting up the render graph passes for:
    // - G-buffer generation
    // - Lighting
    // - Global illumination
    // - Post-processing
    // - Temporal anti-aliasing
}

void WorldRenderer::build_ray_tracing_top_level_acceleration() {
    if (!device_->ray_tracing_enabled()) {
        return;
    }

    // Build bottom-level acceleration structures for meshes
    for (size_t i = 0; i < meshes_.size(); ++i) {
        if (meshes_[i] && i >= mesh_blas_.size()) {
            // Create BLAS for this mesh
            // TODO: Implement BLAS creation
        }
    }

    // Build top-level acceleration structure
    std::vector<vulkan::RayTracingInstanceDesc> instances;
    for (const auto& instance : instances_) {
        if (instance.mesh.id < mesh_blas_.size() && mesh_blas_[instance.mesh.id]) {
            instances.push_back({
                .blas = mesh_blas_[instance.mesh.id],
                .transformation = instance.transform,
                .mesh_index = static_cast<uint32_t>(instance.mesh.id)
            });
        }
    }

    if (!instances.empty()) {
        vulkan::RayTracingTopAccelerationDesc tlas_desc{
            .instances = instances,
            .preallocate_bytes = 1024 * 1024 * 32 // 32 MB
        };

        tlas_ = device_->create_ray_tracing_top_acceleration(tlas_desc, accel_scratch_);
    }
}

void WorldRenderer::update_frame_constants(const CameraMatrices& camera, const std::array<uint32_t, 2>& output_extent) {
    // TODO: Implement frame constants update
    // This would involve updating camera matrices, exposure state, etc.
}

void WorldRenderer::update_instance_transforms() {
    // Update previous transforms for temporal reprojection
    for (auto& instance : instances_) {
        instance.prev_transform = instance.transform;
    }
}

} // namespace tekki::renderer::world