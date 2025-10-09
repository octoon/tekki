#include "tekki/render_graph/pass_builder.h"
#include <stdexcept>
#include <algorithm>
#include <functional>

namespace tekki::render_graph {

PassBuilder::PassBuilder(RenderGraph& rg, size_t passIdx)
    : rg_(rg), passIdx_(passIdx), pass_(std::make_unique<RecordedPass>()) {
}

PassBuilder::~PassBuilder() {
    rg_.RecordPass(std::move(pass_));
}

RgComputePipelineHandle PassBuilder::RegisterComputePipeline(const std::filesystem::path& path) {
    auto desc = ComputePipelineDesc::Builder()
        .ComputeHlsl(path)
        .Build();
    return RegisterComputePipelineWithDesc(std::move(desc));
}

RgComputePipelineHandle PassBuilder::RegisterComputePipelineWithDesc(ComputePipelineDesc desc) {
    auto id = rg_.compute_pipelines.size();

    for (const auto& [set_idx, layout] : rg_.predefined_descriptor_set_layouts) {
        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(layout.bindings)
                .Build()
        );
    }

    rg_.compute_pipelines.push_back(RgComputePipeline{ std::move(desc) });

    return RgComputePipelineHandle{ id };
}

RgRasterPipelineHandle PassBuilder::RegisterRasterPipeline(const std::vector<PipelineShaderDesc>& shaders, RasterPipelineDescBuilder descBuilder) {
    auto id = rg_.raster_pipelines.size();
    auto desc = descBuilder.Build();

    for (const auto& [set_idx, layout] : rg_.predefined_descriptor_set_layouts) {
        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(layout.bindings)
                .Build()
        );
    }

    rg_.raster_pipelines.push_back(RgRasterPipeline{
        .shaders = shaders,
        .desc = std::move(desc)
    });

    return RgRasterPipelineHandle{ id };
}

RgRtPipelineHandle PassBuilder::RegisterRayTracingPipeline(const std::vector<PipelineShaderDesc>& shaders, RayTracingPipelineDesc desc) {
    auto id = rg_.rt_pipelines.size();

    for (const auto& [set_idx, layout] : rg_.predefined_descriptor_set_layouts) {
        desc.descriptor_set_opts[set_idx] = std::make_pair(
            set_idx,
            DescriptorSetLayoutOpts::Builder()
                .Replace(layout.bindings)
                .Build()
        );
    }

    rg_.rt_pipelines.push_back(RgRtPipeline{
        .shaders = shaders,
        .desc = std::move(desc)
    });

    return RgRtPipelineHandle{ id };
}

void PassBuilder::Render(std::function<void(RenderPassApi&)> render) {
    auto prev = pass_->render_fn;
    pass_->render_fn = std::make_unique<std::function<void(RenderPassApi&)>>(render);
    if (prev) {
        throw std::runtime_error("Render function already set");
    }
}

} // namespace tekki::render_graph