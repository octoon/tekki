# Core library - fundamental utilities and types
add_library(tekki-core STATIC
    core/log.cpp
    core/time.cpp
    core/config.cpp
    core/mmap.cpp
)

target_include_directories(tekki-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-core
    PUBLIC
        tekki_compile_options
        glm::glm
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Link filesystem library
if(MSVC)
    # MSVC automatically links filesystem with C++17+
else()
    target_link_libraries(tekki-core PUBLIC stdc++fs)
endif()

# Backend library - Vulkan wrapper and GPU management
add_library(tekki-backend INTERFACE)

target_include_directories(tekki-backend
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-backend
    INTERFACE
        tekki-core
        Vulkan::Vulkan
        vulkan-memory-allocator::vulkan-memory-allocator
        spirv-cross-core
        SPIRV-Tools-static
        efsw::efsw
)

# Render graph library
add_library(tekki-rg STATIC
    render_graph/graph.cpp
    render_graph/resource.cpp
    render_graph/resource_registry.cpp
    render_graph/pass_api.cpp
    render_graph/pass_builder.cpp
    render_graph/temporal.cpp
    render_graph/high_level.cpp
    render_graph/renderer.cpp
    render_graph/image_ops.cpp
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
    asset/image_process.cpp
    asset/mesh.cpp
    asset/gltf_importer.cpp
    asset/gltf_loader.cpp
    asset/gltf_loader_process.cpp
    asset/tangent_calc.cpp
    asset/serialization.cpp
    asset/asset_pipeline.cpp
)

target_include_directories(tekki-asset
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-asset
    PUBLIC
        tekki-core
        tekki-backend
        stb::stb
        OpenEXR::OpenEXR
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        tomlplusplus::tomlplusplus
        half::half
        TinyGLTF::TinyGLTF
)

# Renderer library - main rendering implementation
add_library(tekki-renderer STATIC
    # Renderer modules
    renderer/renderers/dof.cpp
    renderer/renderers/ibl.cpp
    renderer/renderers/ircache.cpp
    renderer/renderers/lighting.cpp
    renderer/renderers/motion_blur.cpp
    renderer/renderers/post.cpp
    renderer/renderers/renderers.cpp
    renderer/renderers/rtdgi.cpp
    renderer/renderers/rtr.cpp
    renderer/renderers/ussgi.cpp
    renderer/renderers/wrc.cpp

    # Renderer utilities
    renderer/lut_renderers.cpp
    renderer/half_res.cpp
    renderer/prefix_scan.cpp
    renderer/image_lut.cpp

    # World renderer
    renderer/world/WorldRenderer.cpp
    renderer/world/world_render_passes.cpp
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
)

# Viewer application
add_executable(tekki-view
    viewer/main.cpp
)

target_link_libraries(tekki-view
    PRIVATE
        tekki-renderer
        imgui::imgui
        glfw
        CLI11::CLI11
)
