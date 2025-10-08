#pragma once

// Main render graph module header
// This provides the public interface for the render graph system

#include "graph.h"
#include "pass_api.h"
#include "pass_builder.h"
#include "resource.h"
#include "resource_registry.h"
#include "temporal.h"

namespace tekki::render_graph
{

// Main types exported from the render graph module
using RenderGraph = RenderGraph;
using PassBuilder = PassBuilder;
using ResourceRegistry = ResourceRegistry;
using RenderPassApi = RenderPassApi;
using TemporalRenderGraph = TemporalRenderGraph;

// Resource types
template <typename T> using Handle = Handle<T>;

template <typename T> using ExportedHandle = ExportedHandle<T>;

template <typename T, typename V> using Ref = Ref<T, V>;

// View types
using GpuSrv = GpuSrv;
using GpuUav = GpuUav;
using GpuRt = GpuRt;

// Pipeline handles
using RgComputePipelineHandle = RgComputePipelineHandle;
using RgRasterPipelineHandle = RgRasterPipelineHandle;
using RgRtPipelineHandle = RgRtPipelineHandle;

// Execution types
using CompiledRenderGraph = CompiledRenderGraph;
using ExecutingRenderGraph = ExecutingRenderGraph;
using RetiredRenderGraph = RetiredRenderGraph;

// Temporal types
using TemporalResourceKey = TemporalResourceKey;
using TemporalRenderGraphState = TemporalRenderGraphState;
using ExportedTemporalRenderGraphState = ExportedTemporalRenderGraphState;

} // namespace tekki::render_graph