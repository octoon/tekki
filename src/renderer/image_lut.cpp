#include "../../include/tekki/renderer/image_lut.h"
#include "../../include/tekki/core/log.h"

namespace tekki::renderer {

ImageLut::ImageLut(vulkan::Device* device, std::unique_ptr<ComputeImageLut> computer)
    : computer_(std::move(computer)), computed_(false)
{
    if (!computer_) {
        TEKKI_LOG_ERROR("ComputeImageLut computer cannot be null");
        return;
    }

    // Create the backing image using the computer
    image_ = computer_->create(device);
    if (!image_) {
        TEKKI_LOG_ERROR("Failed to create backing image for ImageLut");
        return;
    }

    TEKKI_LOG_DEBUG("ImageLut created with backing image");
}

void ImageLut::compute_if_needed(render_graph::RenderGraph& rg)
{
    if (computed_ || !computer_ || !image_) {
        return;
    }

    TEKKI_LOG_DEBUG("Computing ImageLut");

    // Import the backing image into the render graph
    auto rg_image = rg.import(image_, vulkan::AccessType::Nothing);

    // Run the compute shader to generate LUT data
    computer_->compute(rg, rg_image);

    // Export the image for shader access
    rg.export_resource(rg_image, vulkan::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);

    computed_ = true;
    TEKKI_LOG_DEBUG("ImageLut computation completed");
}

std::shared_ptr<vulkan::Image> ImageLut::backing_image() const
{
    return image_;
}

} // namespace tekki::renderer