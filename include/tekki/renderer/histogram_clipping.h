#pragma once

namespace tekki::renderer {

// Histogram clipping configuration
// Original Rust: kajiya/src/world_renderer.rs
struct HistogramClipping {
    float low = 0.0f;
    float high = 0.0f;
};

} // namespace tekki::renderer
