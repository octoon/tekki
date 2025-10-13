#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/device.h"
#include "tekki/render_graph/graph.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/barrier.h"

namespace tekki::renderer {

// Forward declaration
namespace rg = tekki::render_graph;

class ComputeImageLut {
public:
    virtual ~ComputeImageLut() = default;
    virtual std::shared_ptr<tekki::backend::vulkan::Image> Create(std::shared_ptr<tekki::backend::vulkan::Device> device) = 0;
    virtual void Compute(rg::RenderGraph& renderGraph, rg::Handle<tekki::backend::vulkan::Image>& image) = 0;
};

class BrdfFgLutComputer : public ComputeImageLut {
public:
    BrdfFgLutComputer() = default;

    std::shared_ptr<tekki::backend::vulkan::Image> Create(std::shared_ptr<tekki::backend::vulkan::Device> device) override;
    void Compute(rg::RenderGraph& renderGraph, rg::Handle<tekki::backend::vulkan::Image>& image) override;
};

class BezoldBruckeLutComputer : public ComputeImageLut {
public:
    BezoldBruckeLutComputer() = default;

    std::shared_ptr<tekki::backend::vulkan::Image> Create(std::shared_ptr<tekki::backend::vulkan::Device> device) override;
    void Compute(rg::RenderGraph& renderGraph, rg::Handle<tekki::backend::vulkan::Image>& image) override;
};

} // namespace tekki::renderer