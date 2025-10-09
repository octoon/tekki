#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tekki/core/result.h"

namespace tekki::render_graph {

// Forward declarations
class Graph;
class Hl;
class PassApi;
class PassBuilder;
class Resource;
class ResourceRegistry;
class Temporal;

namespace imageops {
// Image operations module
}

namespace renderer {
// Renderer module
}

// Re-export commonly used types
using Graph = Graph;
using Hl = Hl;
using PassApi = PassApi;
using PassBuilder = PassBuilder;
using Resource = Resource;
using ResourceRegistry = ResourceRegistry;
using Temporal = Temporal;

} // namespace tekki::render_graph