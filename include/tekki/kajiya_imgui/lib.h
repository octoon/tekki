#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace tekki::kajiya_imgui {

class ImguiBackend {
public:
    ImguiBackend() = default;
    virtual ~ImguiBackend() = default;

    // Disable copying
    ImguiBackend(const ImguiBackend&) = delete;
    ImguiBackend& operator=(const ImguiBackend&) = delete;

    // Enable moving
    ImguiBackend(ImguiBackend&&) = default;
    ImguiBackend& operator=(ImguiBackend&&) = default;

    /**
     * Initialize the ImGui backend
     */
    virtual void Initialize();

    /**
     * Shutdown the ImGui backend
     */
    virtual void Shutdown();

    /**
     * Begin a new frame
     */
    virtual void NewFrame();

    /**
     * Render the ImGui draw data
     */
    virtual void RenderDrawData();

    /**
     * Handle platform input
     */
    virtual void ProcessInput();

    /**
     * Create fonts texture
     */
    virtual void CreateFontsTexture();

    /**
     * Destroy fonts texture
     */
    virtual void DestroyFontsTexture();

    /**
     * Get the backend instance
     */
    static std::shared_ptr<ImguiBackend> GetInstance();

private:
    static std::shared_ptr<ImguiBackend> s_instance;
};

} // namespace tekki::kajiya_imgui