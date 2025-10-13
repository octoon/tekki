# Backend library - Vulkan wrapper and GPU management
add_library(tekki-backend STATIC
    # Backend core
    backend/bytes.cpp
    backend/chunky_list.cpp
    backend/dynamic_constants.cpp
    backend/error.cpp
    backend/file.cpp
    backend/lib.cpp
    backend/pipeline_cache.cpp
    backend/rust_shader_compiler.cpp
    backend/shader_compiler.cpp
    backend/transient_resource_cache.cpp

    # Vulkan backend
    backend/vulkan/barrier.cpp
    backend/vulkan/buffer.cpp
    backend/vulkan/debug_utils.cpp
    backend/vulkan/device.cpp
    backend/vulkan/error.cpp
    backend/vulkan/image.cpp
    backend/vulkan/instance.cpp
    backend/vulkan/mod.cpp
    backend/vulkan/physical_device.cpp
    backend/vulkan/profiler.cpp
    backend/vulkan/ray_tracing.cpp
    backend/vulkan/shader.cpp
    backend/vulkan/surface.cpp
    backend/vulkan/swapchain.cpp
)

target_include_directories(tekki-backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-backend
    PUBLIC
        tekki_compile_options
        tekki-gpu-allocator
        tekki-gpu-profiler
        hassle
        rspirv-reflect
        Vulkan::Vulkan
        spirv-cross-core
        SPIRV-Tools-static
        efsw::efsw
        glm::glm
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glfw
)

# Render graph library
add_library(tekki-rg STATIC
    render_graph/graph.cpp
    render_graph/imageops.cpp
    render_graph/lib.cpp
    render_graph/pass_builder.cpp
    render_graph/temporal.cpp
)

target_include_directories(tekki-rg
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-rg
    PUBLIC
        tekki-backend
)

# Asset library - loading and processing
add_library(tekki-asset STATIC
    asset/image.cpp
    asset/mesh.cpp
)

target_include_directories(tekki-asset
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-asset
    PUBLIC
        tekki-backend
        OpenEXR::OpenEXR
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        tomlplusplus::tomlplusplus
        half::half
        TinyGLTF::TinyGLTF
)

# Rust shaders shared library
add_library(tekki-rust-shaders-shared STATIC
    rust_shaders_shared/camera.cpp
    rust_shaders_shared/frame_constants.cpp
    rust_shaders_shared/gbuffer.cpp
    rust_shaders_shared/lib.cpp
    rust_shaders_shared/mesh.cpp
    rust_shaders_shared/raster_simple.cpp
    rust_shaders_shared/render_overrides.cpp
    rust_shaders_shared/ssgi.cpp
    rust_shaders_shared/util.cpp
    rust_shaders_shared/view_constants.cpp
    rust_shaders_shared/view_ray.cpp
)

target_include_directories(tekki-rust-shaders-shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-rust-shaders-shared
    PUBLIC
        tekki-backend
)

# Rust shaders library
add_library(tekki-rust-shaders STATIC
    rust_shaders/atmosphere.cpp
    rust_shaders/blur.cpp
    rust_shaders/color.cpp
    rust_shaders/constants.cpp
    rust_shaders/copy_depth_to_r.cpp
    rust_shaders/extract_half_res_depth.cpp
    rust_shaders/rev_blur.cpp
    rust_shaders/sky.cpp
)

target_include_directories(tekki-rust-shaders
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-rust-shaders
    PUBLIC
        tekki-backend
        tekki-rust-shaders-shared
)

# Renderer library - main rendering implementation
add_library(tekki-renderer STATIC
    # Renderer core
    renderer/bindless_descriptor_set.cpp
    renderer/buffer_builder.cpp
    renderer/camera.cpp
    renderer/default_world_renderer.cpp
    renderer/frame_desc.cpp
    renderer/image_cache.cpp
    renderer/image_lut.cpp
    renderer/lib.cpp
    renderer/logging.cpp
    renderer/lut_renderers.cpp
    renderer/math.cpp
    renderer/mmap.cpp
    renderer/ui_renderer.cpp
    renderer/world_render_passes.cpp
    renderer/world_renderer.cpp
    renderer/world_renderer_mmap_adapter.cpp

    # Renderer modules
    renderer/renderers/deferred.cpp
    renderer/renderers/dlss.cpp
    renderer/renderers/dof.cpp
    renderer/renderers/half_res.cpp
    renderer/renderers/ibl.cpp
    renderer/renderers/ircache.cpp
    renderer/renderers/lighting.cpp
    renderer/renderers/mod.cpp
    renderer/renderers/motion_blur.cpp
    renderer/renderers/post.cpp
    renderer/renderers/prefix_scan.cpp
    renderer/renderers/raster_meshes.cpp
    renderer/renderers/reference.cpp
    renderer/renderers/reprojection.cpp
    renderer/renderers/rtdgi.cpp
    renderer/renderers/rtr.cpp
    renderer/renderers/shadow_denoise.cpp
    renderer/renderers/shadows.cpp
    renderer/renderers/sky.cpp
    renderer/renderers/ssgi.cpp
    renderer/renderers/taa.cpp
    renderer/renderers/ussgi.cpp
    renderer/renderers/wrc.cpp

    # Old/deprecated renderers (keep for reference)
    renderer/renderers/old/csgi.cpp
    renderer/renderers/old/sdf.cpp
)

target_include_directories(tekki-renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-renderer
    PUBLIC
        tekki-backend
        tekki-rg
        tekki-asset
        tekki-rust-shaders
        tekki-rust-shaders-shared
)

# ImGui backend library
add_library(tekki-imgui STATIC
    kajiya_imgui/imgui_backend.cpp
    kajiya_imgui/lib.cpp
    ash_imgui/lib.cpp
)

target_include_directories(tekki-imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        "C:/Users/jstzw/.conan2/p/b/imgui002a484c061c1/p/res/bindings"
)

target_link_libraries(tekki-imgui
    PUBLIC
        tekki-backend
        imgui::imgui
        glfw
)

# Simple viewer framework
add_library(tekki-simple STATIC
    kajiya_simple/input.cpp
    kajiya_simple/lib.cpp
    kajiya_simple/main_loop.cpp
)

target_include_directories(tekki-simple
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-simple
    PUBLIC
        tekki-backend
        tekki-renderer
        tekki-imgui
        glfw
)

# Asset pipeline library
add_library(tekki-asset-pipe STATIC
    kajiya_asset_pipe/lib.cpp
    kajiya_asset/mesh.cpp
)

target_include_directories(tekki-asset-pipe
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-asset-pipe
    PUBLIC
        tekki-asset
)

# DLSS support library (optional)
if(TEKKI_WITH_DLSS)
    add_library(tekki-dlss STATIC
        ngx_dlss/lib.cpp
    )

    target_include_directories(tekki-dlss
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(tekki-dlss
        PUBLIC
            tekki-backend
    )

    target_compile_definitions(tekki-dlss
        PUBLIC
            TEKKI_WITH_DLSS
    )
endif()

# Link DLSS if enabled
if(TEKKI_WITH_DLSS)
    target_link_libraries(tekki-renderer PRIVATE tekki-dlss)
endif()
