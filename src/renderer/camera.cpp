#include "tekki/renderer/camera.h"

namespace tekki::renderer {

CameraMatrices CameraBodyMatrices::CalcMatrices(const CameraLens& lens) const {
    auto lensMatrices = lens.CalcMatrices();
    return CameraMatrices{
        lensMatrices.ViewToClip,
        lensMatrices.ClipToView,
        WorldToView,
        ViewToWorld
    };
}

} // namespace tekki::renderer