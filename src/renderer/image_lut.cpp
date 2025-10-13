#include "tekki/renderer/image_lut.h"
#include <memory>
#include <stdexcept>

namespace tekki::renderer {

ImageLut::ImageLut(std::shared_ptr<tekki::backend::vulkan::Device> device, std::shared_ptr<ComputeImageLut> computer)
    : computer(computer), computed(false) {
    image = computer->Create(device);
}

void ImageLut::ComputeIfNeeded(std::shared_ptr<tekki::render_graph::RenderGraph> renderGraph) {
    if (computed) {
        return;
    }

    auto renderGraphImage = renderGraph->Import(image, vk_sync::AccessType::Nothing);

    computer->Compute(renderGraph, renderGraphImage);

    renderGraph->Export(
        renderGraphImage,
        vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
    );

    computed = true;
}

std::shared_ptr<tekki::backend::vulkan::Image> ImageLut::BackingImage() const {
    return image;
}

}