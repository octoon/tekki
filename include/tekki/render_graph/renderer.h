#pragma once

#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/temporal.h"

#include "tekki/backend/RenderBackend.h"
#include "tekki/backend/Device.h"
#include "tekki/backend/dynamic_constants/DynamicConstants.h"
#include "tekki/backend/pipeline_cache/PipelineCache.h"
#include "tekki/backend/transient_resource_cache/TransientResourceCache.h"
#include "tekki/backend/vulkan/swapchain/Swapchain.h"
#include "tekki/backend/vulkan/buffer/Buffer.h"
#include "tekki/backend/rspirv_reflect/rspirv_reflect.h"

#include <memory>
#include <unordered_map>

namespace tekki {
namespace render_graph {

class Renderer {
public:
    struct FrameConstantsLayout {
        uint32_t globalsOffset;
        uint32_t instanceDynamicParametersOffset;
        uint32_t triangleLightsOffset;
    };

    explicit Renderer(backend::RenderBackend& backend);

    template<typename PrepareFrameConstantsFn>
    void drawFrame(PrepareFrameConstantsFn prepareFrameConstants, backend::Swapchain& swapchain);

    template<typename PrepareRenderGraphFn>
    Result<void> prepareFrame(PrepareRenderGraphFn prepareRenderGraph);

    std::shared_ptr<backend::Device> device() const { return m_device; }

private:
    enum class TemporalRgState {
        Inert,
        Exported
    };

    struct TemporalRg {
        TemporalRgState state;
        TemporalRenderGraphState inertState;
        ExportedTemporalRenderGraphState exportedState;

        TemporalRg() : state(TemporalRgState::Inert), inertState() {}
    };

    vk::DescriptorSet createFrameDescriptorSet(backend::RenderBackend& backend, backend::Buffer& dynamicConstants);

    std::shared_ptr<backend::Device> m_device;
    backend::PipelineCache m_pipelineCache;
    backend::TransientResourceCache m_transientResourceCache;
    backend::DynamicConstants m_dynamicConstants;
    vk::DescriptorSet m_frameDescriptorSet;

    std::optional<CompiledRenderGraph> m_compiledRg;
    TemporalRg m_temporalRgState;

    static const std::unordered_map<uint32_t, backend::rspirv_reflect::DescriptorInfo> FRAME_CONSTANTS_LAYOUT;
};

} // namespace render_graph
} // namespace tekki