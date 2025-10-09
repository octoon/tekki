#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace tekki::view {

/**
 * Kajiya scene viewer.
 */
class Opt {
public:
    Opt() = default;

    // Getters
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    float GetTemporalUpsampling() const { return temporal_upsampling; }
    const std::optional<std::filesystem::path>& GetScene() const { return scene; }
    const std::optional<std::filesystem::path>& GetMesh() const { return mesh; }
    float GetMeshScale() const { return mesh_scale; }
    bool GetNoVsync() const { return no_vsync; }
    bool GetNoWindowDecorations() const { return no_window_decorations; }
    bool GetFullscreen() const { return fullscreen; }
    bool GetGraphicsDebugging() const { return graphics_debugging; }
    const std::optional<size_t>& GetPhysicalDeviceIndex() const { return physical_device_index; }
    const std::optional<std::filesystem::path>& GetKeymap() const { return keymap; }

    // Setters
    void SetWidth(uint32_t value) { width = value; }
    void SetHeight(uint32_t value) { height = value; }
    void SetTemporalUpsampling(float value) { temporal_upsampling = value; }
    void SetScene(const std::optional<std::filesystem::path>& value) { scene = value; }
    void SetMesh(const std::optional<std::filesystem::path>& value) { mesh = value; }
    void SetMeshScale(float value) { mesh_scale = value; }
    void SetNoVsync(bool value) { no_vsync = value; }
    void SetNoWindowDecorations(bool value) { no_window_decorations = value; }
    void SetFullscreen(bool value) { fullscreen = value; }
    void SetGraphicsDebugging(bool value) { graphics_debugging = value; }
    void SetPhysicalDeviceIndex(const std::optional<size_t>& value) { physical_device_index = value; }
    void SetKeymap(const std::optional<std::filesystem::path>& value) { keymap = value; }

private:
    uint32_t width = 1920;
    uint32_t height = 1080;
    float temporal_upsampling = 1.0f;
    std::optional<std::filesystem::path> scene;
    std::optional<std::filesystem::path> mesh;
    float mesh_scale = 1.0f;
    bool no_vsync = false;
    bool no_window_decorations = false;
    bool fullscreen = false;
    bool graphics_debugging = false;
    std::optional<size_t> physical_device_index;
    std::optional<std::filesystem::path> keymap;
};

} // namespace tekki::view