#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <functional>
#include <string>
#include <chrono>
#include <glm/glm.hpp>

#include "tekki/renderer/frame_desc.h"
#include "tekki/renderer/world_renderer.h"
#include "tekki/renderer/ui_renderer.h"
#include "tekki/backend/vulkan/render_backend.h"
#include "tekki/rg/renderer.h"
#include "winit/window.h"
#include "winit/event_loop.h"
#include "winit/events.h"

#ifdef TEKKI_FEATURE_DEAR_IMGUI
#include "tekki/imgui/imgui_backend.h"
#include "imgui.h"
#endif

#ifdef TEKKI_FEATURE_PUFFIN_SERVER
#include "puffin_http/server.h"
#include "puffin.h"
#endif

namespace tekki::kajiya_simple {

struct FrameContext {
    float DtFiltered;
    glm::u32vec2 RenderExtent;
    const std::vector<winit::Event>& Events;
    WorldRenderer& WorldRenderer;
    winit::Window& Window;

#ifdef TEKKI_FEATURE_DEAR_IMGUI
    std::optional<class ImguiContext> Imgui;
#endif

    FrameContext(float dtFiltered, const glm::u32vec2& renderExtent, const std::vector<winit::Event>& events, 
                 WorldRenderer& worldRenderer, winit::Window& window)
        : DtFiltered(dtFiltered)
        , RenderExtent(renderExtent)
        , Events(events)
        , WorldRenderer(worldRenderer)
        , Window(window)
    {}

    float AspectRatio() const {
        return static_cast<float>(RenderExtent.x) / static_cast<float>(RenderExtent.y);
    }
};

#ifdef TEKKI_FEATURE_DEAR_IMGUI
class ImguiContext {
public:
    ImguiContext(ImGuiContext* imgui, ImGuiBackend* imguiBackend, UiRenderer* uiRenderer, 
                 winit::Window* window, float dtFiltered)
        : Imgui(imgui)
        , ImguiBackend(imguiBackend)
        , UiRenderer(uiRenderer)
        , Window(window)
        , DtFiltered(dtFiltered)
    {}

    void Frame(std::function<void(ImGuiContext*)> callback) {
        auto* ui = ImguiBackend->PrepareFrame(Window, Imgui, DtFiltered);
        callback(ui);
        ImguiBackend->FinishFrame(ui, Window, UiRenderer);
    }

private:
    ImGuiContext* Imgui;
    ImGuiBackend* ImguiBackend;
    UiRenderer* UiRenderer;
    winit::Window* Window;
    float DtFiltered;
};
#endif

enum class WindowScale {
    Exact,
    SystemNative
};

enum class FullscreenMode {
    Borderless,
    Exclusive
};

class SimpleMainLoopBuilder {
public:
    SimpleMainLoopBuilder();
    
    SimpleMainLoopBuilder& Resolution(const glm::u32vec2& resolution);
    SimpleMainLoopBuilder& Vsync(bool vsync);
    SimpleMainLoopBuilder& GraphicsDebugging(bool graphicsDebugging);
    SimpleMainLoopBuilder& PhysicalDeviceIndex(std::optional<size_t> physicalDeviceIndex);
    SimpleMainLoopBuilder& DefaultLogLevel(int defaultLogLevel);
    SimpleMainLoopBuilder& Fullscreen(std::optional<FullscreenMode> fullscreen);
    SimpleMainLoopBuilder& WindowScale(WindowScale windowScale);
    SimpleMainLoopBuilder& TemporalUpsampling(float temporalUpsampling);
    
    std::unique_ptr<class SimpleMainLoop> Build(const winit::WindowBuilder& windowBuilder);

private:
    glm::u32vec2 Resolution;
    bool Vsync;
    std::optional<FullscreenMode> FullscreenMode;
    bool GraphicsDebugging;
    std::optional<size_t> PhysicalDeviceIndex;
    int DefaultLogLevel;
    WindowScale WindowScale;
    float TemporalUpsampling;
};

class SimpleMainLoop {
public:
    static SimpleMainLoopBuilder Builder();
    
    float WindowAspectRatio() const;
    void Run(std::function<WorldFrameDesc(FrameContext)> frameFn);

    winit::Window& GetWindow() { return Window; }
    WorldRenderer& GetWorldRenderer() { return WorldRenderer; }

private:
    SimpleMainLoop();
    
    struct MainLoopOptional {
#ifdef TEKKI_FEATURE_DEAR_IMGUI
        std::unique_ptr<ImGuiBackend> ImguiBackend;
        std::unique_ptr<ImGuiContext> Imgui;
#endif

#ifdef TEKKI_FEATURE_PUFFIN_SERVER
        std::unique_ptr<puffin_http::Server> PuffinServer;
#endif
    };

    winit::Window Window;
    WorldRenderer WorldRenderer;
    UiRenderer UiRenderer;
    MainLoopOptional Optional;
    std::unique_ptr<winit::EventLoop> EventLoop;
    std::unique_ptr<RenderBackend> RenderBackend;
    std::unique_ptr<rg::Renderer> RgRenderer;
    glm::u32vec2 RenderExtent;
};

} // namespace tekki::kajiya_simple