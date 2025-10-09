#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/Result.h"

namespace tekki::renderer {

// Forward declarations
class Camera;
class DefaultWorldRenderer;
class FrameDesc;
class ImageCache;
class ImageLut;
class Logging;
class LutRenderers;
class Math;
class Mmap;
class Renderers;
class UiRenderer;
class WorldRenderPasses;
class WorldRenderer;
class WorldRendererMmapAdapter;

class BindlessDescriptorSet;
class BufferBuilder;

// External dependencies
namespace asset = kajiya_asset;
namespace backend = kajiya_backend;
namespace rg = kajiya_rg;

// Import render overrides from shared shaders
using rust_shaders_shared::render_overrides::*;

} // namespace tekki::renderer