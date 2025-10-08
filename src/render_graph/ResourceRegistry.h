#pragma once

#include "Resource.h"
#include "../backend/vulkan/shader.h"
#include "../backend/vulkan/dynamic_constants.h"

#include <memory>
#include <vector>
#include <variant>

namespace tekki::render_graph {

// Forward declarations
struct RenderGraphExecutionParams;
struct RenderGraphPipelines;

// Pending resource info
struct PendingRenderResourceInfo {
    // GraphResourceInfo resource; // Will be defined later
};

// Any render resource variant
class AnyRenderResource {
public:
    enum class Type {
        OwnedImage,
        ImportedImage,
        OwnedBuffer,
        ImportedBuffer,
        ImportedRayTracingAcceleration,
        Pending
    };

    Type type;
    std::variant<
        Image,
        std::shared_ptr<Image>,
        Buffer,
        std::shared_ptr<Buffer>,
        std::shared_ptr<RayTracingAcceleration>,
        PendingRenderResourceInfo
    > data;

    AnyRenderResourceRef borrow() const;
};

// Any render resource reference
class AnyRenderResourceRef {
public:
    enum class Type {
        Image,
        Buffer,
        RayTracingAcceleration
    };

    Type type;
    std::variant<
        const Image*,
        const Buffer*,
        const RayTracingAcceleration*
    > data;
};

// Registry resource
struct RegistryResource {
    AnyRenderResource resource;
    vk_sync::AccessType access_type;
};

// Resource registry
class ResourceRegistry {
public:
    RenderGraphExecutionParams* execution_params;
    std::vector<RegistryResource> resources;
    DynamicConstants* dynamic_constants;
    RenderGraphPipelines pipelines;

    // Image access methods
    template<typename ViewType>
    const Image* image(const Ref<ImageResource, ViewType>& resource) {
        return image_from_raw_handle<ViewType>(resource.handle);
    }

    template<typename ViewType>
    const Image* image_from_raw_handle(GraphRawResourceHandle handle) {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::Image) {
            return std::get<const Image*>(ref.data);
        }
        throw std::runtime_error("Resource is not an image");
    }

    // Buffer access methods
    template<typename ViewType>
    const Buffer* buffer(const Ref<BufferResource, ViewType>& resource) {
        return buffer_from_raw_handle<ViewType>(resource.handle);
    }

    template<typename ViewType>
    const Buffer* buffer_from_raw_handle(GraphRawResourceHandle handle) {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::Buffer) {
            return std::get<const Buffer*>(ref.data);
        }
        throw std::runtime_error("Resource is not a buffer");
    }

    // Ray tracing acceleration access methods
    template<typename ViewType>
    const RayTracingAcceleration* rt_acceleration(const Ref<RayTracingAccelerationResource, ViewType>& resource) {
        return rt_acceleration_from_raw_handle<ViewType>(resource.handle);
    }

    template<typename ViewType>
    const RayTracingAcceleration* rt_acceleration_from_raw_handle(GraphRawResourceHandle handle) {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::RayTracingAcceleration) {
            return std::get<const RayTracingAcceleration*>(ref.data);
        }
        throw std::runtime_error("Resource is not a ray tracing acceleration");
    }

    // Image view creation
    vk::ImageView image_view(GraphRawResourceHandle resource, const ImageViewDesc& view_desc) {
        auto image = image_from_raw_handle<GpuSrv>(resource);
        return image->view(execution_params->device, view_desc);
    }

    // Pipeline access methods
    std::shared_ptr<ComputePipeline> compute_pipeline(RgComputePipelineHandle pipeline);
    std::shared_ptr<RasterPipeline> raster_pipeline(RgRasterPipelineHandle pipeline);
    std::shared_ptr<RayTracingPipeline> ray_tracing_pipeline(RgRtPipelineHandle pipeline);
};

} // namespace tekki::render_graph