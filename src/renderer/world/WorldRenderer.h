#pragma once

#include "../../render_graph/RenderGraph.h"
#include "../../backend/vulkan/device.h"
#include "../../backend/vulkan/image.h"
#include "../../backend/vulkan/buffer.h"
#include "../../backend/vulkan/ray_tracing.h"
#include "../../backend/vulkan/shader.h"
#include "../../backend/vulkan/dynamic_constants.h"
#include "../renderers/renderers.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>

namespace tekki::renderer::world {

// Forward declarations
class MeshManager;
struct MeshHandle;
struct InstanceHandle;

// GPU mesh structure - matches Rust layout
struct GpuMesh {
    uint32_t vertex_core_offset;
    uint32_t vertex_uv_offset;
    uint32_t vertex_mat_offset;
    uint32_t vertex_aux_offset;
    uint32_t vertex_tangent_offset;
    uint32_t mat_data_offset;
    uint32_t index_offset;
};

// Mesh handle
struct MeshHandle {
    size_t id;

    MeshHandle(size_t id = 0) : id(id) {}

    bool operator==(const MeshHandle& other) const {
        return id == other.id;
    }

    bool operator!=(const MeshHandle& other) const {
        return id != other.id;
    }
};

// Instance handle
struct InstanceHandle {
    size_t id;

    static const InstanceHandle INVALID;

    InstanceHandle(size_t id = 0) : id(id) {}

    bool is_valid() const {
        return *this != INVALID;
    }

    bool operator==(const InstanceHandle& other) const {
        return id == other.id;
    }

    bool operator!=(const InstanceHandle& other) const {
        return id != other.id;
    }
};

// Instance dynamic parameters
struct InstanceDynamicParameters {
    float emissive_multiplier = 1.0f;

    InstanceDynamicParameters() = default;
    InstanceDynamicParameters(float emissive_multiplier) : emissive_multiplier(emissive_multiplier) {}
};

// Mesh instance
struct MeshInstance {
    glm::mat4 transform;
    glm::mat4 prev_transform;
    MeshHandle mesh;
    InstanceDynamicParameters dynamic_parameters;

    MeshInstance() = default;
    MeshInstance(const glm::mat4& transform, MeshHandle mesh, const InstanceDynamicParameters& params = {})
        : transform(transform), prev_transform(transform), mesh(mesh), dynamic_parameters(params) {}
};

// Render debug modes
enum class RenderDebugMode {
    None,
    WorldRadianceCache
};

// Triangle light
struct TriangleLight {
    std::array<std::array<float, 3>, 3> verts;
    std::array<float, 3> radiance;

    TriangleLight transform(const glm::vec3& translation, const glm::mat3& rotation) const;
    TriangleLight scale_radiance(const glm::vec3& scale) const;
};

// Mesh light set
struct MeshLightSet {
    std::vector<TriangleLight> lights;
};

// Histogram clipping
struct HistogramClipping {
    float low = 0.0f;
    float high = 0.0f;
};

// Dynamic exposure state
struct DynamicExposureState {
    bool enabled = false;
    float speed_log2 = 0.0f;
    HistogramClipping histogram_clipping;

    float ev_fast = 0.0f;
    float ev_slow = 0.0f;

    float ev_smoothed() const;
    void update(float ev, float dt);
};

// Exposure state
struct ExposureState {
    float pre_mult = 1.0f;
    float post_mult = 1.0f;
    float pre_mult_prev = 1.0f;
    float pre_mult_delta = 1.0f;

    ExposureState() = default;
};

// Render modes
enum class RenderMode {
    Standard = 0,
    Reference = 1
};

// Bindless image handle
struct BindlessImageHandle {
    uint32_t id;

    BindlessImageHandle(uint32_t id = 0) : id(id) {}

    bool operator==(const BindlessImageHandle& other) const {
        return id == other.id;
    }

    bool operator!=(const BindlessImageHandle& other) const {
        return id != other.id;
    }
};

// Main world renderer class
class WorldRenderer {
public:
    WorldRenderer(
        const std::array<uint32_t, 2>& render_extent,
        const std::array<uint32_t, 2>& temporal_upscale_extent,
        std::shared_ptr<vulkan::Device> device
    );

    ~WorldRenderer() = default;

    // Mesh management
    MeshHandle add_mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices);
    void remove_mesh(MeshHandle handle);

    // Instance management
    InstanceHandle add_instance(const MeshInstance& instance);
    void remove_instance(InstanceHandle handle);
    void update_instance_transform(InstanceHandle handle, const glm::mat4& transform);

    // Image management
    BindlessImageHandle add_image(std::shared_ptr<vulkan::Image> image);
    void remove_image(BindlessImageHandle handle);

    // Rendering
    void render(
        render_graph::RenderGraph& rg,
        const CameraMatrices& camera,
        const std::array<uint32_t, 2>& output_extent
    );

    // Ray tracing
    void build_ray_tracing_top_level_acceleration();

    // Getters
    std::shared_ptr<vulkan::Device> device() const { return device_; }
    RenderMode render_mode() const { return render_mode_; }
    void set_render_mode(RenderMode mode) { render_mode_ = mode; }

private:
    std::shared_ptr<vulkan::Device> device_;

    // Core resources
    std::shared_ptr<vulkan::RenderPass> raster_simple_render_pass_;
    vk::DescriptorSet bindless_descriptor_set_;

    // Mesh and instance data
    std::vector<std::shared_ptr<vulkan::UploadedTriMesh>> meshes_;
    std::vector<MeshLightSet> mesh_lights_;
    std::vector<MeshInstance> instances_;
    std::vector<InstanceHandle> instance_handles_;
    std::unordered_map<InstanceHandle, size_t> instance_handle_to_index_;

    // Buffers
    std::shared_ptr<vulkan::Buffer> vertex_buffer_;
    uint64_t vertex_buffer_written_ = 0;
    std::shared_ptr<vulkan::Buffer> mesh_buffer_;

    // Ray tracing
    std::vector<std::shared_ptr<vulkan::RayTracingAcceleration>> mesh_blas_;
    std::shared_ptr<vulkan::RayTracingAcceleration> tlas_;
    vulkan::RayTracingAccelerationScratchBuffer accel_scratch_;

    // Bindless resources
    std::vector<std::shared_ptr<vulkan::Image>> bindless_images_;
    size_t next_bindless_image_id_ = 0;
    size_t next_instance_handle_ = 0;
    std::shared_ptr<vulkan::Buffer> bindless_texture_sizes_;

    // Image LUTs
    std::vector<ImageLut> image_luts_;

    // Frame state
    uint32_t frame_idx_ = 0;
    std::optional<CameraMatrices> prev_camera_matrices_;
    std::array<uint32_t, 2> temporal_upscale_extent_;

    // Supersampling
    std::vector<glm::vec2> supersample_offsets_;

    // Debug and settings
    std::optional<render_graph::GraphDebugHook> rg_debug_hook_;
    RenderMode render_mode_ = RenderMode::Standard;
    bool reset_reference_accumulation_ = false;

    // Renderers
    PostProcessRenderer post_;
    SsgiRenderer ssgi_;
    RtrRenderer rtr_;
    LightingRenderer lighting_;
    IrcacheRenderer ircache_;
    RtdgiRenderer rtdgi_;
    TaaRenderer taa_;
    ShadowDenoiseRenderer shadow_denoise_;
    IblRenderer ibl_;

    // Debug settings
    RenderDebugMode debug_mode_ = RenderDebugMode::None;
    size_t debug_shading_mode_ = 0;
    bool debug_show_wrc_ = false;
    float ev_shift_ = 0.0f;
    DynamicExposureState dynamic_exposure_;
    float contrast_ = 1.0f;

    // Lighting settings
    float sun_size_multiplier_ = 1.0f;
    glm::vec3 sun_color_multiplier_ = glm::vec3(1.0f);
    glm::vec3 sky_ambient_ = glm::vec3(0.0f);

    // Render overrides
    RenderOverrides render_overrides_;

    // Exposure state per render mode
    std::array<ExposureState, 2> exposure_state_;

    // Helper methods
    void initialize_default_resources();
    void update_frame_constants(const CameraMatrices& camera, const std::array<uint32_t, 2>& output_extent);
    void update_instance_transforms();
};

} // namespace tekki::renderer::world