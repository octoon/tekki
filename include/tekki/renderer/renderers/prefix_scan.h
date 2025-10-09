#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/buffer.h"

namespace tekki::renderer::renderers {

class PrefixScan {
public:
    /**
     * Performs an inclusive prefix scan on a 1M element buffer of 32-bit unsigned integers
     * @param rg The render graph to add passes to
     * @param inputBuf The input buffer to scan (will be modified in-place)
     */
    static void InclusivePrefixScanU32_1M(
        std::shared_ptr<tekki::render_graph::Graph> rg,
        std::shared_ptr<tekki::backend::vulkan::Buffer> inputBuf
    );
};

} // namespace tekki::renderer::renderers