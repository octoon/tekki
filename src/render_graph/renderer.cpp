#include "tekki/render_graph/renderer.h"

#include "tekki/core/Log.h"
#include "tekki/backend/vulkan/barrier/ImageBarrier.h"

#include <puffin.h>

namespace tekki {
namespace render_graph {

// Static frame constants layout
const std::unordered_map<uint32_t, backend::rspirv_reflect::DescriptorInfo> Renderer::FRAME_CONSTANTS_LAYOUT = {
    // frame_constants
    {
        0,
        backend::rspirv_reflect::DescriptorInfo{
            .ty = backend::rspirv_reflect::DescriptorType::UNIFORM_BUFFER,
            .dimensionality = backend::rspirv_reflect::DescriptorDimensionality::Single,
            .name = {}
        }
    },
    // instance_dynamic_parameters_dyn
    {
        1,
        backend::rspirv_reflect::DescriptorInfo{
            .ty = backend::rspirv_reflect::DescriptorType::STORAGE_BUFFER_DYNAMIC,
            .dimensionality = backend::rspirv_reflect::DescriptorDimensionality::Single,
            .name = {}
        }
    },
    // triangle_lights_dyn
    {
        2,
        backend::rspirv_reflect::DescriptorInfo{
            .ty = backend::rspirv_reflect::DescriptorType::STORAGE_BUFFER_DYNAMIC,
            .dimensionality = backend::rspirv_reflect::DescriptorDimensionality::Single,
            .name = {}
        }
    }
};

Renderer::Renderer(backend::RenderBackend& backend)
    : m_device(backend.device())
    , m_dynamicConstants(backend.device()->createBuffer(
        backend::BufferDesc::newCpuToGpu(
            backend::DYNAMIC_CONSTANTS_SIZE_BYTES * backend::DYNAMIC_CONSTANTS_BUFFER_COUNT,
            vk::BufferUsageFlagBits::eUniformBuffer |
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress
        ),
        "dynamic constants buffer",
        std::nullopt
    ))
    , m_frameDescriptorSet(createFrameDescriptorSet(backend, m_dynamicConstants.buffer()))
    , m_pipelineCache(backend::PipelineCache::create())
    , m_transientResourceCache()
    , m_compiledRg(std::nullopt)
    , m_temporalRgState() {
}

template<typename PrepareFrameConstantsFn>
void Renderer::drawFrame(PrepareFrameConstantsFn prepareFrameConstants, backend::Swapchain& swapchain) {
    auto rg = m_compiledRg.take();
    if (!rg) {
        return;
    }

    auto device = m_device;
    auto rawDevice = device->raw();

    auto currentFrame = device->beginFrame();

    // Both command buffers are accessible now, so begin recording.
    for (auto cb : {&currentFrame.mainCommandBuffer, &currentFrame.presentationCommandBuffer}) {
        rawDevice->resetCommandBuffer(cb->raw, vk::CommandBufferResetFlags{});

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        rawDevice->beginCommandBuffer(cb->raw, beginInfo);
    }

    // Now that we can write to GPU data, prepare global frame constants.
    auto frameConstantsLayout = prepareFrameConstants(m_dynamicConstants);

    ExecutingRenderGraph executingRg;

    // Record and submit the main command buffer
    {
        auto mainCb = currentFrame.mainCommandBuffer;

        currentFrame.profilerData.beginFrame(rawDevice, mainCb.raw);

        {
            PUFFIN_SCOPE("rg begin_execute");

            executingRg = rg->beginExecute(
                RenderGraphExecutionParams{
                    .device = device,
                    .pipelineCache = m_pipelineCache,
                    .frameDescriptorSet = m_frameDescriptorSet,
                    .frameConstantsLayout = frameConstantsLayout,
                    .profilerData = currentFrame.profilerData
                },
                m_transientResourceCache,
                m_dynamicConstants
            );
        }

        // Record and submit the main command buffer
        {
            PUFFIN_SCOPE("main cb");

            {
                PUFFIN_SCOPE("rg::record_main_cb");
                executingRg.recordMainCb(mainCb);
            }

            rawDevice->endCommandBuffer(mainCb.raw);

            vk::SubmitInfo submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &mainCb.raw;

            rawDevice->resetFences(1, &mainCb.submitDoneFence);

            {
                PUFFIN_SCOPE("submit main cb");

                // Try to submit the command buffer to the GPU. We might encounter a GPU crash.
                rawDevice->queueSubmit(
                    device->universalQueue().raw,
                    1,
                    &submitInfo,
                    mainCb.submitDoneFence
                );
            }
        }
    }

    // Now that we've done the main submission and the GPU is busy, acquire the presentation image.
    // This can block, so we're doing it as late as possible.

    auto swapchainImage = swapchain.acquireNextImage();
    if (!swapchainImage) {
        TEKKI_LOG_ERROR("Failed to acquire swapchain image");
        return;
    }

    // Execute the rest of the render graph, and submit the presentation command buffer.
    auto retiredRg = [&]() -> RetiredRenderGraph {
        PUFFIN_SCOPE("presentation cb");

        auto presentationCb = currentFrame.presentationCommandBuffer;

        // Transition the swapchain to CS write
        backend::barrier::recordImageBarrier(
            device,
            presentationCb.raw,
            backend::barrier::ImageBarrier::new_(
                swapchainImage->image.raw,
                backend::AccessType::Present,
                backend::AccessType::ComputeShaderWrite,
                vk::ImageAspectFlagBits::eColor
            ).withDiscard(true)
        );

        auto retiredRg = executingRg.recordPresentationCb(presentationCb, swapchainImage->image);

        // Transition the swapchain to present
        backend::barrier::recordImageBarrier(
            device,
            presentationCb.raw,
            backend::barrier::ImageBarrier::new_(
                swapchainImage->image.raw,
                backend::AccessType::ComputeShaderWrite,
                backend::AccessType::Present,
                vk::ImageAspectFlagBits::eColor
            )
        );

        currentFrame.profilerData.endFrame(rawDevice, presentationCb.raw);

        // Record and submit the presentation command buffer
        {
            rawDevice->endCommandBuffer(presentationCb.raw);

            vk::SubmitInfo submitInfo{};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &swapchainImage->acquireSemaphore;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &swapchainImage->renderingFinishedSemaphore;
            submitInfo.pWaitDstStageMask = &vk::PipelineStageFlagBits::eComputeShader;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &presentationCb.raw;

            rawDevice->resetFences(1, &presentationCb.submitDoneFence);

            {
                PUFFIN_SCOPE("submit presentation cb");
                rawDevice->queueSubmit(
                    device->universalQueue().raw,
                    1,
                    &submitInfo,
                    presentationCb.submitDoneFence
                );
            }
        }

        swapchain.presentImage(*swapchainImage);

        return retiredRg;
    }();

    // Update temporal render graph state
    switch (m_temporalRgState.state) {
        case TemporalRgState::Inert:
            TEKKI_LOG_ERROR("Trying to retire the render graph, but it's inert. Was prepare_frame not called?");
            break;
        case TemporalRgState::Exported:
            m_temporalRgState.state = TemporalRgState::Inert;
            m_temporalRgState.inertState = m_temporalRgState.exportedState.retireTemporal(retiredRg);
            break;
    }

    retiredRg.releaseResources(m_transientResourceCache);

    m_dynamicConstants.advanceFrame();
    device->finishFrame(currentFrame);
}

template<typename PrepareRenderGraphFn>
Result<void> Renderer::prepareFrame(PrepareRenderGraphFn prepareRenderGraph) {
    auto rg = TemporalRenderGraph::new_(
        m_temporalRgState.state == TemporalRgState::Inert
            ? m_temporalRgState.inertState.cloneAssumingInert()
            : TemporalRenderGraphState{},
        m_device
    );

    rg.predefinedDescriptorSetLayouts().insert(
        2,
        PredefinedDescriptorSet{
            .bindings = FRAME_CONSTANTS_LAYOUT
        }
    );

    prepareRenderGraph(rg);
    auto [compiledRg, temporalRgState] = rg.exportTemporal();

    m_compiledRg = compiledRg.compile(m_pipelineCache);

    auto prepareResult = m_pipelineCache.prepareFrame(m_device);
    if (prepareResult.isOk()) {
        // If the frame preparation succeeded, update stored temporal rg state and finish
        m_temporalRgState.state = TemporalRgState::Exported;
        m_temporalRgState.exportedState = std::move(temporalRgState);
        return {};
    } else {
        // If frame preparation failed, we're not going to render anything, but we've potentially created
        // some temporal resources, and we can reuse them in the next attempt.
        //
        // Import any new resources into our temporal rg state, but reset their access modes.

        auto& selfTemporalRgState = m_temporalRgState.inertState;

        for (auto& [resKey, res] : temporalRgState.resources()) {
            if (!selfTemporalRgState.resources().contains(resKey)) {
                auto newRes = std::visit(overloaded{
                    [](TemporalResourceState::Inert res) -> TemporalResourceState {
                        return res;
                    },
                    [](TemporalResourceState::Imported res) -> TemporalResourceState {
                        return TemporalResourceState::Inert{
                            .resource = res.resource,
                            .accessType = backend::AccessType::Nothing
                        };
                    },
                    [](TemporalResourceState::Exported res) -> TemporalResourceState {
                        return TemporalResourceState::Inert{
                            .resource = res.resource,
                            .accessType = backend::AccessType::Nothing
                        };
                    }
                }, res);

                selfTemporalRgState.resources().insert(resKey, std::move(newRes));
            }
        }

        return prepareResult.error();
    }
}

vk::DescriptorSet Renderer::createFrameDescriptorSet(backend::RenderBackend& backend, backend::Buffer& dynamicConstants) {
    auto device = backend.device()->raw();

    std::array<vk::DescriptorBindingFlags, 3> setBindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound,
        vk::DescriptorBindingFlagBits::ePartiallyBound,
        vk::DescriptorBindingFlagBits::ePartiallyBound
    };

    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{};
    bindingFlagsCreateInfo.bindingCount = setBindingFlags.size();
    bindingFlagsCreateInfo.pBindingFlags = setBindingFlags.data();

    std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {
        // frame_constants
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll
        },
        // instance_dynamic_parameters
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll
        },
        // triangle_lights_dyn
        vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.bindingCount = bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.pNext = &bindingFlagsCreateInfo;

    auto descriptorSetLayout = device.createDescriptorSetLayout(layoutCreateInfo);

    std::array<vk::DescriptorPoolSize, 2> descriptorSizes = {
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBufferDynamic,
            .descriptorCount = 1
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 2
        }
    };

    vk::DescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.poolSizeCount = descriptorSizes.size();
    poolCreateInfo.pPoolSizes = descriptorSizes.data();
    poolCreateInfo.maxSets = 1;

    auto descriptorPool = device.createDescriptorPool(poolCreateInfo);

    vk::DescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &descriptorSetLayout;

    auto sets = device.allocateDescriptorSets(allocateInfo);
    auto set = sets[0];

    // Update descriptor set
    {
        vk::DescriptorBufferInfo uniformBufferInfo{
            .buffer = dynamicConstants.raw(),
            .range = backend::MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH
        };

        vk::DescriptorBufferInfo storageBufferInfo{
            .buffer = dynamicConstants.raw(),
            .range = backend::MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES
        };

        std::array<vk::WriteDescriptorSet, 3> descriptorWrites = {
            // `frame_constants`
            vk::WriteDescriptorSet{
                .dstSet = set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                .pBufferInfo = &uniformBufferInfo
            },
            // `instance_dynamic_parameters_dyn`
            vk::WriteDescriptorSet{
                .dstSet = set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
                .pBufferInfo = &storageBufferInfo
            },
            // `triangle_lights_dyn`
            vk::WriteDescriptorSet{
                .dstSet = set,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
                .pBufferInfo = &storageBufferInfo
            }
        };

        device.updateDescriptorSets(descriptorWrites, {});
    }

    return set;
}

} // namespace render_graph
} // namespace tekki