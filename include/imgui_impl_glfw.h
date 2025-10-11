#pragma once

// ImGui GLFW binding stub
// This should normally come from imgui examples, but we create a minimal stub here

struct GLFWwindow;
struct ImGuiContext;

namespace ImGui {

// GLFW backend functions
bool ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window, bool install_callbacks);
void ImGui_ImplGlfw_Shutdown();
void ImGui_ImplGlfw_NewFrame();

} // namespace ImGui
