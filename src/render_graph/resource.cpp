#include "tekki/render_graph/resource.h"

namespace tekki {
namespace render_graph {

// ResourceTraits specializations
const backend::vulkan::Image* ResourceTraits<backend::vulkan::Image>::borrow_resource(const AnyRenderResource& res) {
    if (res.type == AnyRenderResource::Type::OwnedImage) {
        return &std::get<backend::vulkan::Image>(res.data);
    } else if (res.type == AnyRenderResource::Type::ImportedImage) {
        return std::get<std::shared_ptr<backend::vulkan::Image>>(res.data).get();
    }
    throw std::runtime_error("Resource is not an image");
}

const backend::vulkan::Buffer* ResourceTraits<backend::vulkan::Buffer>::borrow_resource(const AnyRenderResource& res) {
    if (res.type == AnyRenderResource::Type::OwnedBuffer) {
        return &std::get<backend::vulkan::Buffer>(res.data);
    } else if (res.type == AnyRenderResource::Type::ImportedBuffer) {
        return std::get<std::shared_ptr<backend::vulkan::Buffer>>(res.data).get();
    }
    throw std::runtime_error("Resource is not a buffer");
}

const backend::vulkan::AccelerationStructure* ResourceTraits<backend::vulkan::AccelerationStructure>::borrow_resource(const AnyRenderResource& res) {
    if (res.type == AnyRenderResource::Type::ImportedRayTracingAcceleration) {
        return std::get<std::shared_ptr<backend::vulkan::AccelerationStructure>>(res.data).get();
    }
    throw std::runtime_error("Resource is not a ray tracing acceleration");
}

// Ref methods (placeholder - to be implemented later)
// template<typename ResType, typename ViewType>
// auto Ref<ResType, ViewType>::bind_view(ImageViewDescBuilder view_desc)
//     -> std::enable_if_t<std::is_same_v<ResType, ImageResource>, RenderPassBinding> {
//
//     return RenderPassBinding{
//         .type = RenderPassBinding::Type::Image,
//         .data = RenderPassImageBinding{
//             .handle = handle,
//             .view_desc = view_desc.build(),
//             .image_layout = ViewType::IS_WRITABLE ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal
//         }
//     };
// }

// Explicit template instantiations
template class Ref<ImageResource, GpuSrv>;
template class Ref<ImageResource, GpuUav>;
template class Ref<ImageResource, GpuRt>;
template class Ref<BufferResource, GpuSrv>;
template class Ref<BufferResource, GpuUav>;
template class Ref<RayTracingAccelerationResource, GpuSrv>;

} // namespace render_graph
} // namespace tekki