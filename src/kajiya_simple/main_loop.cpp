#include "tekki/kajiya_simple/main_loop.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string>

#include "tekki/renderer/frame_desc.h"
#include "tekki/renderer/world_renderer.h"
#include "tekki/renderer/ui_renderer.h"
#include "tekki/backend/vulkan/render_backend.h"
#include "tekki/rg/renderer.h"
#include "winit/window.h"
#include "winit/event_loop.h"
#include "winit/events.h"
#include "glm/glm.hpp"

#ifdef TEKKI_FEATURE_DEAR_IMGUI
#include "tekki/imgui/imgui_backend.h"
#include "imgui.h"
#endif

#ifdef TEKKI_FEATURE_PUFFIN_SERVER
#include "puffin_http/server.h"
#include "puffin.h"
#endif

namespace tekki::kajiya_simple {

SimpleMainLoopBuilder::SimpleMainLoopBuilder()
    : Resolution(glm::u32vec2(1280, 720))
    , Vsync(true)
    , FullscreenMode(std::nullopt)
    , GraphicsDebugging(false)
    , PhysicalDeviceIndex(std::nullopt)
    , DefaultLogLevel(2) // Warn level
    , WindowScale(WindowScale::SystemNative)
    , TemporalUpsampling(1.0f)
{
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::Resolution(const glm::u32vec2& resolution) {
    Resolution = resolution;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::Vsync(bool vsync) {
    Vsync = vsync;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::GraphicsDebugging(bool graphicsDebugging) {
    GraphicsDebugging = graphicsDebugging;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::PhysicalDeviceIndex(std::optional<size_t> physicalDeviceIndex) {
    PhysicalDeviceIndex = physicalDeviceIndex;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::DefaultLogLevel(int defaultLogLevel) {
    DefaultLogLevel = defaultLogLevel;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::Fullscreen(std::optional<FullscreenMode> fullscreen) {
    FullscreenMode = fullscreen;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::WindowScale(WindowScale windowScale) {
    WindowScale = windowScale;
    return *this;
}

SimpleMainLoopBuilder& SimpleMainLoopBuilder::TemporalUpsampling(float temporalUpsampling) {
    TemporalUpsampling = std::clamp(temporalUpsampling, 1.0f, 8.0f);
    return *this;
}

std::unique_ptr<SimpleMainLoop> SimpleMainLoopBuilder::Build(const winit::WindowBuilder& windowBuilder) {
    try {
        return SimpleMainLoop::Build(*this, windowBuilder);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to build SimpleMainLoop: ") + e.what());
    }
}

SimpleMainLoopBuilder SimpleMainLoop::Builder() {
    return SimpleMainLoopBuilder();
}

float SimpleMainLoop::WindowAspectRatio() const {
    auto size = Window.GetInnerSize();
    return static_cast<float>(size.x) / static_cast<float>(size.y);
}

void SimpleMainLoop::Run(std::function<WorldFrameDesc(FrameContext)> frameFn) {
    std::vector<winit::Event> events;
    
    auto lastFrameInstant = std::chrono::steady_clock::now();
    std::optional<std::string> lastErrorText;
    
    // Delta times are filtered over _this many_ frames.
    constexpr size_t DT_FILTER_WIDTH = 10;
    
    // Past delta times used for filtering
    std::deque<float> dtQueue;
    dtQueue.reserve(DT_FILTER_WIDTH);
    
    // Fake the first frame's delta time. In the first frame, shaders
    // and pipelines are be compiled, so it will most likely have a spike.
    int fakeDtCountdown = 1;
    
    bool running = true;
    
    while (running) {
        #ifdef TEKKI_FEATURE_PUFFIN_SERVER
        puffin::GlobalProfiler::lock().new_frame();
        #endif
        
        auto gpuFrameStartTime = std::chrono::steady_clock::now();
        
        // Process events
        events.clear();
        EventLoop->RunReturn([&](const winit::Event& event, winit::ControlFlow* controlFlow) {
            #ifdef TEKKI_FEATURE_DEAR_IMGUI
            Optional.ImguiBackend->HandleEvent(&Window, Optional.Imgui.get(), event);
            #endif
            
            #ifdef TEKKI_FEATURE_DEAR_IMGUI
            bool uiWantsMouse = Optional.Imgui->GetIO().WantCaptureMouse;
            #else
            bool uiWantsMouse = false;
            #endif
            
            *controlFlow = winit::ControlFlow::Poll;
            
            bool allowEvent = true;
            
            if (event.IsWindowEvent()) {
                auto windowEvent = event.GetWindowEvent();
                if (windowEvent.GetType() == winit::WindowEventType::CloseRequested) {
                    *controlFlow = winit::ControlFlow::Exit;
                    running = false;
                } else if ((windowEvent.GetType() == winit::WindowEventType::CursorMoved ||
                           windowEvent.GetType() == winit::WindowEventType::MouseInput) && uiWantsMouse) {
                    allowEvent = false;
                }
            } else if (event.GetType() == winit::EventType::MainEventsCleared) {
                *controlFlow = winit::ControlFlow::Exit;
            }
            
            if (allowEvent) {
                events.push_back(event.ToStatic());
            }
        });
        
        // Filter the frame time before passing it to the application and renderer.
        // Fluctuations in frame rendering times cause stutter in animations,
        // and time-dependent effects (such as motion blur).
        //
        // Should applications need unfiltered delta time, they can calculate
        // it themselves, but it's good to pass the filtered time so users
        // don't need to worry about it.
        float dtFiltered;
        {
            auto now = std::chrono::steady_clock::now();
            auto dtDuration = now - lastFrameInstant;
            lastFrameInstant = now;
            
            float dtRaw = std::chrono::duration<float>(dtDuration).count();
            
            // >= because rendering (and thus the spike) happens _after_ this.
            if (fakeDtCountdown >= 0) {
                // First frame. Return the fake value.
                fakeDtCountdown--;
                dtFiltered = std::min(dtRaw, 1.0f / 60.0f);
            } else {
                // Not the first frame. Start averaging.
                if (dtQueue.size() >= DT_FILTER_WIDTH) {
                    dtQueue.pop_front();
                }
                
                dtQueue.push_back(dtRaw);
                dtFiltered = std::accumulate(dtQueue.begin(), dtQueue.end(), 0.0f) / dtQueue.size();
            }
        }
        
        FrameContext frameContext(
            dtFiltered,
            RenderExtent,
            events,
            WorldRenderer,
            Window
        );
        
        #ifdef TEKKI_FEATURE_DEAR_IMGUI
        if (Optional.Imgui) {
            frameContext.Imgui = std::make_optional<ImguiContext>(
                Optional.Imgui.get(),
                Optional.ImguiBackend.get(),
                &UiRenderer,
                &Window,
                dtFiltered
            );
        }
        #endif
        
        WorldFrameDesc frameDesc = frameFn(frameContext);
        
        // Physical window extent in pixels
        glm::u32vec2 swapchainExtent = Window.GetInnerSize();
        
        try {
            RgRenderer->PrepareFrame([&](rg::RenderGraph* rg) {
                // TODO: Set debug hook if available
                // rg->SetDebugHook(WorldRenderer.GetRgDebugHook());
                
                auto mainImg = WorldRenderer.PrepareRenderGraph(rg, frameDesc);
                auto uiImg = UiRenderer.PrepareRenderGraph(rg);
                
                auto swapChain = rg->GetSwapChain();
                rg::SimpleRenderPass::NewCompute(
                    rg->AddPass("final blit"),
                    "/shaders/final_blit.hlsl"
                )
                .Read(&mainImg)
                .Read(&uiImg)
                .Write(&swapChain)
                .Constants(std::make_tuple(
                    mainImg.GetDesc().GetExtentInvExtent2D(),
                    std::array<float, 4>{
                        static_cast<float>(swapchainExtent.x),
                        static_cast<float>(swapchainExtent.y),
                        1.0f / static_cast<float>(swapchainExtent.x),
                        1.0f / static_cast<float>(swapchainExtent.y)
                    }
                ))
                .Dispatch({swapchainExtent.x, swapchainExtent.y, 1});
            });
            
            RgRenderer->DrawFrame(
                [&](rg::DynamicConstants* dynamicConstants) {
                    WorldRenderer.PrepareFrameConstants(dynamicConstants, frameDesc, dtFiltered);
                },
                &RenderBackend->GetSwapchain()
            );
            
            WorldRenderer.RetireFrame();
            lastErrorText = std::nullopt;
            
        } catch (const std::exception& e) {
            std::string errorText = e.what();
            if (errorText != lastErrorText.value_or("")) {
                printf("%s\n", errorText.c_str());
                lastErrorText = errorText;
            }
        }
        
        #ifdef TEKKI_FEATURE_PUFFIN_SERVER
        // TODO: GPU profiling integration
        #endif
    }
}

SimpleMainLoop::SimpleMainLoop()
    : WorldRenderer()
    , UiRenderer()
{
}

std::unique_ptr<SimpleMainLoop> SimpleMainLoop::Build(const SimpleMainLoopBuilder& builder, const winit::WindowBuilder& windowBuilder) {
    // TODO: Set up logging with builder.DefaultLogLevel
    // TODO: Set environment variable "SMOL_THREADS" to "64"
    
    auto windowBuilderCopy = windowBuilder;
    
    // Note: asking for the logical size means that if the OS is using DPI scaling,
    // we'll get a physically larger window (with more pixels).
    // The internal rendering resolution will still be what was asked of the `builder`,
    // and the last blit pass will perform spatial upsampling.
    windowBuilderCopy.WithInnerSize(winit::LogicalSize(
        static_cast<double>(builder.Resolution.x),
        static_cast<double>(builder.Resolution.y)
    ));
    
    auto eventLoop = std::make_unique<winit::EventLoop>();
    
    if (builder.FullscreenMode.has_value()) {
        auto fullscreenMode = builder.FullscreenMode.value();
        if (fullscreenMode == FullscreenMode::Borderless) {
            windowBuilderCopy.WithFullscreen(winit::Fullscreen::Borderless(std::nullopt));
        } else if (fullscreenMode == FullscreenMode::Exclusive) {
            auto monitor = eventLoop->GetPrimaryMonitor();
            if (!monitor) {
                throw std::runtime_error("No monitor found");
            }
            auto videoModes = monitor->GetVideoModes();
            if (videoModes.empty()) {
                throw std::runtime_error("No video modes available");
            }
            windowBuilderCopy.WithFullscreen(winit::Fullscreen::Exclusive(videoModes[0]));
        }
    }
    
    auto window = windowBuilderCopy.Build(*eventLoop);
    if (!window) {
        throw std::runtime_error("Failed to create window");
    }
    
    // Physical window extent in pixels
    glm::u32vec2 swapchainExtent = window->GetInnerSize();
    
    // Find the internal rendering resolution
    glm::u32vec2 renderExtent(
        static_cast<uint32_t>(builder.Resolution.x / builder.TemporalUpsampling),
        static_cast<uint32_t>(builder.Resolution.y / builder.TemporalUpsampling)
    );
    
    printf("Internal rendering extent: %ux%u\n", renderExtent.x, renderExtent.y);
    
    glm::u32vec2 temporalUpscaleExtent = builder.Resolution;
    
    if (builder.TemporalUpsampling != 1.0f) {
        printf("Temporal upscaling extent: %ux%u\n", temporalUpscaleExtent.x, temporalUpscaleExtent.y);
    }
    
    RenderBackendConfig config{
        swapchainExtent,
        builder.Vsync,
        builder.GraphicsDebugging,
        builder.PhysicalDeviceIndex
    };
    
    auto renderBackend = std::make_unique<RenderBackend>(*window, config);
    if (!renderBackend) {
        throw std::runtime_error("Failed to create render backend");
    }
    
    // TODO: Create LazyCache
    // auto lazyCache = LazyCache::Create();
    
    auto worldRenderer = WorldRenderer(
        renderExtent,
        temporalUpscaleExtent,
        *renderBackend
        // , lazyCache
    );
    
    auto uiRenderer = UiRenderer();
    
    auto rgRenderer = std::make_unique<rg::Renderer>(*renderBackend);
    if (!rgRenderer) {
        throw std::runtime_error("Failed to create RG renderer");
    }
    
    MainLoopOptional optional;
    
    #ifdef TEKKI_FEATURE_DEAR_IMGUI
    auto imgui = std::make_unique<ImGuiContext>();
    auto imguiBackend = std::make_unique<ImGuiBackend>(
        rgRenderer->GetDevice(),
        window.get(),
        imgui.get()
    );
    
    imguiBackend->CreateGraphicsResources(swapchainExtent);
    
    optional.ImguiBackend = std::move(imguiBackend);
    optional.Imgui = std::move(imgui);
    #endif
    
    #ifdef TEKKI_FEATURE_PUFFIN_SERVER
    std::string serverAddr = "0.0.0.0:" + std::to_string(puffin_http::DEFAULT_PORT);
    printf("Serving profile data on %s\n", serverAddr.c_str());
    
    puffin::SetScopesOn(true);
    optional.PuffinServer = std::make_unique<puffin_http::Server>(serverAddr);
    #endif
    
    auto mainLoop = std::make_unique<SimpleMainLoop>();
    mainLoop->Window = std::move(*window);
    mainLoop->WorldRenderer = std::move(worldRenderer);
    mainLoop->UiRenderer = std::move(uiRenderer);
    mainLoop->Optional = std::move(optional);
    mainLoop->EventLoop = std::move(eventLoop);
    mainLoop->RenderBackend = std::move(renderBackend);
    mainLoop->RgRenderer = std::move(rgRenderer);
    mainLoop->RenderExtent = renderExtent;
    
    return mainLoop;
}

} // namespace tekki::kajiya_simple