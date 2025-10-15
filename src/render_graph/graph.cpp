#include "tekki/render_graph/graph.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <string>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <variant>
#include <optional>
#include <spdlog/spdlog.h>
#include "tekki/core/result.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/resource_registry.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/renderer/FrameConstantsLayout.h"

namespace tekki::render_graph {

PassResourceAccessType::PassResourceAccessType(vk_sync::AccessType accessType, PassResourceAccessSyncType syncType)
    : AccessType(accessType), SyncType(syncType) {}

RecordedPass::RecordedPass(const std::string& name, std::size_t idx)
    : Name(name), Idx(idx) {}

RenderGraph::RenderGraph()
    : Passes(), Resources(), ExportedResources(), ComputePipelines(), RasterPipelines(), 
      RtPipelines(), PredefinedDescriptorSetLayouts(), DebugHook(std::nullopt), 
      DebuggedResource(std::nullopt) {}

GraphRawResourceHandle RenderGraph::CreateRawResource(const GraphResourceCreateInfo& info) {
    GraphRawResourceHandle res;
    res.id = static_cast<uint32_t>(Resources.size());
    res.version = 0;

    Resources.push_back(GraphResourceInfo{GraphResourceCreateInfo{info.Desc}});
    return res;
}

Handle<Image> RenderGraph::GetSwapChain() {
    GraphRawResourceHandle res;
    res.id = static_cast<uint32_t>(Resources.size());
    res.version = 0;

    Resources.push_back(GraphResourceInfo{GraphResourceImportInfo::Swapchain()});

    ImageDesc desc = ImageDesc::New2d(VK_FORMAT_R8G8B8A8_UNORM, glm::u32vec2(1, 1));

    return Handle<Image>{res, desc};
}

PassBuilder RenderGraph::AddPass(const std::string& name) {
    std::size_t passIdx = Passes.size();
    return PassBuilder(*this, name, passIdx);
}

ResourceInfo RenderGraph::CalculateResourceInfo() const {
    std::vector<ResourceLifetime> lifetimes;
    std::vector<vk::ImageUsageFlags> imageUsageFlags(Resources.size());
    std::vector<vk::BufferUsageFlags> bufferUsageFlags(Resources.size());

    for (const auto& resource : Resources) {
        ResourceLifetime lifetime;
        if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
            lifetime.LastAccess = 0;
        }
        lifetimes.push_back(lifetime);
    }

    for (std::size_t resIdx = 0; resIdx < Resources.size(); ++resIdx) {
        const auto& resource = Resources[resIdx];
        if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
            const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);
            if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                const auto& desc = std::get<ImageDesc>(createInfo.Desc);
                imageUsageFlags[resIdx] = vk::ImageUsageFlags(desc.Usage);
            } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                const auto& desc = std::get<BufferDesc>(createInfo.Desc);
                bufferUsageFlags[resIdx] = vk::BufferUsageFlags(desc.usage);
            }
        }
    }

    for (std::size_t passIdx = 0; passIdx < Passes.size(); ++passIdx) {
        const auto& pass = Passes[passIdx];

        for (const auto& resAccess : pass.Read) {
            std::size_t resourceIndex = resAccess.Handle.id;
            auto& lifetime = lifetimes[resourceIndex];
            lifetime.LastAccess = std::max(lifetime.LastAccess.value_or(passIdx), passIdx);

            auto accessMask = vk_sync::get_access_info(resAccess.Access.AccessType).access_mask;

            const auto& resource = Resources[resourceIndex];
            if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
                const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);
                if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
                if (std::holds_alternative<GraphResourceImportInfo::ImageImport>(importInfo.data) ||
                    std::holds_alternative<GraphResourceImportInfo::SwapchainImage>(importInfo.data)) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (std::holds_alternative<GraphResourceImportInfo::BufferImport>(importInfo.data)) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            }
        }

        for (const auto& resAccess : pass.Write) {
            std::size_t resourceIndex = resAccess.Handle.id;
            auto& lifetime = lifetimes[resourceIndex];
            lifetime.LastAccess = std::max(lifetime.LastAccess.value_or(passIdx), passIdx);

            auto accessMask = vk_sync::get_access_info(resAccess.Access.AccessType).access_mask;

            const auto& resource = Resources[resourceIndex];
            if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
                const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);
                if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
                if (std::holds_alternative<GraphResourceImportInfo::ImageImport>(importInfo.data) ||
                    std::holds_alternative<GraphResourceImportInfo::SwapchainImage>(importInfo.data)) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (std::holds_alternative<GraphResourceImportInfo::BufferImport>(importInfo.data)) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            }
        }
    }

    for (const auto& [exportableResource, accessType] : ExportedResources) {
        std::size_t rawId = exportableResource.GetRaw().id;

        lifetimes[rawId].LastAccess = Passes.size() > 0 ? Passes.size() - 1 : 0;

        if (accessType != vk_sync::AccessType::None) {
            auto accessMask = vk_sync::get_access_info(accessType).access_mask;

            if (exportableResource.GetType() == ExportableGraphResource::Type::Image) {
                imageUsageFlags[rawId] |= ImageAccessMaskToUsageFlags(accessMask);
            } else if (exportableResource.GetType() == ExportableGraphResource::Type::Buffer) {
                bufferUsageFlags[rawId] |= BufferAccessMaskToUsageFlags(accessMask);
            }
        }
    }

    return ResourceInfo{lifetimes, imageUsageFlags, bufferUsageFlags};
}

std::optional<PendingDebugPass> RenderGraph::HookDebugPass(const RecordedPass& pass) {
    if (!DebugHook.has_value()) {
        return std::nullopt;
    }

    const auto& scopeHook = DebugHook.value().RenderDebugHook;
    if (pass.Name != scopeHook.Name || pass.Idx != scopeHook.Id) {
        return std::nullopt;
    }

    auto isDebugCompatible = [](const ImageDesc& desc) -> bool {
        return vk_sync::image_aspect_mask_from_format(desc.Format) == VK_IMAGE_ASPECT_COLOR_BIT &&
               desc.Type == ImageType::Tex2d;
    };

    for (const auto& srcRef : pass.Write) {
        const auto& resource = Resources[srcRef.Handle.id];

        if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
            const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);
            if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                const auto& imgDesc = std::get<ImageDesc>(createInfo.Desc);
                if (isDebugCompatible(imgDesc)) {
                    ImageDesc modifiedDesc = imgDesc;
                    modifiedDesc.MipLevels = 1;
                    modifiedDesc.Format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

                    Handle<Image> srcHandle{srcRef.Handle, modifiedDesc};
                    return PendingDebugPass{srcHandle};
                }
            }
        } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
            const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
            if (std::holds_alternative<GraphResourceImportInfo::ImageImport>(importInfo.data)) {
                const auto& imageImport = std::get<GraphResourceImportInfo::ImageImport>(importInfo.data);
                const auto& img = imageImport.resource;

                // Imported resources must also support SAMPLED usage
                if (img->desc.Usage & VK_IMAGE_USAGE_SAMPLED_BIT && isDebugCompatible(img->desc)) {
                    ImageDesc modifiedDesc = img->desc;
                    modifiedDesc.MipLevels = 1;
                    modifiedDesc.Format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

                    Handle<Image> srcHandle{srcRef.Handle, modifiedDesc};
                    return PendingDebugPass{srcHandle};
                }
            }
        }
    }

    return std::nullopt;
}

void RenderGraph::RecordPass(const RecordedPass& pass) {
    auto debugPass = HookDebugPass(pass);
    Passes.push_back(pass);
    
    if (debugPass.has_value()) {
        // Implementation for debug pass would go here
        // This is simplified for the translation
    }
}

CompiledRenderGraph RenderGraph::Compile(PipelineCache* pipelineCache) {
    auto resourceInfo = CalculateResourceInfo();
    
    std::vector<ComputePipelineHandle> computePipelines;
    for (const auto& pipeline : ComputePipelines) {
        computePipelines.push_back(pipelineCache->RegisterCompute(pipeline.Desc));
    }
    
    std::vector<RasterPipelineHandle> rasterPipelines;
    for (const auto& pipeline : RasterPipelines) {
        rasterPipelines.push_back(pipelineCache->RegisterRaster(pipeline.Shaders, pipeline.Desc));
    }
    
    std::vector<RtPipelineHandle> rtPipelines;
    for (const auto& pipeline : RtPipelines) {
        rtPipelines.push_back(pipelineCache->RegisterRayTracing(pipeline.Shaders, pipeline.Desc));
    }
    
    RenderGraphPipelines pipelines{
        std::move(computePipelines),
        std::move(rasterPipelines),
        std::move(rtPipelines)
    };
    
    return CompiledRenderGraph(std::move(*this), resourceInfo, std::move(pipelines));
}

CompiledRenderGraph::CompiledRenderGraph(RenderGraph&& rg, const ResourceInfo& resourceInfo, RenderGraphPipelines&& pipelines)
    : Rg(std::move(rg)), ResourceInfo_(resourceInfo), Pipelines(std::move(pipelines)) {}

ExecutingRenderGraph CompiledRenderGraph::BeginExecute(const RenderGraphExecutionParams& params, TransientResourceCache* transientResourceCache, DynamicConstants* dynamicConstants) {
    std::vector<RegistryResource> resources;

    for (std::size_t resourceIdx = 0; resourceIdx < Rg.Resources.size(); ++resourceIdx) {
        const auto& resource = Rg.Resources[resourceIdx];

        if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
            const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);

            if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                auto desc = std::get<ImageDesc>(createInfo.Desc);
                desc.Usage = static_cast<VkImageUsageFlags>(ResourceInfo_.ImageUsageFlags[resourceIdx]);

                std::shared_ptr<Image> image;
                try {
                    image = transientResourceCache->GetImage(desc);
                    if (!image) {
                        // Create backend image directly
                        image = params.Device->CreateImage(desc, std::vector<uint8_t>{});
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to create image: " + std::string(e.what()));
                }

                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::OwnedImage(image);
                reg_res.AccessType = vk_sync::AccessType::None;
                resources.push_back(std::move(reg_res));
            } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                auto desc = std::get<BufferDesc>(createInfo.Desc);
                desc.usage = static_cast<VkBufferUsageFlags>(ResourceInfo_.BufferUsageFlags[resourceIdx]);

                std::shared_ptr<Buffer> buffer;
                try {
                    buffer = transientResourceCache->GetBuffer(desc);
                    if (!buffer) {
                        // Create backend buffer directly - need to wrap in shared_ptr
                        auto rawBuffer = params.Device->CreateBuffer(desc, "rg buffer", {});
                        buffer = std::make_shared<Buffer>(std::move(rawBuffer));
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to create buffer: " + std::string(e.what()));
                }

                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::OwnedBuffer(buffer);
                reg_res.AccessType = vk_sync::AccessType::None;
                resources.push_back(std::move(reg_res));
            }
        } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
            const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);

            if (std::holds_alternative<GraphResourceImportInfo::ImageImport>(importInfo.data)) {
                const auto& imageImport = std::get<GraphResourceImportInfo::ImageImport>(importInfo.data);
                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::ImportedImage(imageImport.resource);
                reg_res.AccessType = imageImport.access_type;
                resources.push_back(std::move(reg_res));
            } else if (std::holds_alternative<GraphResourceImportInfo::BufferImport>(importInfo.data)) {
                const auto& bufferImport = std::get<GraphResourceImportInfo::BufferImport>(importInfo.data);
                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::ImportedBuffer(bufferImport.resource);
                reg_res.AccessType = bufferImport.access_type;
                resources.push_back(std::move(reg_res));
            } else if (std::holds_alternative<GraphResourceImportInfo::RayTracingAccelerationImport>(importInfo.data)) {
                const auto& rtImport = std::get<GraphResourceImportInfo::RayTracingAccelerationImport>(importInfo.data);
                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::ImportedRayTracingAcceleration(rtImport.resource);
                reg_res.AccessType = rtImport.access_type;
                resources.push_back(std::move(reg_res));
            } else if (std::holds_alternative<GraphResourceImportInfo::SwapchainImage>(importInfo.data)) {
                RegistryResource reg_res;
                reg_res.Resource = AnyRenderResource::Pending(PendingRenderResourceInfo{std::make_shared<GraphResourceInfo>(resource)});
                reg_res.AccessType = vk_sync::AccessType::ComputeShaderWrite;
                resources.push_back(std::move(reg_res));
            }
        }
    }

    ResourceRegistry resourceRegistry{
        &params,
        std::move(resources),
        dynamicConstants,
        &Pipelines
    };

    return ExecutingRenderGraph(
        std::deque<RecordedPass>(Rg.Passes.begin(), Rg.Passes.end()),
        std::vector<GraphResourceInfo>(Rg.Resources.begin(), Rg.Resources.end()),
        std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>>(Rg.ExportedResources.begin(), Rg.ExportedResources.end()),
        std::move(resourceRegistry)
    );
}

ExecutingRenderGraph::ExecutingRenderGraph(std::deque<RecordedPass>&& passes, std::vector<GraphResourceInfo>&& resources, std::vector<std::pair<ExportableGraphResource, vk_sync::AccessType>>&& exportedResources, ResourceRegistry&& resourceRegistry)
    : Passes(std::move(passes)), Resources(std::move(resources)), ExportedResources(std::move(exportedResources)), ResourceRegistry_(std::move(resourceRegistry)) {}

void ExecutingRenderGraph::RecordMainCb(const CommandBuffer& cb) {
    std::size_t firstPresentationPass = Passes.size();

    for (std::size_t passIdx = 0; passIdx < Passes.size(); ++passIdx) {
        const auto& pass = Passes[passIdx];
        for (const auto& res : pass.Write) {
            const auto& resource = Resources[res.Handle.id];
            if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
                if (std::holds_alternative<GraphResourceImportInfo::SwapchainImage>(importInfo.data)) {
                    firstPresentationPass = passIdx;
                    break;
                }
            }
        }
    }

    std::vector<RecordedPass> passes(Passes.begin(), Passes.end());
    Passes.clear();

    std::unordered_map<std::uint32_t, PassResourceAccessType*> resourceFirstAccessStates;

    for (std::size_t i = 0; i < firstPresentationPass; ++i) {
        auto& pass = passes[i];
        for (auto& resourceRef : pass.Read) {
            resourceFirstAccessStates[resourceRef.Handle.id] = &resourceRef.Access;
        }
        for (auto& resourceRef : pass.Write) {
            resourceFirstAccessStates[resourceRef.Handle.id] = &resourceRef.Access;
        }
    }

    const auto& params = ResourceRegistry_.ExecutionParams;
    for (auto& [resourceIdx, access] : resourceFirstAccessStates) {
        auto& resource = ResourceRegistry_.Resources[resourceIdx];
        TransitionResource(
            params->Device,
            cb,
            &resource,
            PassResourceAccessType{access->AccessType, PassResourceAccessSyncType::SkipSyncIfSameAccessType},
            false,
            ""
        );

        access->SyncType = PassResourceAccessSyncType::SkipSyncIfSameAccessType;
    }

    for (std::size_t i = 0; i < firstPresentationPass; ++i) {
        RecordPassCb(passes[i], &ResourceRegistry_, cb);
    }

    Passes = std::deque<RecordedPass>(passes.begin() + firstPresentationPass, passes.end());
}

RetiredRenderGraph ExecutingRenderGraph::RecordPresentationCb(const CommandBuffer& cb, const std::shared_ptr<Image>& swapchainImage) {
    const auto& params = ResourceRegistry_.ExecutionParams;

    // Transition exported images to the requested access types
    for (const auto& [resourceIdx, accessType] : ExportedResources) {
        if (accessType != vk_sync::AccessType::None) {
            std::size_t rawId = resourceIdx.GetRaw().id;
            auto& resource = ResourceRegistry_.Resources[rawId];
            TransitionResource(
                params->Device,
                cb,
                &resource,
                PassResourceAccessType{accessType, PassResourceAccessSyncType::AlwaysSync},
                false,
                ""
            );
        }
    }

    // Resolve pending resources (swapchain)
    for (auto& res : ResourceRegistry_.Resources) {
        if (std::holds_alternative<AnyRenderResource::Pending>(res.Resource)) {
            auto& pending = std::get<AnyRenderResource::Pending>(res.Resource);
            if (std::holds_alternative<GraphResourceImportInfo>(pending.Resource->Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(pending.Resource->Info);
                if (std::holds_alternative<GraphResourceImportInfo::SwapchainImage>(importInfo.data)) {
                    res.Resource = AnyRenderResource::ImportedImage(swapchainImage);
                } else {
                    throw std::runtime_error("Only swapchain can be currently pending");
                }
            }
        }
    }

    auto passes = std::move(Passes);
    for (const auto& pass : passes) {
        RecordPassCb(pass, &ResourceRegistry_, cb);
    }

    return RetiredRenderGraph(std::move(ResourceRegistry_.Resources));
}

void ExecutingRenderGraph::RecordPassCb(const RecordedPass& pass, ResourceRegistry* resourceRegistry, const CommandBuffer& cb) {
    const auto& params = resourceRegistry->ExecutionParams;

    try {
        // RecordCrashMarker and backend CommandBuffer commented out
        // backend::vulkan::CommandBuffer backendCb(params->Device->GetRaw(), params->Device->GetUniversalQueue().Family);
        // params->Device->RecordCrashMarker(cb, "begin render pass " + pass.Name);

        if (auto debugUtils = params->Device->DebugUtils()) {
            debugUtils->CmdBeginDebugUtilsLabel(cb.Raw, pass.Name);
        }

        // GPU profiling scope - commented out until profiler is implemented
        // auto queryId = kajiya_backend::gpu_profiler::Profiler().CreateScope(pass.Name);
        // auto vkScope = const_cast<VkProfilerData*>(params->ProfilerData)->BeginScope(params->Device->GetRaw(), cb.Raw, queryId);

        std::vector<std::pair<std::size_t, PassResourceAccessType>> transitions;
        for (const auto& resourceRef : pass.Read) {
            transitions.emplace_back(resourceRef.Handle.id, resourceRef.Access);
        }
        for (const auto& resourceRef : pass.Write) {
            transitions.emplace_back(resourceRef.Handle.id, resourceRef.Access);
        }

        for (const auto& [resourceIdx, access] : transitions) {
            auto& resource = resourceRegistry->Resources[resourceIdx];
            TransitionResource(params->Device, cb, &resource, access, false, "");
        }

        RenderPassApi api{cb, resourceRegistry};
        if (pass.RenderFn) {
            pass.RenderFn(&api);
        }

        // End profiling scope
        // const_cast<VkProfilerData*>(params->ProfilerData)->EndScope(params->Device->GetRaw(), cb.Raw, vkScope);

        if (auto debugUtils = params->Device->DebugUtils()) {
            debugUtils->CmdEndDebugUtilsLabel(cb.Raw);
        }

        // params->Device->RecordCrashMarker(cb, "end render pass " + pass.Name);
    } catch (const std::exception& e) {
        throw std::runtime_error("Pass " + pass.Name + " failed to render: " + std::string(e.what()));
    }
}

void ExecutingRenderGraph::TransitionResource(Device* device, const CommandBuffer& cb, RegistryResource* resource, const PassResourceAccessType& access, bool debug, [[maybe_unused]] const std::string& dbgStr) {
    if (RGAllowPassOverlap && resource->AccessType == access.AccessType &&
        access.SyncType == PassResourceAccessSyncType::SkipSyncIfSameAccessType) {
        return;
    }

    if (debug) {
        // Log transition for debugging - commented out due to fmt compilation issues
        // spdlog::info("\t{}: {:?} -> {:?}", dbgStr,
        //     static_cast<int>(resource->AccessType),
        //     static_cast<int>(access.AccessType));
    }

    if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource->Resource) ||
        std::holds_alternative<AnyRenderResource::ImportedImage>(resource->Resource)) {

        std::shared_ptr<Image> image;
        if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource->Resource)) {
            image = std::get<AnyRenderResource::OwnedImage>(resource->Resource).resource;
        } else {
            image = std::get<AnyRenderResource::ImportedImage>(resource->Resource).resource;
        }

        if (debug) {
            // Can't use fmt on ImageDesc without a formatter
            // spdlog::info("\t(image {:?})", image->desc);
        }

        auto aspectMask = vk_sync::image_aspect_mask_from_access_type_and_format(
            access.AccessType, image->desc.Format);

        if (!aspectMask) {
            throw std::runtime_error("Invalid image access");
        }

        vk_sync::record_image_barrier(
            device->GetRaw(),
            cb.Raw,
            vk_sync::ImageBarrier{
                image->Raw,
                resource->AccessType,
                access.AccessType,
                aspectMask.value()
            }
        );

        resource->AccessType = access.AccessType;
    } else if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(resource->Resource) ||
               std::holds_alternative<AnyRenderResource::ImportedBuffer>(resource->Resource)) {

        std::shared_ptr<Buffer> buffer;
        if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(resource->Resource)) {
            buffer = std::get<AnyRenderResource::OwnedBuffer>(resource->Resource).resource;
        } else {
            buffer = std::get<AnyRenderResource::ImportedBuffer>(resource->Resource).resource;
        }

        if (debug) {
            // Can't use fmt on BufferDesc without a formatter
            // spdlog::info("\t(buffer {:?})", buffer->desc);
        }

        vk_sync::BufferBarrier barrier;
        barrier.prev_access = resource->AccessType;
        barrier.next_access = access.AccessType;
        barrier.src_queue_family_index = device->GetUniversalQueue().Family.index;
        barrier.dst_queue_family_index = device->GetUniversalQueue().Family.index;
        barrier.buffer = buffer->Raw;
        barrier.offset = 0;
        barrier.size = buffer->desc.size;

        vk_sync::cmd::pipeline_barrier(
            device->GetRaw(),
            cb.Raw,
            std::nullopt,
            std::vector<vk_sync::BufferBarrier>{barrier},
            {}
        );

        resource->AccessType = access.AccessType;
    } else if (std::holds_alternative<AnyRenderResource::ImportedRayTracingAcceleration>(resource->Resource)) {
        if (debug) {
            spdlog::info("\t(bvh)");
        }
        // TODO: Implement proper global barrier for ray tracing acceleration structures
        resource->AccessType = access.AccessType;
    }
}

// Helper function for global barriers (currently unused but kept for compatibility)
void GlobalBarrier(Device* device, const CommandBuffer& cb,
                   const std::vector<vk_sync::AccessType>& previousAccesses,
                   const std::vector<vk_sync::AccessType>& nextAccesses) {
    vk_sync::GlobalBarrier globalBarrier{previousAccesses, nextAccesses};
    vk_sync::cmd::pipeline_barrier(
        device->GetRaw(),
        cb.Raw,
        std::optional<vk_sync::GlobalBarrier>(globalBarrier),
        std::vector<vk_sync::BufferBarrier>{},
        std::vector<vk_sync::ImageBarrier>{}
    );
}

RetiredRenderGraph::RetiredRenderGraph(std::vector<RegistryResource>&& resources)
    : Resources(std::move(resources)) {}

// Helper to extract resource pointer from variant
template<typename Res>
static const Res* BorrowResourceFromVariant(const RegistryResource& regResource) {
    if (std::holds_alternative<AnyRenderResource::OwnedImage>(regResource.Resource)) {
        if constexpr (std::is_same_v<Res, Image>) {
            return std::get<AnyRenderResource::OwnedImage>(regResource.Resource).resource.get();
        }
    } else if (std::holds_alternative<AnyRenderResource::ImportedImage>(regResource.Resource)) {
        if constexpr (std::is_same_v<Res, Image>) {
            return std::get<AnyRenderResource::ImportedImage>(regResource.Resource).resource.get();
        }
    } else if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(regResource.Resource)) {
        if constexpr (std::is_same_v<Res, Buffer>) {
            return std::get<AnyRenderResource::OwnedBuffer>(regResource.Resource).resource.get();
        }
    } else if (std::holds_alternative<AnyRenderResource::ImportedBuffer>(regResource.Resource)) {
        if constexpr (std::is_same_v<Res, Buffer>) {
            return std::get<AnyRenderResource::ImportedBuffer>(regResource.Resource).resource.get();
        }
    }
    return nullptr;
}

template<typename Res>
std::pair<const Res*, vk_sync::AccessType> RetiredRenderGraph::ExportedResource(const ExportedHandle<Res>& handle) const {
    const auto& regResource = Resources[handle.id];
    return std::make_pair(
        BorrowResourceFromVariant<Res>(regResource),
        regResource.AccessType
    );
}

// Explicit template instantiations
template std::pair<const Image*, vk_sync::AccessType> RetiredRenderGraph::ExportedResource<Image>(const ExportedHandle<Image>&) const;
template std::pair<const Buffer*, vk_sync::AccessType> RetiredRenderGraph::ExportedResource<Buffer>(const ExportedHandle<Buffer>&) const;

void RetiredRenderGraph::ReleaseResources(TransientResourceCache* transientResourceCache) {
    for (auto& resource : Resources) {
        if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource.Resource)) {
            transientResourceCache->InsertImage(std::get<AnyRenderResource::OwnedImage>(resource.Resource).resource);
        } else if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(resource.Resource)) {
            transientResourceCache->InsertBuffer(std::get<AnyRenderResource::OwnedBuffer>(resource.Resource).resource);
        } else if (std::holds_alternative<AnyRenderResource::Pending>(resource.Resource)) {
            throw std::runtime_error("RetiredRenderGraph::release_resources called while a resource was in Pending state");
        }
    }
}

vk::ImageUsageFlags ImageAccessMaskToUsageFlags(VkAccessFlags accessMask) {
    // Convert VkAccessFlags to uint32_t for comparison
    uint32_t mask = static_cast<uint32_t>(accessMask);

    if (mask & VK_ACCESS_SHADER_READ_BIT) {
        return vk::ImageUsageFlagBits::eSampled;
    }
    if (mask & VK_ACCESS_SHADER_WRITE_BIT) {
        return vk::ImageUsageFlagBits::eStorage;
    }
    if (mask & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
        return vk::ImageUsageFlagBits::eColorAttachment;
    }
    if (mask & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
        return vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    if (mask & VK_ACCESS_TRANSFER_READ_BIT) {
        return vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (mask & VK_ACCESS_TRANSFER_WRITE_BIT) {
        return vk::ImageUsageFlagBits::eTransferDst;
    }
    if (mask & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)) {
        return vk::ImageUsageFlagBits::eStorage;
    }

    throw std::runtime_error("Invalid image access mask");
}

vk::BufferUsageFlags BufferAccessMaskToUsageFlags(VkAccessFlags accessMask) {
    // Convert VkAccessFlags to uint32_t for comparison
    uint32_t mask = static_cast<uint32_t>(accessMask);

    if (mask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
        return vk::BufferUsageFlagBits::eIndirectBuffer;
    }
    if (mask & VK_ACCESS_INDEX_READ_BIT) {
        return vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if (mask & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) {
        return vk::BufferUsageFlagBits::eVertexBuffer;
    }
    if (mask & VK_ACCESS_UNIFORM_READ_BIT) {
        return vk::BufferUsageFlagBits::eUniformBuffer;
    }
    if (mask & VK_ACCESS_SHADER_READ_BIT) {
        return vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (mask & VK_ACCESS_SHADER_WRITE_BIT) {
        return vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (mask & VK_ACCESS_TRANSFER_READ_BIT) {
        return vk::BufferUsageFlagBits::eTransferSrc;
    }
    if (mask & VK_ACCESS_TRANSFER_WRITE_BIT) {
        return vk::BufferUsageFlagBits::eTransferDst;
    }
    if (mask & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)) {
        return vk::BufferUsageFlagBits::eStorageBuffer;
    }

    throw std::runtime_error("Invalid buffer access mask");
}

bool RGAllowPassOverlap = true;

// Template implementations for Import/Export

// Import for Image
template<>
Handle<Image> RenderGraph::Import<Image>(const std::shared_ptr<Image>& resource, vk_sync::AccessType accessTypeAtImportTime) {
    GraphRawResourceHandle res;
    res.id = static_cast<uint32_t>(Resources.size());
    res.version = 0;

    ImageDesc desc = resource->desc;

    Resources.push_back(GraphResourceInfo{GraphResourceImportInfo::Image(resource, accessTypeAtImportTime)});

    return Handle<Image>{res, desc};
}

// Import for Buffer
template<>
Handle<Buffer> RenderGraph::Import<Buffer>(const std::shared_ptr<Buffer>& resource, vk_sync::AccessType accessTypeAtImportTime) {
    GraphRawResourceHandle res;
    res.id = static_cast<uint32_t>(Resources.size());
    res.version = 0;

    BufferDesc desc = resource->desc;

    Resources.push_back(GraphResourceInfo{GraphResourceImportInfo::Buffer(resource, accessTypeAtImportTime)});

    return Handle<Buffer>{res, desc};
}

// Import for RayTracingAcceleration
template<>
Handle<RayTracingAcceleration> RenderGraph::Import<RayTracingAcceleration>(const std::shared_ptr<RayTracingAcceleration>& resource, vk_sync::AccessType accessTypeAtImportTime) {
    GraphRawResourceHandle res;
    res.id = static_cast<uint32_t>(Resources.size());
    res.version = 0;

    RayTracingAccelerationDesc desc;

    Resources.push_back(GraphResourceInfo{GraphResourceImportInfo::RayTracingAcceleration(resource, accessTypeAtImportTime)});

    return Handle<RayTracingAcceleration>{res, desc};
}

// Export for Image
template<>
ExportedHandle<Image> RenderGraph::Export<Image>(const Handle<Image>& resource, vk_sync::AccessType accessType) {
    ExportedHandle<Image> res;
    res.id = resource.raw.id;

    ExportedResources.push_back(std::make_pair(ExportableGraphResource(resource), accessType));

    return res;
}

// Export for Buffer
template<>
ExportedHandle<Buffer> RenderGraph::Export<Buffer>(const Handle<Buffer>& resource, vk_sync::AccessType accessType) {
    ExportedHandle<Buffer> res;
    res.id = resource.raw.id;

    ExportedResources.push_back(std::make_pair(ExportableGraphResource(resource), accessType));

    return res;
}

// Export for RayTracingAcceleration
template<>
ExportedHandle<RayTracingAcceleration> RenderGraph::Export<RayTracingAcceleration>([[maybe_unused]] const Handle<RayTracingAcceleration>& resource, [[maybe_unused]] vk_sync::AccessType accessType) {
    // TODO: implement
    throw std::runtime_error("Export for RayTracingAcceleration not yet implemented");
}

// Template implementation for Create
template<typename Desc>
Handle<typename Desc::Resource> RenderGraph::Create(const Desc& desc) {
    GraphRawResourceHandle raw = CreateRawResource(GraphResourceCreateInfo{GraphResourceDesc(desc)});

    return Handle<typename Desc::Resource>{raw, desc};
}

}  // namespace tekki::render_graph