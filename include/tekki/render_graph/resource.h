#pragma once

#include <memory>
#include <variant>

#include "../backend/vulkan/buffer.h"
#include "../backend/vulkan/image.h"
#include "../backend/vulkan/ray_tracing.h"

namespace tekki::render_graph
{

// Pipeline handle types
struct RgComputePipelineHandle
{
    size_t id;
};

struct RgRasterPipelineHandle
{
    size_t id;
};

struct RgRtPipelineHandle
{
    size_t id;
};

// Image view description builder (placeholder)
struct ImageViewDescBuilder
{
    ImageViewDescBuilder& mip_level(uint32_t) { return *this; }
    ImageViewDescBuilder& mip_count(uint32_t) { return *this; }
    ImageViewDescBuilder& array_layer(uint32_t) { return *this; }
    ImageViewDescBuilder& array_layer_count(uint32_t) { return *this; }
    backend::vulkan::ImageViewDesc build() const { return backend::vulkan::ImageViewDesc{}; }
};

// Forward declarations
struct GraphRawResourceHandle;

// Pending resource info
struct PendingRenderResourceInfo
{
    // GraphResourceInfo resource; // Will be defined later
};

// Any render resource reference
class AnyRenderResourceRef
{
public:
    enum class Type
    {
        Image,
        Buffer,
        RayTracingAcceleration
    };

    Type type;
    std::variant<const backend::vulkan::Image*, const backend::vulkan::Buffer*, const backend::vulkan::AccelerationStructure*> data;
};

// Any render resource variant
class AnyRenderResource
{
public:
    enum class Type
    {
        OwnedImage,
        ImportedImage,
        OwnedBuffer,
        ImportedBuffer,
        ImportedRayTracingAcceleration,
        Pending
    };

    Type type;
    std::variant<backend::vulkan::Image, std::shared_ptr<backend::vulkan::Image>, backend::vulkan::Buffer, std::shared_ptr<backend::vulkan::Buffer>,
                 std::shared_ptr<backend::vulkan::AccelerationStructure>, PendingRenderResourceInfo>
        data;

    AnyRenderResourceRef borrow() const
    {
        // Placeholder implementation
        return AnyRenderResourceRef{};
    }
};

// Resource type aliases
using ImageResource = backend::vulkan::Image;
using BufferResource = backend::vulkan::Buffer;
using RayTracingAccelerationResource = backend::vulkan::AccelerationStructure;

// Resource trait equivalent - using template specialization instead of inheritance
template <typename T> struct ResourceTraits;

// Specialization for Image
template <> struct ResourceTraits<backend::vulkan::Image>
{
    using DescType = backend::vulkan::ImageDesc;

    static const backend::vulkan::Image* borrow_resource(const AnyRenderResource& res);
};

// Specialization for Buffer
template <> struct ResourceTraits<backend::vulkan::Buffer>
{
    using DescType = backend::vulkan::BufferDesc;

    static const backend::vulkan::Buffer* borrow_resource(const AnyRenderResource& res);
};

// Specialization for RayTracingAcceleration
template <> struct ResourceTraits<backend::vulkan::AccelerationStructure>
{
    struct DescType
    {
    }; // Empty struct for RT acceleration desc

    static const backend::vulkan::AccelerationStructure* borrow_resource(const AnyRenderResource& res);
};

// Resource description types
struct RayTracingAccelerationDesc
{
};

// Resource description trait
template <typename T> struct ResourceDescTraits
{
    using ResourceType = T;
};

// Specializations
template <> struct ResourceDescTraits<backend::vulkan::ImageDesc>
{
    using ResourceType = backend::vulkan::Image;
};

template <> struct ResourceDescTraits<backend::vulkan::BufferDesc>
{
    using ResourceType = backend::vulkan::Buffer;
};

template <> struct ResourceDescTraits<RayTracingAccelerationDesc>
{
    using ResourceType = backend::vulkan::AccelerationStructure;
};

// Graph resource description variant
using GraphResourceDesc = std::variant<backend::vulkan::ImageDesc, backend::vulkan::BufferDesc, RayTracingAccelerationDesc>;

// Graph resource handle
struct GraphRawResourceHandle
{
    uint32_t id;
    uint32_t version;

    GraphRawResourceHandle next_version() const { return {id, version + 1}; }

    bool operator==(const GraphRawResourceHandle& other) const { return id == other.id && version == other.version; }
};

// Typed resource handle
template <typename ResType> class Handle
{
public:
    GraphRawResourceHandle raw;
    typename ResourceTraits<ResType>::DescType desc;

    const typename ResourceTraits<ResType>::DescType* get_desc() const { return &desc; }

    Handle clone_unchecked() const { return {raw, desc}; }

    bool operator==(const Handle& other) const { return raw == other.raw; }
};

// Exported handle
template <typename ResType> class ExportedHandle
{
public:
    GraphRawResourceHandle raw;

    ExportedHandle clone() const { return {raw}; }
};

// GPU view types
struct GpuSrv
{
    static constexpr bool IS_WRITABLE = false;
};

struct GpuUav
{
    static constexpr bool IS_WRITABLE = true;
};

struct GpuRt
{
    static constexpr bool IS_WRITABLE = true;
};

// Typed resource reference
template <typename ResType, typename ViewType> class Ref
{
public:
    GraphRawResourceHandle handle;
    typename ResourceTraits<ResType>::DescType desc;

    const typename ResourceTraits<ResType>::DescType* get_desc() const { return &desc; }

    Ref clone() const { return {handle, desc}; }

    // Bind methods for images (placeholder - to be implemented later)
    // template <typename T = ResType>
    // auto bind_view(ImageViewDescBuilder view_desc)
    //     -> std::enable_if_t<std::is_same_v<T, ImageResource>, RenderPassBinding>;
};

} // namespace tekki::render_graph