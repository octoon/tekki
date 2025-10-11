#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/device.h"

namespace tekki::render_graph {

// Forward declarations for Image and Buffer are in their own headers
// They will be included at the end of this file

// Resource descriptor types
struct GraphResourceDesc {
    // Placeholder for resource descriptor
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
};

struct ComputePipelineDesc {
    // Placeholder for compute pipeline descriptor
    std::string shader_entry_point;
    VkPipelineLayout layout;
};

struct PipelineShaderDesc {
    // Placeholder for pipeline shader descriptor
    std::string shader_module;
    std::string entry_point;
    VkShaderStageFlagBits stage;
};

struct RasterPipelineDesc {
    // Placeholder for raster pipeline descriptor
    VkPipelineLayout layout;
    VkRenderPass render_pass;
};

struct RayTracingPipelineDesc {
    // Placeholder for ray tracing pipeline descriptor
    VkPipelineLayout layout;
};

// Handle types
struct GraphRawResourceHandle {
    uint32_t id = 0;
    uint32_t version = 0;

    bool is_valid() const { return id != 0; }
    bool operator==(const GraphRawResourceHandle& other) const { return id == other.id && version == other.version; }

    GraphRawResourceHandle next_version() const {
        return GraphRawResourceHandle{id, version + 1};
    }
};

struct ComputePipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

struct RasterPipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

struct RtPipelineHandle {
    uint64_t id = 0;
    bool is_valid() const { return id != 0; }
};

// Exported handle
template<typename T>
struct ExportedHandle {
    uint64_t id = 0;

    bool is_valid() const { return id != 0; }
    bool operator==(const ExportedHandle& other) const { return id == other.id; }
};

// Pipeline cache
class PipelineCache {
public:
    // Placeholder implementation
    void GetOrCreateComputePipeline([[maybe_unused]] const ComputePipelineDesc& desc) {}
    void GetOrCreateRasterPipeline([[maybe_unused]] const RasterPipelineDesc& desc) {}
    void GetOrCreateRtPipeline([[maybe_unused]] const RayTracingPipelineDesc& desc) {}
};

// Transient resource cache
class TransientResourceCache {
public:
    // Placeholder implementation
    void ReleaseResources() {}
};

// Dynamic constants
class DynamicConstants {
public:
    // Placeholder implementation
    void Update() {}
};

// Command buffer
class CommandBuffer {
public:
    // Placeholder implementation
    void Begin() {}
    void End() {}
};

// Resource registry
class ResourceRegistry {
public:
    // Placeholder implementation
    void RegisterResource([[maybe_unused]] const GraphRawResourceHandle& handle, [[maybe_unused]] const std::shared_ptr<void>& resource) {}
    std::shared_ptr<void> GetResource([[maybe_unused]] const GraphRawResourceHandle& handle) const { return nullptr; }
};

// Registry resource
struct RegistryResource {
    GraphRawResourceHandle handle;
    std::shared_ptr<void> resource;
};

// Pending debug pass
struct PendingDebugPass {
    // Placeholder
    std::string name;
};

// Render pass API
class RenderPassApi {
public:
    // Placeholder implementation
    void SetPipeline([[maybe_unused]] const ComputePipelineHandle& pipeline) {}
    void SetPipeline([[maybe_unused]] const RasterPipelineHandle& pipeline) {}
    void SetPipeline([[maybe_unused]] const RtPipelineHandle& pipeline) {}
    void Dispatch([[maybe_unused]] uint32_t x, [[maybe_unused]] uint32_t y, [[maybe_unused]] uint32_t z) {}
    void Draw([[maybe_unused]] uint32_t vertex_count, [[maybe_unused]] uint32_t instance_count, [[maybe_unused]] uint32_t first_vertex, [[maybe_unused]] uint32_t first_instance) {}
};

// Device (forward declared in backend)
using Device = tekki::backend::vulkan::Device;

// Profiler data
struct VkProfilerData {
    // Placeholder
    uint64_t timestamp;
};

// GPU view types
struct GpuUav {};
struct GpuSrv {};
struct GpuRt {};

// Raster pipeline descriptor builder
struct RasterPipelineDescBuilder {
    // Placeholder implementation
    VkPipelineLayout layout;
    VkRenderPass render_pass;
};

} // namespace tekki::render_graph

// Include Image and Buffer definitions after all other types are defined
#include "tekki/render_graph/Image.h"
#include "tekki/render_graph/Buffer.h"