#include "tekki/view/opt.h"

namespace tekki::view {

Opt::Opt() = default;

uint32_t Opt::GetWidth() const { return width; }
uint32_t Opt::GetHeight() const { return height; }
float Opt::GetTemporalUpsampling() const { return temporal_upsampling; }
const std::optional<std::filesystem::path>& Opt::GetScene() const { return scene; }
const std::optional<std::filesystem::path>& Opt::GetMesh() const { return mesh; }
float Opt::GetMeshScale() const { return mesh_scale; }
bool Opt::GetNoVsync() const { return no_vsync; }
bool Opt::GetNoWindowDecorations() const { return no_window_decorations; }
bool Opt::GetFullscreen() const { return fullscreen; }
bool Opt::GetGraphicsDebugging() const { return graphics_debugging; }
const std::optional<size_t>& Opt::GetPhysicalDeviceIndex() const { return physical_device_index; }
const std::optional<std::filesystem::path>& Opt::GetKeymap() const { return keymap; }

void Opt::SetWidth(uint32_t value) { width = value; }
void Opt::SetHeight(uint32_t value) { height = value; }
void Opt::SetTemporalUpsampling(float value) { temporal_upsampling = value; }
void Opt::SetScene(const std::optional<std::filesystem::path>& value) { scene = value; }
void Opt::SetMesh(const std::optional<std::filesystem::path>& value) { mesh = value; }
void Opt::SetMeshScale(float value) { mesh_scale = value; }
void Opt::SetNoVsync(bool value) { no_vsync = value; }
void Opt::SetNoWindowDecorations(bool value) { no_window_decorations = value; }
void Opt::SetFullscreen(bool value) { fullscreen = value; }
void Opt::SetGraphicsDebugging(bool value) { graphics_debugging = value; }
void Opt::SetPhysicalDeviceIndex(const std::optional<size_t>& value) { physical_device_index = value; }
void Opt::SetKeymap(const std::optional<std::filesystem::path>& value) { keymap = value; }

} // namespace tekki::view