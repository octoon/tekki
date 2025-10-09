#include "tekki/renderer/renderers/prefix_scan.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/vulkan/buffer.h"
#include <memory>
#include <cstddef>

namespace tekki::renderer::renderers {

void PrefixScan::InclusivePrefixScanU32_1M(
    std::shared_ptr<tekki::render_graph::Graph> rg,
    std::shared_ptr<tekki::backend::vulkan::Buffer> inputBuf
) {
    constexpr size_t SEGMENT_SIZE = 1024;

    // First pass: prefix scan within segments
    auto pass1 = rg->add_pass("_prefix scan 1");
    auto pass1_hl = std::make_shared<tekki::render_graph::SimpleRenderPass>(
        pass1, "/shaders/prefix_scan/inclusive_prefix_scan.hlsl");
    pass1_hl->write(inputBuf);
    pass1_hl->dispatch({static_cast<uint32_t>(SEGMENT_SIZE * SEGMENT_SIZE / 2), 1, 1}); // TODO: indirect

    // Second pass: compute segment sums
    tekki::backend::vulkan::BufferDesc segmentSumDesc;
    segmentSumDesc.size = sizeof(uint32_t) * SEGMENT_SIZE;
    segmentSumDesc.usage = VkBufferUsageFlags(0);
    segmentSumDesc.memoryUsage = gpu_allocator::MemoryUsage::GPU_ONLY;
    
    auto segmentSumBuf = rg->create_buffer(segmentSumDesc);
    
    auto pass2 = rg->add_pass("_prefix scan 2");
    auto pass2_hl = std::make_shared<tekki::render_graph::SimpleRenderPass>(
        pass2, "/shaders/prefix_scan/inclusive_prefix_scan_segments.hlsl");
    pass2_hl->read(inputBuf);
    pass2_hl->write(segmentSumBuf);
    pass2_hl->dispatch({static_cast<uint32_t>(SEGMENT_SIZE / 2), 1, 1}); // TODO: indirect

    // Third pass: merge segment sums
    auto pass3 = rg->add_pass("_prefix scan merge");
    auto pass3_hl = std::make_shared<tekki::render_graph::SimpleRenderPass>(
        pass3, "/shaders/prefix_scan/inclusive_prefix_scan_merge.hlsl");
    pass3_hl->write(inputBuf);
    pass3_hl->read(segmentSumBuf);
    pass3_hl->dispatch({static_cast<uint32_t>(SEGMENT_SIZE * SEGMENT_SIZE / 2), 1, 1}); // TODO: indirect
}

} // namespace tekki::renderer::renderers