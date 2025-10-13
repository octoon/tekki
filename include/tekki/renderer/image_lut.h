#pragma once

#include <memory>
#include <vector>
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/render_graph/graph.h"

namespace tekki::renderer {

class ComputeImageLut {
public:
    virtual ~ComputeImageLut() = default;
    virtual std::shared_ptr<tekki::backend::vulkan::Image> Create(std::shared_ptr<tekki::backend::vulkan::Device> device) = 0;
    virtual void Compute(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph, tekki::render_graph::Handle<tekki::backend::vulkan::Image> image) = 0;
};

class ImageLut {
private:
    std::shared_ptr<tekki::backend::vulkan::Image> image;
    std::shared_ptr<ComputeImageLut> computer;
    bool computed;

public:
    ImageLut(std::shared_ptr<tekki::backend::vulkan::Device> device, std::shared_ptr<ComputeImageLut> computer);

    void ComputeIfNeeded(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph);

    /// Note: contains garbage until `ComputeIfNeeded` is called.
    std::shared_ptr<tekki::backend::vulkan::Image> BackingImage() const;
};

}