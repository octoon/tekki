#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "tekki/core/result.h"

namespace tekki::renderer::renderers {

class GbufferDepth {
public:
    std::shared_ptr<class Image> GeometricNormal;
    std::shared_ptr<class Image> Gbuffer;
    std::shared_ptr<class Image> Depth;

    GbufferDepth(
        std::shared_ptr<class Image> geometricNormal,
        std::shared_ptr<class Image> gbuffer,
        std::shared_ptr<class Image> depth
    );

    std::shared_ptr<class Image> HalfViewNormal(class RenderGraph& rg);
    std::shared_ptr<class Image> HalfDepth(class RenderGraph& rg);

private:
    std::shared_ptr<class Image> halfViewNormal;
    std::shared_ptr<class Image> halfDepth;
};

class PingPongTemporalResource {
public:
    std::string OutputTex;
    std::string HistoryTex;

    PingPongTemporalResource(const std::string& name);

    std::pair<std::shared_ptr<class Image>, std::shared_ptr<class Image>> 
    GetOutputAndHistory(class TemporalRenderGraph& rg, const class ImageDesc& desc);

private:
    void SwapTextures();
};

} // namespace tekki::renderer::renderers