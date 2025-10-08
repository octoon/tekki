#pragma once

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "../backend/vulkan/device.h"
#include "../backend/vulkan/dynamic_constants.h"
#include "../backend/vulkan/image.h"
#include "../backend/vulkan/shader.h"
#include "Resource.h"
#include "ResourceRegistry.h"

namespace tekki::render_graph
{

// Forward declarations
class RenderPassApi;

// Descriptor set binding types
struct DescriptorSetBinding
{
    enum class Type
    {
        Image,
        ImageArray,
        Buffer,
        RayTracingAcceleration,
        DynamicBuffer,
        DynamicStorageBuffer
    };

    Type type;
    std::variant < vk::DescriptorImageInfo, std::vector<vk::DescriptorImageInfo>, vk::DescriptorBufferInfo,
        vk::AccelerationStructureKHR, struct
    {
        vk::DescriptorBufferInfo buffer;
        uint32_t offset;
    }, struct
    {
        vk::DescriptorBufferInfo buffer;
        uint32_t offset;
    } > data;
};

// Common pipeline binding
struct RenderPassCommonShaderPipelineBinding
{
    std::vector<std::pair<uint32_t, std::vector<RenderPassBinding>>> bindings;
    std::vector<std::pair<uint32_t, vk::DescriptorSet>> raw_bindings;
};

// Pipeline binding wrapper
template <typename HandleType> class RenderPassPipelineBinding
{
public:
    HandleType pipeline;
    RenderPassCommonShaderPipelineBinding binding;

    RenderPassPipelineBinding(HandleType pipeline) : pipeline(pipeline) {}

    RenderPassPipelineBinding& descriptor_set(uint32_t set_idx, const std::vector<RenderPassBinding>& bindings)
    {
        binding.bindings.emplace_back(set_idx, bindings);
        return *this;
    }

    RenderPassPipelineBinding& raw_descriptor_set(uint32_t set_idx, vk::DescriptorSet descriptor_set)
    {
        binding.raw_bindings.emplace_back(set_idx, descriptor_set);
        return *this;
    }
};

// Into binding trait
template <typename T>
concept IntoRenderPassPipelineBinding = requires(T t) {
    { t.into_binding() } -> std::same_as<RenderPassPipelineBinding<T>>;
};

// Main render pass API
class RenderPassApi
{
public:
    CommandBuffer* cb;
    ResourceRegistry* resources;

    // Device access
    Device* device() { return resources->execution_params->device; }

    // Dynamic constants access
    DynamicConstants* dynamic_constants() { return resources->dynamic_constants; }

    // Pipeline binding
    template <IntoRenderPassPipelineBinding HandleType>
    auto bind_pipeline(RenderPassPipelineBinding<HandleType> binding);

    // Specific pipeline binding methods
    BoundComputePipeline bind_compute_pipeline(RenderPassPipelineBinding<RgComputePipelineHandle> binding);
    BoundRasterPipeline bind_raster_pipeline(RenderPassPipelineBinding<RgRasterPipelineHandle> binding);
    BoundRayTracingPipeline bind_ray_tracing_pipeline(RenderPassPipelineBinding<RgRtPipelineHandle> binding);

    // Render pass management
    void begin_render_pass(const RenderPass* render_pass, std::array<uint32_t, 2> dims,
                           const std::vector<std::pair<Ref<ImageResource, GpuRt>, ImageViewDesc>>& color_attachments,
                           std::optional<std::pair<Ref<ImageResource, GpuRt>, ImageViewDesc>> depth_attachment);

    void end_render_pass();

    // Viewport and scissor
    void set_default_view_and_scissor(std::array<uint32_t, 2> dims);

private:
    void bind_pipeline_common(Device* device, const ShaderPipelineCommon* pipeline,
                              const RenderPassCommonShaderPipelineBinding* binding);
};

// Bound pipeline types
class BoundComputePipeline
{
public:
    RenderPassApi* api;
    std::shared_ptr<ComputePipeline> pipeline;

    void dispatch(std::array<uint32_t, 3> threads);
    void dispatch_indirect(Ref<BufferResource, GpuSrv> args_buffer, uint64_t args_buffer_offset);
    void push_constants(vk::CommandBuffer command_buffer, uint32_t offset, const std::vector<uint8_t>& constants);
};

class BoundRasterPipeline
{
public:
    RenderPassApi* api;
    std::shared_ptr<RasterPipeline> pipeline;

    void push_constants(vk::CommandBuffer command_buffer, vk::ShaderStageFlags stage_flags, uint32_t offset,
                        const std::vector<uint8_t>& constants);
};

class BoundRayTracingPipeline
{
public:
    RenderPassApi* api;
    std::shared_ptr<RayTracingPipeline> pipeline;

    void trace_rays(std::array<uint32_t, 3> threads);
    void trace_rays_indirect(Ref<BufferResource, GpuSrv> args_buffer, uint64_t args_buffer_offset);
};

// Render pass binding types
struct RenderPassImageBinding
{
    GraphRawResourceHandle handle;
    ImageViewDesc view_desc;
    vk::ImageLayout image_layout;
};

struct RenderPassBufferBinding
{
    GraphRawResourceHandle handle;
};

struct RenderPassRayTracingAccelerationBinding
{
    GraphRawResourceHandle handle;
};

struct RenderPassBinding
{
    enum class Type
    {
        Image,
        ImageArray,
        Buffer,
        RayTracingAcceleration,
        DynamicConstants,
        DynamicConstantsStorageBuffer
    };

    Type type;
    std::variant<RenderPassImageBinding, std::vector<RenderPassImageBinding>, RenderPassBufferBinding,
                 RenderPassRayTracingAccelerationBinding, uint32_t, uint32_t>
        data;
};

// Bind trait for resource references
template <typename T>
concept BindRgRef = requires(T t) {
    { t.bind() } -> std::same_as<RenderPassBinding>;
};

// Implementation for various resource types
template <BindRgRef T> RenderPassBinding bind_rg_ref(const T& ref)
{
    return ref.bind();
}

// Specialized bind methods for different resource types
inline RenderPassBinding bind_rg_ref(const Ref<ImageResource, GpuSrv>& ref)
{
    return RenderPassBinding{.type = RenderPassBinding::Type::Image,
                             .data = RenderPassImageBinding{.handle = ref.handle,
                                                            .view_desc = ImageViewDesc{},
                                                            .image_layout = vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL}};
}

inline RenderPassBinding bind_rg_ref(const std::vector<Ref<ImageResource, GpuSrv>>& refs)
{
    std::vector<RenderPassImageBinding> bindings;
    for (const auto& ref : refs)
    {
        bindings.push_back({.handle = ref.handle,
                            .view_desc = ImageViewDesc{},
                            .image_layout = vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL});
    }
    return RenderPassBinding{.type = RenderPassBinding::Type::ImageArray, .data = bindings};
}

inline RenderPassBinding bind_rg_ref(const Ref<ImageResource, GpuUav>& ref)
{
    return RenderPassBinding{.type = RenderPassBinding::Type::Image,
                             .data = RenderPassImageBinding{.handle = ref.handle,
                                                            .view_desc = ImageViewDesc{},
                                                            .image_layout = vk::ImageLayout::GENERAL}};
}

inline RenderPassBinding bind_rg_ref(const Ref<BufferResource, GpuSrv>& ref)
{
    return RenderPassBinding{.type = RenderPassBinding::Type::Buffer, .data = RenderPassBufferBinding{ref.handle}};
}

inline RenderPassBinding bind_rg_ref(const Ref<BufferResource, GpuUav>& ref)
{
    return RenderPassBinding{.type = RenderPassBinding::Type::Buffer, .data = RenderPassBufferBinding{ref.handle}};
}

inline RenderPassBinding bind_rg_ref(const Ref<RayTracingAccelerationResource, GpuSrv>& ref)
{
    return RenderPassBinding{.type = RenderPassBinding::Type::RayTracingAcceleration,
                             .data = RenderPassRayTracingAccelerationBinding{ref.handle}};
}

// Helper function for descriptor set binding
void bind_descriptor_set(Device* device, CommandBuffer* cb, const ShaderPipelineCommon* pipeline, uint32_t set_index,
                         const std::vector<DescriptorSetBinding>& bindings);

} // namespace tekki::render_graph