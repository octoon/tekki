#include "tekki/renderer/renderers/mod.h"
#include <memory>
#include <string>
#include <utility>
#include <stdexcept>

namespace tekki::renderer::renderers {

GbufferDepth::GbufferDepth(
    std::shared_ptr<class Image> geometricNormal,
    std::shared_ptr<class Image> gbuffer,
    std::shared_ptr<class Image> depth
) : GeometricNormal(std::move(geometricNormal)),
    Gbuffer(std::move(gbuffer)),
    Depth(std::move(depth)),
    halfViewNormal(nullptr),
    halfDepth(nullptr)
{
}

std::shared_ptr<class Image> GbufferDepth::HalfViewNormal(class RenderGraph& rg) {
    if (!halfViewNormal) {
        try {
            halfViewNormal = half_res::ExtractHalfResGbufferViewNormalRGBA8(rg, Gbuffer);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create half view normal: " + std::string(e.what()));
        }
    }
    return halfViewNormal;
}

std::shared_ptr<class Image> GbufferDepth::HalfDepth(class RenderGraph& rg) {
    if (!halfDepth) {
        try {
            halfDepth = half_res::ExtractHalfResDepth(rg, Depth);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create half depth: " + std::string(e.what()));
        }
    }
    return halfDepth;
}

PingPongTemporalResource::PingPongTemporalResource(const std::string& name)
    : OutputTex(name + ":0"), HistoryTex(name + ":1")
{
}

std::pair<std::shared_ptr<class Image>, std::shared_ptr<class Image>> 
PingPongTemporalResource::GetOutputAndHistory(class TemporalRenderGraph& rg, const class ImageDesc& desc) {
    try {
        auto output_tex = rg.GetOrCreateTemporal(OutputTex, desc);
        auto history_tex = rg.GetOrCreateTemporal(HistoryTex, desc);
        
        SwapTextures();
        
        return std::make_pair(output_tex, history_tex);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get output and history textures: " + std::string(e.what()));
    }
}

void PingPongTemporalResource::SwapTextures() {
    std::swap(OutputTex, HistoryTex);
}

} // namespace tekki::renderer::renderers