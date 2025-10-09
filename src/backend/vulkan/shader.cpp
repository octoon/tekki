// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/shader.rs

#include "../../include/tekki/backend/vulkan/shader.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include "../../include/tekki/backend/shader_compiler.h"
#include "../../include/tekki/backend/spirv_reflect_helper.h"
#include "../../include/tekki/backend/vulkan/device.h"

namespace tekki::backend::vulkan
{

// ============================================================================
// RenderPassAttachmentDesc Implementation
// ============================================================================

vk::AttachmentDescription RenderPassAttachmentDesc::ToVk(vk::ImageLayout initial_layout,
                                                         vk::ImageLayout final_layout) const
{
    vk::AttachmentDescription desc{};
    desc.format = format;
    desc.samples = samples;
    desc.loadOp = load_op;
    desc.storeOp = store_op;
    desc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    desc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    desc.initialLayout = initial_layout;
    desc.finalLayout = final_layout;
    return desc;
}

// ============================================================================
// FramebufferCacheKey Implementation
// ============================================================================

bool FramebufferCacheKey::operator==(const FramebufferCacheKey& other) const
{
    return dims == other.dims && attachments == other.attachments;
}

size_t FramebufferCacheKeyHash::operator()(const FramebufferCacheKey& key) const
{
    size_t hash = 0;
    hash ^= std::hash<uint32_t>{}(key.dims[0]) << 0;
    hash ^= std::hash<uint32_t>{}(key.dims[1]) << 1;

    for (const auto& [usage, flags] : key.attachments)
    {
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(usage));
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(flags));
    }

    return hash;
}

// ============================================================================
// FramebufferCache Implementation
// ============================================================================

FramebufferCache::FramebufferCache(vk::RenderPass render_pass,
                                   const std::vector<RenderPassAttachmentDesc>& color_attachments,
                                   std::optional<RenderPassAttachmentDesc> depth_attachment)
    : render_pass_(render_pass), color_attachment_count_(color_attachments.size())
{
    attachment_desc_ = color_attachments;
    if (depth_attachment)
    {
        attachment_desc_.push_back(*depth_attachment);
    }
}

vk::Framebuffer FramebufferCache::GetOrCreate(vk::Device device, const FramebufferCacheKey& key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(key);
    if (it != entries_.end())
    {
        return it->second;
    }

    // TODO: Implement imageless framebuffer creation
    // This requires:
    // 1. Create vk::FramebufferAttachmentImageInfoKHR for each attachment
    // 2. Use VK_KHR_imageless_framebuffer extension
    // 3. Create framebuffer with IMAGELESS flag
    //
    // Reference implementation (Rust):
    // ```rust
    // let attachments = self.attachment_desc.iter().zip(key.attachments.iter())
    //     .map(|(desc, (usage, flags))| {
    //         vk::FramebufferAttachmentImageInfoKHR::builder()
    //             .width(width).height(height).flags(*flags).layer_count(1)
    //             .view_formats(slice::from_ref(&desc.format)).usage(*usage).build()
    //     }).collect::<ArrayVec<[_; MAX_COLOR_ATTACHMENTS + 1]>>();
    //
    // let mut imageless_desc = vk::FramebufferAttachmentsCreateInfoKHR::builder()
    //     .attachment_image_infos(&attachments);
    //
    // let mut fbo_desc = vk::FramebufferCreateInfo::builder()
    //     .flags(vk::FramebufferCreateFlags::IMAGELESS_KHR)
    //     .render_pass(self.render_pass).width(width).height(height).layers(1)
    //     .push_next(&mut imageless_desc);
    //
    // fbo_desc.attachment_count = attachments.len() as _;
    // unsafe { device.create_framebuffer(&fbo_desc, None)? }
    // ```

    spdlog::error("FramebufferCache::GetOrCreate - TODO: Implement imageless framebuffer creation");
    throw std::runtime_error("FramebufferCache::GetOrCreate not implemented");
}

// ============================================================================
// Render Pass Creation
// ============================================================================

std::shared_ptr<RenderPass> CreateRenderPass(Device* device, const RenderPassDesc& desc)
{
    // Collect attachment descriptions
    std::vector<vk::AttachmentDescription> attachments;

    for (const auto& color_attachment : desc.color_attachments)
    {
        attachments.push_back(color_attachment.ToVk(vk::ImageLayout::eColorAttachmentOptimal,
                                                     vk::ImageLayout::eColorAttachmentOptimal));
    }

    if (desc.depth_attachment)
    {
        attachments.push_back(desc.depth_attachment->ToVk(
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal));
    }

    // Color attachment references
    std::vector<vk::AttachmentReference> color_refs;
    for (uint32_t i = 0; i < desc.color_attachments.size(); ++i)
    {
        color_refs.push_back(
            vk::AttachmentReference{.attachment = i, .layout = vk::ImageLayout::eColorAttachmentOptimal});
    }

    // Depth attachment reference
    vk::AttachmentReference depth_ref{.attachment = static_cast<uint32_t>(desc.color_attachments.size()),
                                      .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    // Subpass description
    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.data();

    if (desc.depth_attachment)
    {
        subpass.pDepthStencilAttachment = &depth_ref;
    }

    // Create render pass
    vk::RenderPassCreateInfo render_pass_info{};
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    vk::RenderPass render_pass = device->raw().createRenderPass(render_pass_info);

    auto result = std::make_shared<RenderPass>();
    result->raw = render_pass;
    result->framebuffer_cache =
        std::make_shared<FramebufferCache>(render_pass, desc.color_attachments, desc.depth_attachment);

    spdlog::info("Created render pass with {} color attachments, depth: {}", desc.color_attachments.size(),
                 desc.depth_attachment.has_value());

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

vk::ShaderStageFlags ShaderPipelineStageToVk(ShaderPipelineStage stage)
{
    switch (stage)
    {
    case ShaderPipelineStage::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
    case ShaderPipelineStage::Pixel:
        return vk::ShaderStageFlagBits::eFragment;
    case ShaderPipelineStage::RayGen:
        return vk::ShaderStageFlagBits::eRaygenKHR;
    case ShaderPipelineStage::RayMiss:
        return vk::ShaderStageFlagBits::eMissKHR;
    case ShaderPipelineStage::RayClosestHit:
        return vk::ShaderStageFlagBits::eClosestHitKHR;
    }
    return vk::ShaderStageFlagBits::eVertex; // Fallback
}

// ============================================================================
// Descriptor Set Layout Creation
// ============================================================================

// Helper: Parse sampler name (e.g., "sampler_llr" -> linear/linear/repeat)
static std::optional<SamplerDesc> ParseSamplerName(const std::string& name)
{
    const std::string prefix = "sampler_";
    if (!name.starts_with(prefix) || name.length() < prefix.length() + 3)
    {
        return std::nullopt;
    }

    std::string spec = name.substr(prefix.length());

    // Parse texel filter (n=nearest, l=linear)
    vk::Filter texel_filter;
    if (spec[0] == 'n')
        texel_filter = vk::Filter::eNearest;
    else if (spec[0] == 'l')
        texel_filter = vk::Filter::eLinear;
    else
        return std::nullopt;

    // Parse mipmap mode (n=nearest, l=linear)
    vk::SamplerMipmapMode mipmap_mode;
    if (spec[1] == 'n')
        mipmap_mode = vk::SamplerMipmapMode::eNearest;
    else if (spec[1] == 'l')
        mipmap_mode = vk::SamplerMipmapMode::eLinear;
    else
        return std::nullopt;

    // Parse address mode (r=repeat, mr=mirrored_repeat, c=clamp, cb=clamp_to_border)
    vk::SamplerAddressMode address_mode;
    std::string addr = spec.substr(2);
    if (addr == "r")
        address_mode = vk::SamplerAddressMode::eRepeat;
    else if (addr == "mr")
        address_mode = vk::SamplerAddressMode::eMirroredRepeat;
    else if (addr == "c")
        address_mode = vk::SamplerAddressMode::eClampToEdge;
    else if (addr == "cb")
        address_mode = vk::SamplerAddressMode::eClampToBorder;
    else
        return std::nullopt;

    return SamplerDesc{texel_filter, mipmap_mode, address_mode};
}

DescriptorSetLayoutResult CreateDescriptorSetLayouts(
    Device* device, const StageDescriptorSetLayouts& descriptor_sets, vk::ShaderStageFlags stage_flags,
    const std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>& set_opts)
{
    DescriptorSetLayoutResult result;

    // Find maximum set index
    uint32_t set_count = 0;
    for (const auto& [set_idx, _] : descriptor_sets)
    {
        set_count = std::max(set_count, set_idx + 1);
    }

    // Also check set_opts for max set index
    for (const auto& opt : set_opts)
    {
        if (opt)
        {
            set_count = std::max(set_count, opt->first + 1);
        }
    }

    // Create layouts for each set
    for (uint32_t set_idx = 0; set_idx < set_count; ++set_idx)
    {
        // Determine stage flags: Set 0 uses provided stage_flags, others use ALL
        vk::ShaderStageFlags current_stage_flags = (set_idx == 0) ? stage_flags : vk::ShaderStageFlagBits::eAll;

        // Find options for this set
        const DescriptorSetLayoutOpts* opts = nullptr;
        DescriptorSetLayoutOpts default_opts;
        for (const auto& opt : set_opts)
        {
            if (opt && opt->first == set_idx)
            {
                opts = &opt->second;
                break;
            }
        }
        if (!opts)
        {
            opts = &default_opts;
        }

        // Get descriptor set bindings (from opts override or shader reflection)
        const DescriptorSetLayout* bindings = nullptr;
        if (opts->replace)
        {
            bindings = &opts->replace.value();
        }
        else
        {
            auto it = descriptor_sets.find(set_idx);
            if (it != descriptor_sets.end())
            {
                bindings = &it->second;
            }
        }

        if (bindings)
        {
            // Create descriptor set layout
            std::vector<vk::DescriptorSetLayoutBinding> vk_bindings;
            std::vector<vk::DescriptorBindingFlags> binding_flags;
            std::vector<vk::Sampler> immutable_samplers_storage; // Keep samplers alive
            vk::DescriptorSetLayoutCreateFlags layout_flags{};

            for (const auto& [binding_idx, binding] : *bindings)
            {
                vk::DescriptorSetLayoutBinding vk_binding{};
                vk_binding.binding = binding_idx;
                vk_binding.descriptorType = binding.type;
                vk_binding.stageFlags = current_stage_flags;

                // Handle descriptor count and bindless
                if (binding.dimensionality == DescriptorDimensionality::RuntimeArray)
                {
                    // Bindless runtime array
                    vk_binding.descriptorCount = device->max_bindless_descriptor_count();
                    layout_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;

                    binding_flags.push_back(vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                                            vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending |
                                            vk::DescriptorBindingFlagBits::ePartiallyBound |
                                            vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
                }
                else
                {
                    vk_binding.descriptorCount = binding.count;
                    binding_flags.push_back(vk::DescriptorBindingFlagBits::ePartiallyBound);
                }

                // Handle immutable samplers
                if (binding.type == vk::DescriptorType::eSampler)
                {
                    auto sampler_desc = ParseSamplerName(binding.name);
                    if (sampler_desc)
                    {
                        vk::Sampler sampler = device->get_sampler(*sampler_desc);
                        immutable_samplers_storage.push_back(sampler);
                        vk_binding.pImmutableSamplers = &immutable_samplers_storage.back();
                    }
                }

                // Make storage buffers dynamic if name ends with "_dyn"
                if (binding.type == vk::DescriptorType::eStorageBuffer && binding.name.ends_with("_dyn"))
                {
                    vk_binding.descriptorType = vk::DescriptorType::eStorageBufferDynamic;
                }

                // Make uniform buffers always dynamic (kajiya convention)
                if (binding.type == vk::DescriptorType::eUniformBuffer)
                {
                    vk_binding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
                }

                vk_bindings.push_back(vk_binding);
            }

            // Create descriptor set layout
            vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
            binding_flags_info.bindingCount = static_cast<uint32_t>(binding_flags.size());
            binding_flags_info.pBindingFlags = binding_flags.data();

            vk::DescriptorSetLayoutCreateInfo layout_info{};
            layout_info.flags = opts->flags.value_or(vk::DescriptorSetLayoutCreateFlags{}) | layout_flags;
            layout_info.bindingCount = static_cast<uint32_t>(vk_bindings.size());
            layout_info.pBindings = vk_bindings.data();
            layout_info.pNext = &binding_flags_info;

            vk::DescriptorSetLayout layout = device->raw().createDescriptorSetLayout(layout_info);
            result.layouts.push_back(layout);

            // Store layout info for descriptor pool creation
            std::unordered_map<uint32_t, vk::DescriptorType> layout_info_map;
            for (const auto& vk_binding : vk_bindings)
            {
                layout_info_map[vk_binding.binding] = vk_binding.descriptorType;
            }
            result.layout_info.push_back(layout_info_map);

            spdlog::debug("Created descriptor set layout for set {}: {} bindings", set_idx, vk_bindings.size());
        }
        else
        {
            // Empty set - create empty layout
            vk::DescriptorSetLayoutCreateInfo layout_info{};
            vk::DescriptorSetLayout layout = device->raw().createDescriptorSetLayout(layout_info);
            result.layouts.push_back(layout);
            result.layout_info.push_back({});
        }
    }

    return result;
}

// ============================================================================
// Shader Stage Layout Merging
// ============================================================================

StageDescriptorSetLayouts MergeShaderStageLayouts(const std::vector<StageDescriptorSetLayouts>& stages)
{
    if (stages.empty())
    {
        return {};
    }

    // Start with first stage
    StageDescriptorSetLayouts result = stages[0];

    // Merge remaining stages
    for (size_t i = 1; i < stages.size(); ++i)
    {
        const auto& stage = stages[i];

        for (const auto& [set_idx, set_bindings] : stage)
        {
            auto& result_set = result[set_idx];

            for (const auto& [binding_idx, binding] : set_bindings)
            {
                auto it = result_set.find(binding_idx);
                if (it == result_set.end())
                {
                    // Binding doesn't exist in result - add it
                    result_set[binding_idx] = binding;
                }
                else
                {
                    // Binding exists - verify it matches
                    const auto& existing = it->second;
                    if (existing.type != binding.type)
                    {
                        spdlog::error("Binding mismatch: set={}, binding={}, name={}", set_idx, binding_idx,
                                      binding.name);
                        spdlog::error("  Existing type: {}", vk::to_string(existing.type));
                        spdlog::error("  New type: {}", vk::to_string(binding.type));
                        throw std::runtime_error("Descriptor binding type mismatch between shader stages");
                    }
                    if (existing.name != binding.name)
                    {
                        spdlog::warn("Binding name mismatch: set={}, binding={}", set_idx, binding_idx);
                        spdlog::warn("  Existing name: {}", existing.name);
                        spdlog::warn("  New name: {}", binding.name);
                    }
                }
            }
        }
    }

    return result;
}

// ============================================================================
// Compute Pipeline Creation
// TODO: Implement this function (critical)
// ============================================================================

ComputePipeline CreateComputePipeline(Device* device, const std::vector<uint8_t>& spirv,
                                      const ComputePipelineDesc& desc)
{
    // TODO: Create a compute pipeline from SPIR-V bytecode
    //
    // This function needs to:
    //
    // 1. Reflect SPIR-V to get descriptor set layouts
    //    - Use SPIRV-Cross or SPIRV-Reflect library
    //    - Extract binding information for each descriptor set
    //
    // 2. Create descriptor set layouts
    //    - Call CreateDescriptorSetLayouts with reflection data
    //
    // 3. Create pipeline layout
    //    - Use descriptor set layouts from step 2
    //    - Add push constant range if desc.push_constants_bytes > 0
    //
    // 4. Create shader module
    //    - Convert byte vector to uint32_t SPIR-V code
    //    - Create VkShaderModule
    //
    // 5. Create compute pipeline
    //    - Use shader module and pipeline layout
    //    - Entry point is desc.source.GetEntry()
    //
    // 6. Extract compute shader workgroup size from SPIR-V
    //    - Use ShaderCompiler::GetComputeShaderLocalSize
    //
    // 7. Calculate descriptor pool sizes
    //    - Iterate through all bindings in all sets
    //    - Count descriptors by type
    //
    // Reference implementation: kajiya/src/vulkan/shader.rs:387-474

    spdlog::error("CreateComputePipeline - TODO: Implement compute pipeline creation");
    spdlog::error("This is a critical function. Without it:");
    spdlog::error("  - Cannot run any compute shaders");
    spdlog::error("  - Most rendering techniques in kajiya use compute shaders");
    spdlog::error("");
    spdlog::error("Required dependencies:");
    spdlog::error("  - SPIRV-Cross or SPIRV-Reflect for shader reflection");
    spdlog::error("  - CreateDescriptorSetLayouts (see above)");

    // Return empty result as placeholder
    ComputePipeline result;
    result.group_size = {1, 1, 1}; // Default workgroup size
    return result;
}

// ============================================================================
// Raster Pipeline Creation
// TODO: Implement this function (critical)
// ============================================================================

RasterPipeline CreateRasterPipeline(Device* device, const std::vector<PipelineShader<std::vector<uint8_t>>>& shaders,
                                    const RasterPipelineDesc& desc)
{
    // TODO: Create a graphics/raster pipeline from multiple shader stages
    //
    // This function needs to:
    //
    // 1. Reflect all shader stages to get descriptor sets
    //    - Use SPIRV-Cross/SPIRV-Reflect on each shader
    //    - Store in vector of StageDescriptorSetLayouts
    //
    // 2. Merge descriptor sets from all stages
    //    - Call MergeShaderStageLayouts
    //
    // 3. Create descriptor set layouts
    //    - Call CreateDescriptorSetLayouts with merged layouts
    //    - Use vk::ShaderStageFlags::eAllGraphics for stage_flags
    //
    // 4. Create pipeline layout
    //    - Use descriptor set layouts from step 3
    //    - Add push constant range if desc.push_constants_bytes > 0
    //
    // 5. Create shader modules for each stage
    //    - Convert byte vectors to SPIR-V
    //    - Create VkShaderModule for each
    //
    // 6. Configure graphics pipeline state:
    //    - Vertex input: No vertex buffers (uses vertex pulling)
    //    - Input assembly: Triangle list
    //    - Viewport/scissor: Dynamic state
    //    - Rasterization:
    //      * Front face: counter-clockwise
    //      * Cull mode: back (if desc.face_cull), else none
    //      * Polygon mode: fill
    //    - Multisample: 1 sample
    //    - Depth/stencil:
    //      * Depth test: enabled
    //      * Depth write: desc.depth_write
    //      * Depth compare: greater-or-equal (reverse Z)
    //    - Color blend:
    //      * No blending
    //      * Color write mask: all
    //      * One attachment per color target in render pass
    //
    // 7. Create graphics pipeline
    //    - Use shader stages, pipeline layout, and state
    //    - Use desc.render_pass for render pass
    //
    // 8. Calculate descriptor pool sizes
    //    - Same as compute pipeline
    //
    // Reference implementation: kajiya/src/vulkan/shader.rs:822-1009

    spdlog::error("CreateRasterPipeline - TODO: Implement raster pipeline creation");
    spdlog::error("This is a critical function. Without it:");
    spdlog::error("  - Cannot render any geometry");
    spdlog::error("  - Cannot use any graphics shaders (vertex + pixel)");
    spdlog::error("");
    spdlog::error("Required dependencies:");
    spdlog::error("  - SPIRV-Cross or SPIRV-Reflect for shader reflection");
    spdlog::error("  - MergeShaderStageLayouts (see above)");
    spdlog::error("  - CreateDescriptorSetLayouts (see above)");
    spdlog::error("");
    spdlog::error("Pipeline state configuration:");
    spdlog::error("  - No vertex buffers (vertex pulling pattern)");
    spdlog::error("  - Reverse Z depth (depth_compare: greater_or_equal)");
    spdlog::error("  - Dynamic viewport/scissor");
    spdlog::error("  - Configurable face culling: {}", desc.face_cull);
    spdlog::error("  - Configurable depth write: {}", desc.depth_write);

    // Return empty result as placeholder
    RasterPipeline result;
    return result;
}

} // namespace tekki::backend::vulkan
