#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"

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

// External dependencies (C++ versions)
// Note: These were originally Rust crate references, now point to C++ equivalents
// namespace asset = tekki::asset;
// namespace backend = tekki::backend;
// namespace rg = tekki::render_graph;

// Import render overrides from shared shaders
// TODO: This needs to be updated when rust_shaders_shared is fully ported
// using rust_shaders_shared::render_overrides::*;

} // namespace tekki::renderer