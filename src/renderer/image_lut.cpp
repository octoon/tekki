#include "tekki/renderer/image_lut.h"
#include <memory>
#include <stdexcept>

namespace tekki::renderer {

std::shared_ptr<tekki::backend::vulkan::Image> ComputeImageLut::Create(std::shared_ptr<tekki::backend::Device> device) {
    throw std::runtime_error("Pure virtual function ComputeImageLut::Create called");
}

void ComputeImageLut::Compute(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph, std::shared_ptr<tekki::render_graph::Handle<tekki::backend::vulkan::Image>> image) {
    throw std::runtime_error("Pure virtual function ComputeImageLut::Compute called");
}

ImageLut::ImageLut(std::shared_ptr<tekki::backend::Device> device, std::shared_ptr<ComputeImageLut> computer)
    : computer(computer), computed(false) {
    image = computer->Create(device);
}

void ImageLut::ComputeIfNeeded(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph) {
    if (computed) {
        return;
    }

    auto renderGraphImage = renderGraph->Import(image, tekki::backend::vk_sync::AccessType::Nothing);

    computer->Compute(renderGraph, renderGraphImage);

    renderGraph->Export(
        renderGraphImage,
        tekki::backend::vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
    );

    computed = true;
}

/// Note: contains garbage until `ComputeIfNeeded` is called.
std::shared_ptr<tekki::backend::vulkan::Image> ImageLut::BackingImage() const {
    return image;
}

}