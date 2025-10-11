#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"
#include "tekki/render_graph/temporal.h"
#include "tekki/render_graph/imageops.h"

namespace tekki::render_graph {

// Re-export commonly used types from submodules
// This allows users to include just lib.h to get access to all render graph types

namespace imageops {
// Image operations module
}

namespace renderer {
// Renderer module
}

} // namespace tekki::render_graph