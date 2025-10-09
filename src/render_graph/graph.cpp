```cpp
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
    res.Id = Resources.size();
    res.Version = 0;
    
    Resources.push_back(GraphResourceInfo{GraphResourceCreateInfo{info.Desc}});
    return res;
}

Handle<Image> RenderGraph::GetSwapChain() {
    GraphRawResourceHandle res;
    res.Id = Resources.size();
    res.Version = 0;
    
    Resources.push_back(GraphResourceInfo{GraphResourceImportInfo::SwapchainImage});
    
    ImageDesc desc;
    desc.Format = vk::Format::eR8G8B8A8Unorm;
    desc.Extent = {1, 1, 1};
    desc.ImageType = ImageType::Tex2d;
    
    return Handle<Image>{res, desc};
}

PassBuilder RenderGraph::AddPass(const std::string& name) {
    std::size_t passIdx = Passes.size();
    return PassBuilder(this, name, passIdx);
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
                imageUsageFlags[resIdx] = desc.Usage;
            } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                const auto& desc = std::get<BufferDesc>(createInfo.Desc);
                bufferUsageFlags[resIdx] = desc.Usage;
            }
        }
    }
    
    for (std::size_t passIdx = 0; passIdx < Passes.size(); ++passIdx) {
        const auto& pass = Passes[passIdx];
        
        for (const auto& resAccess : pass.Read) {
            std::size_t resourceIndex = resAccess.Handle.Id;
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
                if (importInfo == GraphResourceImportInfo::Image || 
                    importInfo == GraphResourceImportInfo::SwapchainImage) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (importInfo == GraphResourceImportInfo::Buffer) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            }
        }
        
        for (const auto& resAccess : pass.Write) {
            std::size_t resourceIndex = resAccess.Handle.Id;
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
                if (importInfo == GraphResourceImportInfo::Image || 
                    importInfo == GraphResourceImportInfo::SwapchainImage) {
                    imageUsageFlags[resourceIndex] |= ImageAccessMaskToUsageFlags(accessMask);
                } else if (importInfo == GraphResourceImportInfo::Buffer) {
                    bufferUsageFlags[resourceIndex] |= BufferAccessMaskToUsageFlags(accessMask);
                }
            }
        }
    }
    
    for (const auto& [exportableResource, accessType] : ExportedResources) {
        std::size_t rawId = 0;
        if (exportableResource == ExportableGraphResource::Image) {
            // Get handle from exported resources
        } else if (exportableResource == ExportableGraphResource::Buffer) {
            // Get handle from exported resources
        }
        
        lifetimes[rawId].LastAccess = Passes.size() > 0 ? Passes.size() - 1 : 0;
        
        if (accessType != vk_sync::AccessType::Nothing) {
            auto accessMask = vk_sync::get_access_info(accessType).access_mask;
            
            if (exportableResource == ExportableGraphResource::Image) {
                imageUsageFlags[rawId] |= ImageAccessMaskToUsageFlags(accessMask);
            } else if (exportableResource == ExportableGraphResource::Buffer) {
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
        return vk_sync::image_aspect_mask_from_format(desc.Format) == vk::ImageAspectFlagBits::eColor &&
               desc.ImageType == ImageType::Tex2d;
    };
    
    for (const auto& srcRef : pass.Write) {
        const auto& resource = Resources[srcRef.Handle.Id];
        
        if (std::holds_alternative<GraphResourceCreateInfo>(resource.Info)) {
            const auto& createInfo = std::get<GraphResourceCreateInfo>(resource.Info);
            if (std::holds_alternative<ImageDesc>(createInfo.Desc)) {
                const auto& imgDesc = std::get<ImageDesc>(createInfo.Desc);
                if (isDebugCompatible(imgDesc)) {
                    ImageDesc modifiedDesc = imgDesc;
                    modifiedDesc.MipLevels = 1;
                    modifiedDesc.Format = vk::Format::eB10G11R11UfloatPack32;
                    
                    Handle<Image> srcHandle{srcRef.Handle, modifiedDesc};
                    return PendingDebugPass{srcHandle};
                }
            }
        } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
            const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
            if (importInfo == GraphResourceImportInfo::Image) {
                // For imported images, we'd need access to the actual resource
                // This is simplified for the translation
                return std::nullopt;
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
                desc.Usage = ResourceInfo_.ImageUsageFlags[resourceIdx];
                
                std::shared_ptr<Image> image;
                try {
                    image = transientResourceCache->GetImage(desc);
                    if (!image) {
                        image = params.Device->CreateImage(desc, {});
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to create image: " + std::string(e.what()));
                }
                
                resources.push_back(RegistryResource{
                    AnyRenderResource::OwnedImage(image),
                    vk_sync::AccessType::Nothing
                });
            } else if (std::holds_alternative<BufferDesc>(createInfo.Desc)) {
                auto desc = std::get<BufferDesc>(createInfo.Desc);
                desc.Usage = ResourceInfo_.BufferUsageFlags[resourceIdx];
                
                std::shared_ptr<Buffer> buffer;
                try {
                    buffer = transientResourceCache->GetBuffer(desc);
                    if (!buffer) {
                        buffer = params.Device->CreateBuffer(desc, "rg buffer", std::nullopt);
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Failed to create buffer: " + std::string(e.what()));
                }
                
                resources.push_back(RegistryResource{
                    AnyRenderResource::OwnedBuffer(buffer),
                    vk_sync::AccessType::Nothing
                });
            }
        } else if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
            const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
            
            if (importInfo == GraphResourceImportInfo::Image) {
                // Implementation for imported image would go here
            } else if (importInfo == GraphResourceImportInfo::Buffer) {
                // Implementation for imported buffer would go here
            } else if (importInfo == GraphResourceImportInfo::RayTracingAcceleration) {
                // Implementation for imported RT acceleration would go here
            } else if (importInfo == GraphResourceImportInfo::SwapchainImage) {
                resources.push_back(RegistryResource{
                    AnyRenderResource::Pending(PendingRenderResourceInfo{resource}),
                    vk_sync::AccessType::ComputeShaderWrite
                });
            }
        }
    }
    
    ResourceRegistry resourceRegistry{
        params,
        resources,
        dynamicConstants,
        Pipelines
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
            const auto& resource = Resources[res.Handle.Id];
            if (std::holds_alternative<GraphResourceImportInfo>(resource.Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(resource.Info);
                if (importInfo == GraphResourceImportInfo::SwapchainImage) {
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
            resourceFirstAccessStates[resourceRef.Handle.Id] = &resourceRef.Access;
        }
        for (auto& resourceRef : pass.Write) {
            resourceFirstAccessStates[resourceRef.Handle.Id] = &resourceRef.Access;
        }
    }
    
    const auto& params = ResourceRegistry_.ExecutionParams;
    for (auto& [resourceIdx, access] : resourceFirstAccessStates) {
        auto& resource = ResourceRegistry_.Resources[resourceIdx];
        TransitionResource(
            params.Device,
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
    
    for (const auto& [resourceIdx, accessType] : ExportedResources) {
        if (accessType != vk_sync::AccessType::Nothing) {
            std::size_t rawId = 0; // Would get from resourceIdx
            auto& resource = ResourceRegistry_.Resources[rawId];
            TransitionResource(
                params.Device,
                cb,
                &resource,
                PassResourceAccessType{accessType, PassResourceAccessSyncType::AlwaysSync},
                false,
                ""
            );
        }
    }
    
    for (auto& res : ResourceRegistry_.Resources) {
        if (std::holds_alternative<AnyRenderResource::Pending>(res.Resource)) {
            auto& pending = std::get<AnyRenderResource::Pending>(res.Resource);
            if (std::holds_alternative<GraphResourceImportInfo>(pending.Info.Resource.Info)) {
                const auto& importInfo = std::get<GraphResourceImportInfo>(pending.Info.Resource.Info);
                if (importInfo == GraphResourceImportInfo::SwapchainImage) {
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
        params.Device->RecordCrashMarker(cb, "begin render pass " + pass.Name);
        
        if (auto debugUtils = params.Device->DebugUtils()) {
            vk::DebugUtilsLabelEXT label;
            label.pLabelName = pass.Name.c_str();
            debugUtils->CmdBeginDebugUtilsLabel(cb.Raw, label);
        }
        
        // GPU profiling scope would go here
        
        std::vector<std::pair<std::size_t, PassResourceAccessType>> transitions;
        for (const auto& resourceRef : pass.Read) {
            transitions.emplace_back(resourceRef.Handle.Id, resourceRef.Access);
        }
        for (const auto& resourceRef : pass.Write) {
            transitions.emplace_back(resourceRef.Handle.Id, resourceRef.Access);
        }
        
        for (const auto& [resourceIdx, access] : transitions) {
            auto& resource = resourceRegistry->Resources[resourceIdx];
            TransitionResource(params.Device, cb, &resource, access, false, "");
        }
        
        RenderPassApi api{cb, resourceRegistry};
        if (pass.RenderFn) {
            pass.RenderFn(&api);
        }
        
        if (auto debugUtils = params.Device->DebugUtils()) {
            debugUtils->CmdEndDebugUtilsLabel(cb.Raw);
        }
        
        params.Device->RecordCrashMarker(cb, "end render pass " + pass.Name);
    } catch (const std::exception& e) {
        throw std::runtime_error("Pass " + pass.Name + " failed to render: " + std::string(e.what()));
    }
}

void ExecutingRenderGraph::TransitionResource(const Device* device, const CommandBuffer& cb, RegistryResource* resource, const PassResourceAccessType& access, bool debug, const std::string& dbgStr) {
    if (RGAllowPassOverlap && resource->AccessType == access.AccessType && 
        access.SyncType == PassResourceAccessSyncType::SkipSyncIfSameAccessType) {
        return;
    }
    
    if (debug) {
        // Log transition
    }
    
    if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource->Resource) || 
        std::holds_alternative<AnyRenderResource::ImportedImage>(resource->Resource)) {
        
        std::shared_ptr<Image> image;
        if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource->Resource)) {
            image = std::get<AnyRenderResource::OwnedImage>(resource->Resource);
        } else {
            image = std::get<AnyRenderResource::ImportedImage>(resource->Resource);
        }
        
        auto aspectMask = vk_sync::image_aspect_mask_from_access_type_and_format(
            access.AccessType, image->Desc.Format);
        
        if (!aspectMask) {
            throw std::runtime_error("Invalid image access");
        }
        
        vk_sync::record_image_barrier(
            device->Raw,
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
            buffer = std::get<AnyRenderResource::OwnedBuffer>(resource->Resource);
        } else {
            buffer = std::get<AnyRenderResource::ImportedBuffer>(resource->Resource);
        }
        
        vk_sync::cmd::pipeline_barrier(
            device->Raw,
            cb.Raw,
            std::nullopt,
            std::vector<vk_sync::BufferBarrier>{
                vk_sync::BufferBarrier{
                    {resource->AccessType},
                    {access.AccessType},
                    device->UniversalQueue.Family.Index,
                    device->UniversalQueue.Family.Index,
                    buffer->Raw,
                    0,
                    buffer->Desc.Size
                }
            },
            {}
        );
        
        resource->AccessType = access.AccessType;
    } else if (std::holds_alternative<AnyRenderResource::ImportedRayTracingAcceleration>(resource->Resource)) {
        resource->AccessType = access.AccessType;
    }
}

RetiredRenderGraph::RetiredRenderGraph(std::vector<RegistryResource>&& resources)
    : Resources(std::move(resources)) {}

void RetiredRenderGraph::ReleaseResources(TransientResourceCache* transientResourceCache) {
    for (auto& resource : Resources) {
        if (std::holds_alternative<AnyRenderResource::OwnedImage>(resource.Resource)) {
            transientResourceCache->InsertImage(std::get<AnyRenderResource::OwnedImage>(resource.Resource));
        } else if (std::holds_alternative<AnyRenderResource::OwnedBuffer>(resource.Resource)) {
            transientResourceCache->InsertBuffer(std::get<AnyRenderResource::OwnedBuffer>(resource.Resource));
        } else if (std::holds_alternative<AnyRenderResource::Pending>(resource.Resource)) {
            throw std::runtime_error("RetiredRenderGraph::release_resources called while a resource was in Pending state");
        }
    }
}

vk::ImageUsageFlags ImageAccessMaskToUsageFlags(vk::AccessFlags accessMask) {
    switch (accessMask) {
        case vk::AccessFlagBits::eShaderRead:
            return vk::ImageUsageFlagBits::eSampled;
        case vk::AccessFlagBits::eShaderWrite:
            return vk::ImageUsageFlagBits::eStorage;
        case vk::AccessFlagBits::eColorAttachmentRead:
        case vk::AccessFlagBits::eColorAttachmentWrite:
            return vk::ImageUsageFlagBits::eColorAttachment;
        case vk::AccessFlagBits::eDepthStencilAttachmentRead:
        case vk::AccessFlagBits::eDepthStencilAttachmentWrite:
            return vk::ImageUsageFlagBits::eDepthStencilAttachment;
        case vk::AccessFlagBits::eTransferRead:
            return vk::ImageUsageFlagBits::eTransferSrc;
        case vk::AccessFlagBits::eTransferWrite:
            return vk::ImageUsageFlagBits::eTransferDst;
        default:
            if (accessMask == (vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite)) {
                return vk::ImageUsageFlagBits::eStorage;
            }
            throw std::runtime_error("Invalid image access mask");
    }
}

vk::BufferUsageFlags BufferAccessMaskToUsageFlags(vk::AccessFlags accessMask) {
    switch (accessMask) {
        case vk::AccessFlagBits::eIndirectCommandRead:
            return vk::BufferUsageFlagBits::eIndirectBuffer;
        case vk::AccessFlagBits::eIndexRead:
            return vk::BufferUsageFlagBits::eIndexBuffer;
        case vk::AccessFlagBits::eVertexAttributeRead:
            return vk::BufferUsageFlagBits::eUniformTexelBuffer;
        case vk::AccessFlagBits::eUniformRead:
            return vk::BufferUsageFlagBits::eUniformBuffer;
        case vk::AccessFlagBits::eShaderRead:
            return vk::BufferUsageFlagBits::eUniformTexelBuffer;
        case vk::AccessFlagBits::eShaderWrite:
            return vk::BufferUsageFlagBits::eStorageBuffer;
        case