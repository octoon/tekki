#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace tekki::rust_shaders_shared {

// Camera module
class Camera {
public:
    Camera();
    ~Camera();
    
    // Camera methods will be implemented here
};

// Frame constants module
class FrameConstants {
public:
    FrameConstants();
    ~FrameConstants();
    
    // Frame constants methods will be implemented here
};

// GBuffer module
class GBuffer {
public:
    GBuffer();
    ~GBuffer();
    
    // GBuffer methods will be implemented here
};

// Mesh module
class Mesh {
public:
    Mesh();
    ~Mesh();
    
    // Mesh methods will be implemented here
};

// Raster simple module
class RasterSimple {
public:
    RasterSimple();
    ~RasterSimple();
    
    // Raster simple methods will be implemented here
};

// Render overrides module
class RenderOverrides {
public:
    RenderOverrides();
    ~RenderOverrides();
    
    // Render overrides methods will be implemented here
};

// SSGI module
class Ssgi {
public:
    Ssgi();
    ~Ssgi();
    
    // SSGI methods will be implemented here
};

// Util module
class Util {
public:
    Util();
    ~Util();
    
    // Utility methods will be implemented here
};

// View constants module
class ViewConstants {
public:
    ViewConstants();
    ~ViewConstants();
    
    // View constants methods will be implemented here
};

// View ray module
class ViewRay {
public:
    ViewRay();
    ~ViewRay();
    
    // View ray methods will be implemented here
};

} // namespace tekki::rust_shaders_shared