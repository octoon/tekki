#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "tekki/render_graph/resource.h"

namespace tekki::render_graph {

// Forward declarations
class Graph;
namespace backend {
    namespace vulkan {
        class Image;
        class Buffer;
    }
}

// Resource registry is defined in types.h to avoid duplicate definitions
// This file provides forward declarations and additional utilities

} // namespace tekki::render_graph
