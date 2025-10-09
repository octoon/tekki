// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/shader.rs

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace tekki::backend::vulkan
{

class Device;
struct ImageDesc;

// Constants
constexpr size_t MAX_DESCRIPTOR_SETS = 4;
constexpr size_t MAX_COLOR_ATTACHMENTS = 8;

// ============================================================================
// Descriptor Set Layout
// ============================================================================

// Descriptor dimensionality (for bindless support)
enum class DescriptorDimensionality
{
    Single,      // Normal descriptor
    Array,       // Fixed-size array
    RuntimeArray // Bindless runtime array
};

// Descriptor binding information (simplified from rspirv_reflect)
struct DescriptorInfo
{
    uint32_t binding;
    vk::DescriptorType type;
    uint32_t count;
    std::string name;
    DescriptorDimensionality dimensionality = DescriptorDimensionality::Single;
};

using DescriptorSetLayout = std::unordered_map<uint32_t, DescriptorInfo>;
using StageDescriptorSetLayouts = std::unordered_map<uint32_t, DescriptorSetLayout>;

// Options for descriptor set layout creation
struct DescriptorSetLayoutOpts
{
    std::optional<vk::DescriptorSetLayoutCreateFlags> flags;
    std::optional<DescriptorSetLayout> replace; // Override shader reflection

    DescriptorSetLayoutOpts() = default;
};

// ============================================================================
// Pipeline Common
// ============================================================================

struct ShaderPipelineCommon
{
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
    std::vector<std::unordered_map<uint32_t, vk::DescriptorType>> set_layout_info;
    std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    vk::PipelineBindPoint pipeline_bind_point;

    ShaderPipelineCommon() = default;
    ~ShaderPipelineCommon() = default;
};

// ============================================================================
// Compute Pipeline
// ============================================================================

struct ComputePipeline
{
    ShaderPipelineCommon common;
    std::array<uint32_t, 3> group_size;

    ComputePipeline() = default;

    // Convenience accessors
    const ShaderPipelineCommon& GetCommon() const { return common; }
    vk::Pipeline GetPipeline() const { return common.pipeline; }
    vk::PipelineLayout GetPipelineLayout() const { return common.pipeline_layout; }
};

// Shader source specification
enum class ShaderSourceType
{
    Rust,
    Hlsl,
};

struct ShaderSource
{
    ShaderSourceType type;
    std::string entry; // For Rust
    std::filesystem::path path; // For HLSL

    static ShaderSource Rust(const std::string& entry)
    {
        return ShaderSource{.type = ShaderSourceType::Rust, .entry = entry};
    }

    static ShaderSource Hlsl(const std::filesystem::path& path)
    {
        return ShaderSource{.type = ShaderSourceType::Hlsl, .path = path};
    }

    std::string GetEntry() const { return type == ShaderSourceType::Rust ? entry : "main"; }
};

struct ComputePipelineDesc
{
    std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> descriptor_set_opts;
    size_t push_constants_bytes = 0;
    ShaderSource source;

    ComputePipelineDesc() = default;
};

// Create compute pipeline from SPIR-V bytecode
// TODO: Implement this function
ComputePipeline CreateComputePipeline(Device* device, const std::vector<uint8_t>& spirv,
                                      const ComputePipelineDesc& desc);

// ============================================================================
// Raster Pipeline
// ============================================================================

struct RasterPipeline
{
    ShaderPipelineCommon common;

    RasterPipeline() = default;

    // Convenience accessors
    const ShaderPipelineCommon& GetCommon() const { return common; }
    vk::Pipeline GetPipeline() const { return common.pipeline; }
    vk::PipelineLayout GetPipelineLayout() const { return common.pipeline_layout; }
};

// Shader pipeline stage
enum class ShaderPipelineStage
{
    Vertex,
    Pixel,
    RayGen,
    RayMiss,
    RayClosestHit,
};

// Pipeline shader descriptor
struct PipelineShaderDesc
{
    ShaderPipelineStage stage;
    std::optional<std::vector<std::pair<size_t, vk::DescriptorSetLayoutCreateFlags>>> descriptor_set_layout_flags;
    size_t push_constants_bytes = 0;
    std::string entry = "main";
    ShaderSource source;

    PipelineShaderDesc() = default;
};

// Shader with bytecode
template <typename ShaderCode>
struct PipelineShader
{
    ShaderCode code;
    PipelineShaderDesc desc;

    PipelineShader() = default;
    PipelineShader(ShaderCode code, const PipelineShaderDesc& desc) : code(std::move(code)), desc(desc) {}
};

// ============================================================================
// Render Pass
// ============================================================================

struct RenderPassAttachmentDesc
{
    vk::Format format;
    vk::AttachmentLoadOp load_op;
    vk::AttachmentStoreOp store_op;
    vk::SampleCountFlags samples;

    static RenderPassAttachmentDesc New(vk::Format format)
    {
        return RenderPassAttachmentDesc{.format = format,
                                        .load_op = vk::AttachmentLoadOp::eLoad,
                                        .store_op = vk::AttachmentStoreOp::eStore,
                                        .samples = vk::SampleCountFlagBits::e1};
    }

    RenderPassAttachmentDesc& GarbageInput()
    {
        load_op = vk::AttachmentLoadOp::eDontCare;
        return *this;
    }

    RenderPassAttachmentDesc& ClearInput()
    {
        load_op = vk::AttachmentLoadOp::eClear;
        return *this;
    }

    RenderPassAttachmentDesc& DiscardOutput()
    {
        store_op = vk::AttachmentStoreOp::eDontCare;
        return *this;
    }

    vk::AttachmentDescription ToVk(vk::ImageLayout initial_layout, vk::ImageLayout final_layout) const;
};

// Framebuffer cache key
struct FramebufferCacheKey
{
    std::array<uint32_t, 2> dims;
    std::vector<std::pair<vk::ImageUsageFlags, vk::ImageCreateFlags>> attachments;

    // TODO: Implement hash and equality operators for unordered_map
    bool operator==(const FramebufferCacheKey& other) const;
};

struct FramebufferCacheKeyHash
{
    size_t operator()(const FramebufferCacheKey& key) const;
};

// Framebuffer cache
class FramebufferCache
{
  public:
    FramebufferCache(vk::RenderPass render_pass, const std::vector<RenderPassAttachmentDesc>& color_attachments,
                     std::optional<RenderPassAttachmentDesc> depth_attachment);

    ~FramebufferCache() = default;

    // Get or create framebuffer
    // TODO: Implement this function
    vk::Framebuffer GetOrCreate(vk::Device device, const FramebufferCacheKey& key);

  private:
    mutable std::mutex mutex_;
    std::unordered_map<FramebufferCacheKey, vk::Framebuffer, FramebufferCacheKeyHash> entries_;
    std::vector<RenderPassAttachmentDesc> attachment_desc_;
    vk::RenderPass render_pass_;
    size_t color_attachment_count_;
};

// Render pass
struct RenderPass
{
    vk::RenderPass raw;
    std::shared_ptr<FramebufferCache> framebuffer_cache;

    RenderPass() = default;
};

struct RenderPassDesc
{
    std::vector<RenderPassAttachmentDesc> color_attachments;
    std::optional<RenderPassAttachmentDesc> depth_attachment;
};

// Create render pass
// TODO: Implement this function
std::shared_ptr<RenderPass> CreateRenderPass(Device* device, const RenderPassDesc& desc);

// ============================================================================
// Raster Pipeline Descriptor
// ============================================================================

struct RasterPipelineDesc
{
    std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> descriptor_set_opts;
    std::shared_ptr<RenderPass> render_pass;
    bool face_cull = false;
    bool depth_write = true;
    size_t push_constants_bytes = 0;

    RasterPipelineDesc() = default;
};

// Create raster pipeline
// TODO: Implement this function
RasterPipeline CreateRasterPipeline(Device* device, const std::vector<PipelineShader<std::vector<uint8_t>>>& shaders,
                                    const RasterPipelineDesc& desc);

// ============================================================================
// Descriptor Set Layout Creation
// ============================================================================

struct DescriptorSetLayoutResult
{
    std::vector<vk::DescriptorSetLayout> layouts;
    std::vector<std::unordered_map<uint32_t, vk::DescriptorType>> layout_info;
};

// Create descriptor set layouts from shader reflection
// TODO: Implement this function
DescriptorSetLayoutResult CreateDescriptorSetLayouts(
    Device* device, const StageDescriptorSetLayouts& descriptor_sets, vk::ShaderStageFlags stage_flags,
    const std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>& set_opts);

// ============================================================================
// Utility Functions
// ============================================================================

// Merge descriptor set layouts from multiple shader stages
// TODO: Implement this function
StageDescriptorSetLayouts MergeShaderStageLayouts(const std::vector<StageDescriptorSetLayouts>& stages);

// Convert shader stage enum to Vulkan shader stage flags
vk::ShaderStageFlags ShaderPipelineStageToVk(ShaderPipelineStage stage);

// ============================================================================
// PLACEHOLDER DECLARATIONS
// These functions are declared but not yet implemented
// ============================================================================

// TODO: Ray tracing pipeline support
struct RayTracingPipeline
{
    ShaderPipelineCommon common;
    // TODO: Add ray tracing specific members (shader groups, etc.)
};

// TODO: Pipeline cache (separate from shader cache)
class PipelineCache
{
  public:
    // TODO: Implement pipeline caching
    PipelineCache() = default;
    ~PipelineCache() = default;
};

} // namespace tekki::backend::vulkan
