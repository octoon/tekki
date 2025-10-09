#include "tekki/renderer/lut_renderers.h"
#include <stdexcept>
#include <memory>

namespace tekki::renderer {

std::shared_ptr<tekki::backend::Image> BrdfFgLutComputer::Create(std::shared_ptr<tekki::backend::Device> device) {
    tekki::backend::vulkan::ImageDesc desc;
    desc.ImageType = tekki::backend::vulkan::ImageType::Tex2d;
    desc.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.Extent = {64, 64, 1};
    desc.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.MipLevels = 1;
    desc.ArrayLayers = 1;
    desc.Samples = VK_SAMPLE_COUNT_1_BIT;
    
    try {
        return device->CreateImage(desc, {});
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create BRDF FG LUT image: " + std::string(e.what()));
    }
}

void BrdfFgLutComputer::Compute(std::shared_ptr<tekki::render_graph::Graph> renderGraph, std::shared_ptr<tekki::backend::Image> image) {
    auto pass = renderGraph->AddPass("brdf_fg lut");
    
    auto pipeline = pass->RegisterComputePipeline("/shaders/lut/brdf_fg.hlsl");
    auto imageRef = pass->Write(image, vk_sync::AccessType::ComputeShaderWrite);
    
    pass->Render([pipeline, imageRef](std::shared_ptr<tekki::render_graph::PassApi> api) {
        try {
            auto boundPipeline = api->BindComputePipeline(
                pipeline->IntoBinding()->DescriptorSet(0, {imageRef->Bind()})
            );
            
            boundPipeline->Dispatch(imageRef->GetDesc().Extent);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to execute BRDF FG LUT compute pass: " + std::string(e.what()));
        }
    });
}

std::shared_ptr<tekki::backend::Image> BezoldBruckeLutComputer::Create(std::shared_ptr<tekki::backend::Device> device) {
    tekki::backend::vulkan::ImageDesc desc;
    desc.ImageType = tekki::backend::vulkan::ImageType::Tex2d;
    desc.Format = VK_FORMAT_R16G16_SFLOAT;
    desc.Extent = {64, 1, 1};
    desc.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.MipLevels = 1;
    desc.ArrayLayers = 1;
    desc.Samples = VK_SAMPLE_COUNT_1_BIT;
    
    try {
        return device->CreateImage(desc, {});
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create Bezold-Brucke LUT image: " + std::string(e.what()));
    }
}

void BezoldBruckeLutComputer::Compute(std::shared_ptr<tekki::render_graph::Graph> renderGraph, std::shared_ptr<tekki::backend::Image> image) {
    auto pass = renderGraph->AddPass("bezold_brucke lut");
    
    auto pipeline = pass->RegisterComputePipeline("/shaders/lut/bezold_brucke.hlsl");
    auto imageRef = pass->Write(image, vk_sync::AccessType::ComputeShaderWrite);
    
    pass->Render([pipeline, imageRef](std::shared_ptr<tekki::render_graph::PassApi> api) {
        try {
            auto boundPipeline = api->BindComputePipeline(
                pipeline->IntoBinding()->DescriptorSet(0, {imageRef->Bind()})
            );
            
            boundPipeline->Dispatch(imageRef->GetDesc().Extent);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to execute Bezold-Brucke LUT compute pass: " + std::string(e.what()));
        }
    });
}

}