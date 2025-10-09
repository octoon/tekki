#include "tekki/rust_shaders_shared/lib.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <stdexcept>

namespace tekki::rust_shaders_shared {

// Camera module
Camera::Camera() = default;
Camera::~Camera() = default;

// Frame constants module
FrameConstants::FrameConstants() = default;
FrameConstants::~FrameConstants() = default;

// GBuffer module
GBuffer::GBuffer() = default;
GBuffer::~GBuffer() = default;

// Mesh module
Mesh::Mesh() = default;
Mesh::~Mesh() = default;

// Raster simple module
RasterSimple::RasterSimple() = default;
RasterSimple::~RasterSimple() = default;

// Render overrides module
RenderOverrides::RenderOverrides() = default;
RenderOverrides::~RenderOverrides() = default;

// SSGI module
Ssgi::Ssgi() = default;
Ssgi::~Ssgi() = default;

// Util module
Util::Util() = default;
Util::~Util() = default;

// View constants module
ViewConstants::ViewConstants() = default;
ViewConstants::~ViewConstants() = default;

// View ray module
ViewRay::ViewRay() = default;
ViewRay::~ViewRay() = default;

} // namespace tekki::rust_shaders_shared