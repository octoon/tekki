// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/shader.rs

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "core/common.h"

namespace tekki::backend::vulkan
{

struct ShaderModuleDesc
{
    std::string name;
    std::vector<uint32_t> spirv_code;
    vk::ShaderStageFlagBits stage;

    ShaderModuleDesc(const std::string& name, const std::vector<uint32_t>& code, vk::ShaderStageFlagBits stage)
        : name(name), spirv_code(code), stage(stage)
    {
    }

    bool operator==(const ShaderModuleDesc& other) const
    {
        return name == other.name && spirv_code == other.spirv_code && stage == other.stage;
    }
};

struct ShaderModuleDescHash
{
    std::size_t operator()(const ShaderModuleDesc& desc) const
    {
        return std::hash<std::string>{}(desc.name) ^ std::hash<size_t>{}(desc.spirv_code.size()) ^
               std::hash<int>{}(static_cast<int>(desc.stage));
    }
};

class ShaderModule
{
public:
    ShaderModule(vk::Device device, const ShaderModuleDesc& desc);
    ~ShaderModule();

    vk::ShaderModule raw() const { return module_; }
    const ShaderModuleDesc& desc() const { return desc_; }
    vk::ShaderStageFlagBits stage() const { return desc_.stage; }

private:
    vk::Device device_;
    vk::ShaderModule module_;
    ShaderModuleDesc desc_;
};

struct PipelineLayoutDesc
{
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

    bool operator==(const PipelineLayoutDesc& other) const
    {
        return descriptor_set_layouts == other.descriptor_set_layouts &&
               push_constant_ranges == other.push_constant_ranges;
    }
};

struct PipelineLayoutDescHash
{
    std::size_t operator()(const PipelineLayoutDesc& desc) const
    {
        std::size_t hash = 0;
        for (auto layout : desc.descriptor_set_layouts)
        {
            hash ^= std::hash<uint64_t>{}(reinterpret_cast<uint64_t>(layout));
        }
        for (const auto& range : desc.push_constant_ranges)
        {
            hash ^= std::hash<uint32_t>{}(range.offset) ^ std::hash<uint32_t>{}(range.size);
        }
        return hash;
    }
};

class PipelineLayout
{
public:
    PipelineLayout(vk::Device device, const PipelineLayoutDesc& desc);
    ~PipelineLayout();

    vk::PipelineLayout raw() const { return layout_; }
    const PipelineLayoutDesc& desc() const { return desc_; }

private:
    vk::Device device_;
    vk::PipelineLayout layout_;
    PipelineLayoutDesc desc_;
};

struct GraphicsPipelineDesc
{
    std::vector<std::shared_ptr<ShaderModule>> shader_modules;
    std::shared_ptr<PipelineLayout> layout;
    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    vk::PipelineViewportStateCreateInfo viewport_state;
    vk::PipelineRasterizationStateCreateInfo rasterization;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    vk::PipelineColorBlendStateCreateInfo color_blend;
    vk::PipelineDynamicStateCreateInfo dynamic_state;

    std::string name;
};

class GraphicsPipeline
{
public:
    GraphicsPipeline(vk::Device device, const GraphicsPipelineDesc& desc);
    ~GraphicsPipeline();

    vk::Pipeline raw() const { return pipeline_; }
    const GraphicsPipelineDesc& desc() const { return desc_; }

private:
    vk::Device device_;
    vk::Pipeline pipeline_;
    GraphicsPipelineDesc desc_;
};

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Compile HLSL/GLSL to SPIR-V
    std::vector<uint32_t> compile_from_source(const std::string& source, const std::string& entry_point,
                                              vk::ShaderStageFlagBits stage,
                                              const std::vector<std::string>& defines = {});

    // Load SPIR-V from file
    std::vector<uint32_t> load_spirv(const std::string& filename);

    // Cache management
    void set_cache_directory(const std::string& path);
    void clear_cache();

private:
    std::string cache_directory_;
    std::unordered_map<std::string, std::vector<uint32_t>> cache_;
};

class PipelineCache
{
public:
    PipelineCache(const std::shared_ptr<class Device>& device);
    ~PipelineCache();

    // Shader module cache
    std::shared_ptr<ShaderModule> get_or_create_shader_module(const ShaderModuleDesc& desc);

    // Pipeline layout cache
    std::shared_ptr<PipelineLayout> get_or_create_pipeline_layout(const PipelineLayoutDesc& desc);

    // Graphics pipeline cache
    std::shared_ptr<GraphicsPipeline> get_or_create_graphics_pipeline(const GraphicsPipelineDesc& desc);

    // Clear all caches
    void clear();

private:
    std::shared_ptr<class Device> device_;

    std::unordered_map<ShaderModuleDesc, std::shared_ptr<ShaderModule>, ShaderModuleDescHash> shader_cache_;
    std::unordered_map<PipelineLayoutDesc, std::shared_ptr<PipelineLayout>, PipelineLayoutDescHash> layout_cache_;
    std::unordered_map<GraphicsPipelineDesc, std::shared_ptr<GraphicsPipeline>> pipeline_cache_;
};

} // namespace tekki::backend::vulkan