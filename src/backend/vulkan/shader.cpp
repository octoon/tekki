// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/shader.rs

#include "../../include/tekki/backend/vulkan/shader.h"

#include <fstream>
#include <spdlog/spdlog.h>

namespace tekki::backend::vulkan
{

ShaderModule::ShaderModule(vk::Device device, const ShaderModuleDesc& desc) : device_(device), desc_(desc)
{
    vk::ShaderModuleCreateInfo create_info{.codeSize = desc.spirv_code.size() * sizeof(uint32_t),
                                           .pCode = desc.spirv_code.data()};

    module_ = device_.createShaderModule(create_info);
}

ShaderModule::~ShaderModule()
{
    if (module_)
    {
        device_.destroyShaderModule(module_);
    }
}

PipelineLayout::PipelineLayout(vk::Device device, const PipelineLayoutDesc& desc) : device_(device), desc_(desc)
{
    vk::PipelineLayoutCreateInfo create_info{
        .setLayoutCount = static_cast<uint32_t>(desc.descriptor_set_layouts.size()),
        .pSetLayouts = desc.descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(desc.push_constant_ranges.size()),
        .pPushConstantRanges = desc.push_constant_ranges.data()};

    layout_ = device_.createPipelineLayout(create_info);
}

PipelineLayout::~PipelineLayout()
{
    if (layout_)
    {
        device_.destroyPipelineLayout(layout_);
    }
}

GraphicsPipeline::GraphicsPipeline(vk::Device device, const GraphicsPipelineDesc& desc) : device_(device), desc_(desc)
{
    // Collect shader stages
    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    for (const auto& module : desc.shader_modules)
    {
        stages.push_back(
            vk::PipelineShaderStageCreateInfo{.stage = module->stage(), .module = module->raw(), .pName = "main"});
    }

    vk::GraphicsPipelineCreateInfo pipeline_info{.stageCount = static_cast<uint32_t>(stages.size()),
                                                 .pStages = stages.data(),
                                                 .pVertexInputState = &desc.vertex_input,
                                                 .pInputAssemblyState = &desc.input_assembly,
                                                 .pViewportState = &desc.viewport_state,
                                                 .pRasterizationState = &desc.rasterization,
                                                 .pMultisampleState = &desc.multisample,
                                                 .pDepthStencilState = &desc.depth_stencil,
                                                 .pColorBlendState = &desc.color_blend,
                                                 .pDynamicState = &desc.dynamic_state,
                                                 .layout = desc.layout->raw(),
                                                 .renderPass = VK_NULL_HANDLE, // TODO: Set appropriate render pass
                                                 .subpass = 0};

    pipeline_ = device_.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info).value;
}

GraphicsPipeline::~GraphicsPipeline()
{
    if (pipeline_)
    {
        device_.destroyPipeline(pipeline_);
    }
}

ShaderCompiler::ShaderCompiler()
{
    // TODO: Initialize shader compiler (glslang/dxc)
}

ShaderCompiler::~ShaderCompiler()
{
    // TODO: Cleanup shader compiler
}

std::vector<uint32_t> ShaderCompiler::compile_from_source(const std::string& source, const std::string& entry_point,
                                                          vk::ShaderStageFlagBits stage,
                                                          const std::vector<std::string>& defines)
{
    // TODO: Implement shader compilation
    // This should:
    // 1. Apply defines
    // 2. Compile source to SPIR-V
    // 3. Cache result
    // 4. Return SPIR-V code

    spdlog::warn("Shader compilation not yet implemented");
    return {};
}

std::vector<uint32_t> ShaderCompiler::load_spirv(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open SPIR-V file: " + filename);
    }

    size_t file_size = file.tellg();
    file.seekg(0);

    if (file_size % sizeof(uint32_t) != 0)
    {
        throw std::runtime_error("Invalid SPIR-V file size");
    }

    std::vector<uint32_t> spirv(file_size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), file_size);

    return spirv;
}

void ShaderCompiler::set_cache_directory(const std::string& path)
{
    cache_directory_ = path;
    // TODO: Create directory if it doesn't exist
}

void ShaderCompiler::clear_cache()
{
    cache_.clear();
    // TODO: Clear disk cache if cache_directory_ is set
}

PipelineCache::PipelineCache(const std::shared_ptr<class Device>& device) : device_(device) {}

PipelineCache::~PipelineCache()
{
    clear();
}

std::shared_ptr<ShaderModule> PipelineCache::get_or_create_shader_module(const ShaderModuleDesc& desc)
{
    auto it = shader_cache_.find(desc);
    if (it != shader_cache_.end())
    {
        return it->second;
    }

    auto module = std::make_shared<ShaderModule>(device_->raw(), desc);
    shader_cache_[desc] = module;
    return module;
}

std::shared_ptr<PipelineLayout> PipelineCache::get_or_create_pipeline_layout(const PipelineLayoutDesc& desc)
{
    auto it = layout_cache_.find(desc);
    if (it != layout_cache_.end())
    {
        return it->second;
    }

    auto layout = std::make_shared<PipelineLayout>(device_->raw(), desc);
    layout_cache_[desc] = layout;
    return layout;
}

std::shared_ptr<GraphicsPipeline> PipelineCache::get_or_create_graphics_pipeline(const GraphicsPipelineDesc& desc)
{
    auto it = pipeline_cache_.find(desc);
    if (it != pipeline_cache_.end())
    {
        return it->second;
    }

    auto pipeline = std::make_shared<GraphicsPipeline>(device_->raw(), desc);
    pipeline_cache_[desc] = pipeline;
    return pipeline;
}

void PipelineCache::clear()
{
    shader_cache_.clear();
    layout_cache_.clear();
    pipeline_cache_.clear();
}

} // namespace tekki::backend::vulkan