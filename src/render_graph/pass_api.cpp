#include "tekki/render_graph/pass_api.h"

namespace tekki {
namespace render_graph {

// RenderPassApi pipeline binding methods
template<typename IntoRenderPassPipelineBinding HandleType>
auto RenderPassApi::bind_pipeline(RenderPassPipelineBinding<HandleType> binding) {
    auto pipeline = resources->compute_pipeline(binding.pipeline);
    bind_pipeline_common(device(), pipeline.get(), &binding.binding);

    if constexpr (std::is_same_v<HandleType, RgComputePipelineHandle>) {
        return BoundComputePipeline{this, std::move(pipeline)};
    } else if constexpr (std::is_same_v<HandleType, RgRasterPipelineHandle>) {
        return BoundRasterPipeline{this, std::move(pipeline)};
    } else if constexpr (std::is_same_v<HandleType, RgRtPipelineHandle>) {
        return BoundRayTracingPipeline{this, std::move(pipeline)};
    }
}

BoundComputePipeline RenderPassApi::bind_compute_pipeline(RenderPassPipelineBinding<RgComputePipelineHandle> binding) {
    auto pipeline = resources->compute_pipeline(binding.pipeline);
    bind_pipeline_common(device(), pipeline.get(), &binding.binding);
    return BoundComputePipeline{this, std::move(pipeline)};
}

BoundRasterPipeline RenderPassApi::bind_raster_pipeline(RenderPassPipelineBinding<RgRasterPipelineHandle> binding) {
    auto pipeline = resources->raster_pipeline(binding.pipeline);
    bind_pipeline_common(device(), pipeline.get(), &binding.binding);
    return BoundRasterPipeline{this, std::move(pipeline)};
}

BoundRayTracingPipeline RenderPassApi::bind_ray_tracing_pipeline(RenderPassPipelineBinding<RgRtPipelineHandle> binding) {
    auto pipeline = resources->ray_tracing_pipeline(binding.pipeline);
    bind_pipeline_common(device(), pipeline.get(), &binding.binding);
    return BoundRayTracingPipeline{this, std::move(pipeline)};
}

// RenderPassApi render pass management
void RenderPassApi::begin_render_pass(
    const RenderPass* render_pass,
    std::array<uint32_t, 2> dims,
    const std::vector<std::pair<Ref<ImageResource, GpuRt>, ImageViewDesc>>& color_attachments,
    std::optional<std::pair<Ref<ImageResource, GpuRt>, ImageViewDesc>> depth_attachment) {

    // Implementation for beginning render pass
    // This would typically create framebuffer and begin the render pass
    // For now, this is a placeholder
}

void RenderPassApi::end_render_pass() {
    // Implementation for ending render pass
    // This would typically end the render pass
}

void RenderPassApi::set_default_view_and_scissor(std::array<uint32_t, 2> dims) {
    vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(dims[0]),
        .height = static_cast<float>(dims[1]),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vk::Rect2D scissor{
        .offset = {0, 0},
        .extent = {dims[0], dims[1]}
    };

    cb->cmdSetViewport(0, 1, &viewport);
    cb->cmdSetScissor(0, 1, &scissor);
}

void RenderPassApi::bind_pipeline_common(
    Device* device,
    const ShaderPipelineCommon* pipeline,
    const RenderPassCommonShaderPipelineBinding* binding) {

    // Bind descriptor sets
    for (const auto& [setIdx, bindings] : binding->bindings) {
        std::vector<DescriptorSetBinding> descriptorBindings;
        descriptorBindings.reserve(bindings.size());

        for (const auto& binding : bindings) {
            descriptorBindings.push_back(convertToDescriptorSetBinding(binding));
        }

        bind_descriptor_set(device, cb, pipeline, setIdx, descriptorBindings);
    }

    // Bind raw descriptor sets
    for (const auto& [setIdx, descriptorSet] : binding->raw_bindings) {
        cb->cmdBindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            pipeline->layout(),
            setIdx,
            1,
            &descriptorSet,
            0,
            nullptr
        );
    }
}

// Bound pipeline methods
void BoundComputePipeline::dispatch(std::array<uint32_t, 3> threads) {
    api->cb->cmdDispatch(threads[0], threads[1], threads[2]);
}

void BoundComputePipeline::dispatch_indirect(Ref<BufferResource, GpuSrv> args_buffer, uint64_t args_buffer_offset) {
    auto buffer = api->resources->buffer(args_buffer);
    api->cb->cmdDispatchIndirect(buffer->raw(), args_buffer_offset);
}

void BoundComputePipeline::push_constants(vk::CommandBuffer command_buffer, uint32_t offset, const std::vector<uint8_t>& constants) {
    command_buffer.pushConstants(
        pipeline->layout(),
        vk::ShaderStageFlagBits::eCompute,
        offset,
        constants.size(),
        constants.data()
    );
}

void BoundRasterPipeline::push_constants(vk::CommandBuffer command_buffer, vk::ShaderStageFlags stage_flags, uint32_t offset, const std::vector<uint8_t>& constants) {
    command_buffer.pushConstants(
        pipeline->layout(),
        stage_flags,
        offset,
        constants.size(),
        constants.data()
    );
}

void BoundRayTracingPipeline::trace_rays(std::array<uint32_t, 3> threads) {
    // Implementation for trace rays
    // This would typically call vkCmdTraceRaysKHR
}

void BoundRayTracingPipeline::trace_rays_indirect(Ref<BufferResource, GpuSrv> args_buffer, uint64_t args_buffer_offset) {
    // Implementation for indirect trace rays
    // This would typically call vkCmdTraceRaysIndirectKHR
}

// Helper function for descriptor set binding
void bind_descriptor_set(
    Device* device,
    CommandBuffer* cb,
    const ShaderPipelineCommon* pipeline,
    uint32_t set_index,
    const std::vector<DescriptorSetBinding>& bindings) {

    // Implementation for binding descriptor sets
    // This would typically create descriptor set writes and update descriptor sets
    // For now, this is a placeholder
}

// Helper function to convert RenderPassBinding to DescriptorSetBinding
DescriptorSetBinding convertToDescriptorSetBinding(const RenderPassBinding& binding) {
    switch (binding.type) {
        case RenderPassBinding::Type::Image: {
            auto& imageBinding = std::get<RenderPassImageBinding>(binding.data);
            return DescriptorSetBinding{
                .type = DescriptorSetBinding::Type::Image,
                .data = vk::DescriptorImageInfo{
                    .imageView = imageBinding.view_desc.createView(),
                    .imageLayout = imageBinding.image_layout
                }
            };
        }
        case RenderPassBinding::Type::Buffer: {
            auto& bufferBinding = std::get<RenderPassBufferBinding>(binding.data);
            return DescriptorSetBinding{
                .type = DescriptorSetBinding::Type::Buffer,
                .data = vk::DescriptorBufferInfo{
                    .buffer = bufferBinding.handle.getBuffer(),
                    .range = VK_WHOLE_SIZE
                }
            };
        }
        case RenderPassBinding::Type::DynamicConstants: {
            auto offset = std::get<uint32_t>(binding.data);
            return DescriptorSetBinding{
                .type = DescriptorSetBinding::Type::DynamicBuffer,
                .data = DescriptorSetBinding::DynamicBuffer{
                    .buffer = {}, // Would be dynamic constants buffer
                    .offset = offset
                }
            };
        }
        default:
            throw std::runtime_error("Unsupported binding type");
    }
}

} // namespace render_graph
} // namespace tekki