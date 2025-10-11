#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <filesystem>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/types.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"

namespace tekki::render_graph {

// Concept definitions
template<typename T>
concept ResourceDesc = requires {
    typename T::Resource;
};

template<typename T>
concept Resource = requires {
    typename T::Desc;
};

template<typename T>
concept GpuViewType = true; // Placeholder concept

// GPU view types are defined in types.h
// Raster pipeline descriptor builder is defined in types.h


class PassBuilder {
public:
    PassBuilder(RenderGraph& rg, size_t passIdx);
    ~PassBuilder();

    template<typename Desc>
    auto Create(const Desc& desc) -> Handle<typename Desc::Resource>
        requires ResourceDesc<Desc> && std::same_as<typename Desc::Resource::Desc, Desc>
    {
        return rg.Create(desc);
    }

    template<typename Res, typename ViewType>
    Ref<Res, ViewType> WriteImpl(Handle<Res>& handle, vk_sync::AccessType accessType, PassResourceAccessSyncType syncType)
        requires Resource<Res> && GpuViewType<ViewType>
    {
        auto& pass = *pass_;

        // CHECK DISABLED: multiple writes or mixing of reads and writes is valid with non-overlapping views
        /*
        // Don't know of a good way to use the borrow checker to verify that writes and reads
        // don't overlap, and that multiple writes don't happen to the same resource.
        // The borrow checker will at least check that resources don't alias each other,
        // but for the access in render passes, we resort to a runtime check.
        if (std::any_of(pass.Write.begin(), pass.Write.end(), [&](const auto& item) { return item.Handle == handle.Raw; })) {
            throw std::runtime_error("Trying to write twice to the same resource within one render pass");
        } else if (std::any_of(pass.Read.begin(), pass.Read.end(), [&](const auto& item) { return item.Handle == handle.Raw; })) {
            throw std::runtime_error("Trying to read and write to the same resource within one render pass");
        }*/

        pass.Write.push_back(PassResourceRef{
            .Handle = handle.Raw,
            .Access = PassResourceAccessType(accessType, syncType)
        });

        return Ref<Res, ViewType>{
            .Desc = handle.Desc,
            .Handle = handle.Raw.NextVersion(),
            .Marker = {}
        };
    }

    template<typename Res>
    Ref<Res, GpuUav> Write(Handle<Res>& handle, vk_sync::AccessType accessType)
        requires Resource<Res>
    {
        switch (accessType) {
            case vk_sync::AccessType::CommandBufferWriteNVX:
            case vk_sync::AccessType::VertexShaderWrite:
            case vk_sync::AccessType::TessellationControlShaderWrite:
            case vk_sync::AccessType::TessellationEvaluationShaderWrite:
            case vk_sync::AccessType::GeometryShaderWrite:
            case vk_sync::AccessType::FragmentShaderWrite:
            case vk_sync::AccessType::ComputeShaderWrite:
            case vk_sync::AccessType::AnyShaderWrite:
            case vk_sync::AccessType::TransferWrite:
            case vk_sync::AccessType::HostWrite:
            case vk_sync::AccessType::ColorAttachmentReadWrite:
            case vk_sync::AccessType::General:
                break;
            default:
                throw std::runtime_error("Invalid access type");
        }

        return WriteImpl<Res, GpuUav>(handle, accessType, PassResourceAccessSyncType::AlwaysSync);
    }

    template<typename Res>
    Ref<Res, GpuUav> WriteNoSync(Handle<Res>& handle, vk_sync::AccessType accessType)
        requires Resource<Res>
    {
        switch (accessType) {
            case vk_sync::AccessType::CommandBufferWriteNVX:
            case vk_sync::AccessType::VertexShaderWrite:
            case vk_sync::AccessType::TessellationControlShaderWrite:
            case vk_sync::AccessType::TessellationEvaluationShaderWrite:
            case vk_sync::AccessType::GeometryShaderWrite:
            case vk_sync::AccessType::FragmentShaderWrite:
            case vk_sync::AccessType::ComputeShaderWrite:
            case vk_sync::AccessType::AnyShaderWrite:
            case vk_sync::AccessType::TransferWrite:
            case vk_sync::AccessType::HostWrite:
            case vk_sync::AccessType::ColorAttachmentReadWrite:
            case vk_sync::AccessType::General:
                break;
            default:
                throw std::runtime_error("Invalid access type");
        }

        return WriteImpl<Res, GpuUav>(handle, accessType, PassResourceAccessSyncType::SkipSyncIfSameAccessType);
    }

    template<typename Res>
    Ref<Res, GpuRt> Raster(Handle<Res>& handle, vk_sync::AccessType accessType)
        requires Resource<Res>
    {
        switch (accessType) {
            case vk_sync::AccessType::ColorAttachmentWrite:
            case vk_sync::AccessType::DepthStencilAttachmentWrite:
            case vk_sync::AccessType::DepthAttachmentWriteStencilReadOnly:
            case vk_sync::AccessType::StencilAttachmentWriteDepthReadOnly:
                break;
            default:
                throw std::runtime_error("Invalid access type");
        }

        return WriteImpl<Res, GpuRt>(handle, accessType, PassResourceAccessSyncType::AlwaysSync);
    }

    template<typename Res>
    Ref<Res, GpuSrv> Read(const Handle<Res>& handle, vk_sync::AccessType accessType)
        requires Resource<Res>
    {
        switch (accessType) {
            case vk_sync::AccessType::CommandBufferReadNVX:
            case vk_sync::AccessType::IndirectBuffer:
            case vk_sync::AccessType::IndexBuffer:
            case vk_sync::AccessType::VertexBuffer:
            case vk_sync::AccessType::VertexShaderReadUniformBuffer:
            case vk_sync::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::VertexShaderReadOther:
            case vk_sync::AccessType::TessellationControlShaderReadUniformBuffer:
            case vk_sync::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::TessellationControlShaderReadOther:
            case vk_sync::AccessType::TessellationEvaluationShaderReadUniformBuffer:
            case vk_sync::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::TessellationEvaluationShaderReadOther:
            case vk_sync::AccessType::GeometryShaderReadUniformBuffer:
            case vk_sync::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::GeometryShaderReadOther:
            case vk_sync::AccessType::FragmentShaderReadUniformBuffer:
            case vk_sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::FragmentShaderReadColorInputAttachment:
            case vk_sync::AccessType::FragmentShaderReadDepthStencilInputAttachment:
            case vk_sync::AccessType::FragmentShaderReadOther:
            case vk_sync::AccessType::ColorAttachmentRead:
            case vk_sync::AccessType::DepthStencilAttachmentRead:
            case vk_sync::AccessType::ComputeShaderReadUniformBuffer:
            case vk_sync::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::ComputeShaderReadOther:
            case vk_sync::AccessType::AnyShaderReadUniformBuffer:
            case vk_sync::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:
            case vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:
            case vk_sync::AccessType::AnyShaderReadOther:
            case vk_sync::AccessType::TransferRead:
            case vk_sync::AccessType::HostRead:
            case vk_sync::AccessType::Present:
                break;
            default:
                throw std::runtime_error("Invalid access type");
        }

        auto& pass = *pass_;

        // CHECK DISABLED: multiple writes or mixing of reads and writes is valid with non-overlapping views
        /*// Runtime "borrow" check; see info in `Write` above.
        if (std::any_of(pass.Write.begin(), pass.Write.end(), [&](const auto& item) { return item.Handle == handle.Raw; })) {
            throw std::runtime_error("Trying to read and write to the same resource within one render pass");
        }*/

        pass.Read.push_back(PassResourceRef{
            .Handle = handle.Raw,
            .Access = PassResourceAccessType(accessType, PassResourceAccessSyncType::SkipSyncIfSameAccessType)
        });

        return Ref<Res, GpuSrv>{
            .Desc = handle.Desc,
            .Handle = handle.Raw,
            .Marker = {}
        };
    }

    template<typename Res>
    Ref<Res, GpuRt> RasterRead(const Handle<Res>& handle, vk_sync::AccessType accessType)
        requires Resource<Res>
    {
        switch (accessType) {
            case vk_sync::AccessType::ColorAttachmentRead:
            case vk_sync::AccessType::DepthStencilAttachmentRead:
                break;
            default:
                throw std::runtime_error("Invalid access type");
        }

        auto& pass = *pass_;

        pass.Read.push_back(PassResourceRef{
            .Handle = handle.Raw,
            .Access = PassResourceAccessType(accessType, PassResourceAccessSyncType::SkipSyncIfSameAccessType)
        });

        return Ref<Res, GpuRt>{
            .Desc = handle.Desc,
            .Handle = handle.Raw,
            .Marker = {}
        };
    }

    RgComputePipelineHandle RegisterComputePipeline(const std::filesystem::path& path);
    RgComputePipelineHandle RegisterComputePipelineWithDesc(ComputePipelineDesc desc);
    RgRasterPipelineHandle RegisterRasterPipeline(const std::vector<PipelineShaderDesc>& shaders, RasterPipelineDescBuilder desc);
    RgRtPipelineHandle RegisterRayTracingPipeline(const std::vector<PipelineShaderDesc>& shaders, RayTracingPipelineDesc desc);

    void Render(std::function<void(RenderPassApi&)> render);

private:
    RenderGraph& rg_;
    size_t passIdx_;
    std::unique_ptr<RecordedPass> pass_;
};

} // namespace tekki::render_graph