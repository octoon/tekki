#pragma once

#include "../backend/vulkan/image.h"
#include "../backend/vulkan/buffer.h"
#include "../backend/vulkan/ray_tracing.h"

#include <memory>
#include <variant>

namespace tekki::render_graph {

// Forward declarations
struct GraphRawResourceHandle;
class AnyRenderResource;
class AnyRenderResourceRef;

// Resource type aliases
using ImageResource = Image;
using BufferResource = Buffer;
using RayTracingAccelerationResource = RayTracingAcceleration;

// Resource trait equivalent - using template specialization instead of inheritance
template<typename T>
struct ResourceTraits;

// Specialization for Image
template<>
struct ResourceTraits<Image> {
    using DescType = ImageDesc;

    static const Image* borrow_resource(const AnyRenderResource& res);
};

// Specialization for Buffer
template<>
struct ResourceTraits<Buffer> {
    using DescType = BufferDesc;

    static const Buffer* borrow_resource(const AnyRenderResource& res);
};

// Specialization for RayTracingAcceleration
template<>
struct ResourceTraits<RayTracingAcceleration> {
    struct DescType {}; // Empty struct for RT acceleration desc

    static const RayTracingAcceleration* borrow_resource(const AnyRenderResource& res);
};

// Resource description types
struct RayTracingAccelerationDesc {};

// Resource description trait
template<typename T>
struct ResourceDescTraits {
    using ResourceType = T;
};

// Specializations
template<>
struct ResourceDescTraits<ImageDesc> {
    using ResourceType = Image;
};

template<>
struct ResourceDescTraits<BufferDesc> {
    using ResourceType = Buffer;
};

template<>
struct ResourceDescTraits<RayTracingAccelerationDesc> {
    using ResourceType = RayTracingAcceleration;
};

// Graph resource description variant
using GraphResourceDesc = std::variant<ImageDesc, BufferDesc, RayTracingAccelerationDesc>;

// Graph resource handle
struct GraphRawResourceHandle {
    uint32_t id;
    uint32_t version;

    GraphRawResourceHandle next_version() const {
        return {id, version + 1};
    }

    bool operator==(const GraphRawResourceHandle& other) const {
        return id == other.id && version == other.version;
    }
};

// Typed resource handle
template<typename ResType>
class Handle {
public:
    GraphRawResourceHandle raw;
    typename ResourceTraits<ResType>::DescType desc;

    const typename ResourceTraits<ResType>::DescType* desc() const { return &desc; }

    Handle clone_unchecked() const {
        return {raw, desc};
    }

    bool operator==(const Handle& other) const {
        return raw == other.raw;
    }
};

// Exported handle
template<typename ResType>
class ExportedHandle {
public:
    GraphRawResourceHandle raw;

    ExportedHandle clone() const {
        return {raw};
    }
};

// GPU view types
struct GpuSrv {
    static constexpr bool IS_WRITABLE = false;
};

struct GpuUav {
    static constexpr bool IS_WRITABLE = true;
};

struct GpuRt {
    static constexpr bool IS_WRITABLE = true;
};

// Typed resource reference
template<typename ResType, typename ViewType>
class Ref {
public:
    GraphRawResourceHandle handle;
    typename ResourceTraits<ResType>::DescType desc;

    const typename ResourceTraits<ResType>::DescType* desc() const { return &desc; }

    Ref clone() const {
        return {handle, desc};
    }

    // Bind methods for images
    template<typename T = ResType>
    auto bind_view(ImageViewDescBuilder view_desc) -> std::enable_if_t<std::is_same_v<T, ImageResource>, RenderPassBinding>;
};

} // namespace tekki::render_graph