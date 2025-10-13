#include "tekki/render_graph/pass_builder.h"
#include <stdexcept>
#include <algorithm>
#include <functional>

namespace tekki::render_graph {

PassBuilder::PassBuilder(RenderGraph& rg, const std::string& name, size_t passIdx)
    : rg_(rg), passIdx_(passIdx), pass_(std::make_unique<RecordedPass>(name, passIdx)) {
}

PassBuilder::~PassBuilder() {
    rg_.RecordPass(*pass_);
}

RgComputePipelineHandle PassBuilder::RegisterComputePipeline(const std::filesystem::path& path) {
    auto desc = ComputePipelineDesc::Builder()
        .ComputeHlsl(path)
        .Build();
    return RegisterComputePipelineWithDesc(std::move(desc));
}

RgComputePipelineHandle PassBuilder::RegisterComputePipelineWithDesc(ComputePipelineDesc desc) {
    auto id = rg_.ComputePipelines.size();

    for (const auto& [set_idx, layout] : rg_.PredefinedDescriptorSetLayouts) {
        // Convert unordered_map to vector of VkDescriptorSetLayoutBinding
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& [binding_idx, desc_info] : layout.Bindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = binding_idx;
            binding.descriptorType = desc_info.type;
            binding.descriptorCount = desc_info.count;
            binding.stageFlags = desc_info.stageFlags;
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(bindings)
                .Build()
        );
    }

    rg_.ComputePipelines.push_back(RgComputePipeline{ std::move(desc) });

    return RgComputePipelineHandle{ id };
}

RgRasterPipelineHandle PassBuilder::RegisterRasterPipeline(const std::vector<PipelineShaderDesc>& shaders, RasterPipelineDescBuilder descBuilder) {
    auto id = rg_.RasterPipelines.size();
    auto desc = descBuilder.Build();

    for (const auto& [set_idx, layout] : rg_.PredefinedDescriptorSetLayouts) {
        // Convert unordered_map to vector of VkDescriptorSetLayoutBinding
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& [binding_idx, desc_info] : layout.Bindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = binding_idx;
            binding.descriptorType = desc_info.type;
            binding.descriptorCount = desc_info.count;
            binding.stageFlags = desc_info.stageFlags;
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(bindings)
                .Build()
        );
    }

    rg_.RasterPipelines.push_back(RgRasterPipeline{
        .Shaders = shaders,
        .Desc = std::move(desc)
    });

    return RgRasterPipelineHandle{ id };
}

RgRtPipelineHandle PassBuilder::RegisterRayTracingPipeline(const std::vector<PipelineShaderDesc>& shaders, RayTracingPipelineDesc desc) {
    auto id = rg_.RtPipelines.size();

    for (const auto& [set_idx, layout] : rg_.PredefinedDescriptorSetLayouts) {
        // Convert unordered_map to vector of VkDescriptorSetLayoutBinding
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& [binding_idx, desc_info] : layout.Bindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = binding_idx;
            binding.descriptorType = desc_info.type;
            binding.descriptorCount = desc_info.count;
            binding.stageFlags = desc_info.stageFlags;
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(bindings)
                .Build()
        );
    }

    rg_.RtPipelines.push_back(RgRtPipeline{
        .Shaders = shaders,
        .Desc = std::move(desc)
    });

    return RgRtPipelineHandle{ id };
}

void PassBuilder::Render(std::function<void(RenderPassApi&)> render) {
    if (pass_->RenderFn) {
        throw std::runtime_error("Render function already set");
    }

    // Wrap the reference-taking function to a pointer-taking function
    pass_->RenderFn = [render](RenderPassApi* api) {
        render(*api);
    };
}

} // namespace tekki::render_graph