#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "../backend/vulkan/dynamic_constants.h"
#include "../backend/vulkan/shader.h"
#include "../backend/vulkan/barrier.h"
#include "../backend/vulkan/ray_tracing.h"
#include "../backend/vulkan/device.h"
#include "../backend/vulkan/image.h"
#include "resource.h"
#include "execution_params.h"

namespace tekki::render_graph
{

// Forward declarations
struct RenderGraphExecutionParams;
struct RenderGraphPipelines;

// Forward declarations for pipeline types
class ComputePipeline;
class RasterPipeline;
class RayTracingPipeline;


// Registry resource
struct RegistryResource
{
    AnyRenderResource resource;
    backend::vulkan::AccessType access_type;
};

// Resource registry
class ResourceRegistry
{
public:
    RenderGraphExecutionParams* execution_params;
    std::vector<RegistryResource> resources;
    backend::vulkan::DynamicConstants* dynamic_constants;
    RenderGraphPipelines pipelines;

    // Image access methods
    template <typename ViewType> const backend::vulkan::Image* image(const Ref<ImageResource, ViewType>& resource)
    {
        return image_from_raw_handle<ViewType>(resource.handle);
    }

    template <typename ViewType> const backend::vulkan::Image* image_from_raw_handle(GraphRawResourceHandle handle)
    {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::Image)
        {
            return std::get<const backend::vulkan::Image*>(ref.data);
        }
        throw std::runtime_error("Resource is not an image");
    }

    // Buffer access methods
    template <typename ViewType> const backend::vulkan::Buffer* buffer(const Ref<BufferResource, ViewType>& resource)
    {
        return buffer_from_raw_handle<ViewType>(resource.handle);
    }

    template <typename ViewType> const backend::vulkan::Buffer* buffer_from_raw_handle(GraphRawResourceHandle handle)
    {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::Buffer)
        {
            return std::get<const backend::vulkan::Buffer*>(ref.data);
        }
        throw std::runtime_error("Resource is not a buffer");
    }

    // Ray tracing acceleration access methods
    template <typename ViewType>
    const backend::vulkan::AccelerationStructure* rt_acceleration(const Ref<RayTracingAccelerationResource, ViewType>& resource)
    {
        return rt_acceleration_from_raw_handle<ViewType>(resource.handle);
    }

    template <typename ViewType>
    const backend::vulkan::AccelerationStructure* rt_acceleration_from_raw_handle(GraphRawResourceHandle handle)
    {
        auto& reg_resource = resources[handle.id];
        auto ref = reg_resource.resource.borrow();
        if (ref.type == AnyRenderResourceRef::Type::RayTracingAcceleration)
        {
            return std::get<const backend::vulkan::AccelerationStructure*>(ref.data);
        }
        throw std::runtime_error("Resource is not a ray tracing acceleration");
    }

    // Image view creation
    vk::ImageView image_view(GraphRawResourceHandle resource, const backend::vulkan::ImageViewDesc& /*view_desc*/)
    {
        auto image_ptr = image_from_raw_handle<GpuSrv>(resource);
        // Create a proper image view for the resource
        vk::ImageViewCreateInfo view_info{};
        view_info.image = image_ptr->raw();
        view_info.viewType = vk::ImageViewType::e2D;
        view_info.format = image_ptr->desc().format;
        view_info.components = vk::ComponentMapping{};
        view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        vk::ImageView image_view;
        auto result = execution_params->device->raw().createImageView(&view_info, nullptr, &image_view);
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create image view");
        }
        return image_view;
    }

    // Pipeline access methods
    std::shared_ptr<ComputePipeline> compute_pipeline(RgComputePipelineHandle pipeline);
    std::shared_ptr<RasterPipeline> raster_pipeline(RgRasterPipelineHandle pipeline);
    std::shared_ptr<RayTracingPipeline> ray_tracing_pipeline(RgRtPipelineHandle pipeline);
};

} // namespace tekki::render_graph