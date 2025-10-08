# Core library - fundamental utilities and types
add_library(tekki-core STATIC
    core/log.cpp
    core/log.h
    core/error.hpp
    core/common.hpp
    core/time.cpp
    core/time.h
    core/config.cpp
    core/config.h
)

target_include_directories(tekki-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
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
add_library(tekki-rg INTERFACE)

target_include_directories(tekki-rg
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tekki-rg
    INTERFACE
        tekki-backend
)

# Asset library - loading and processing
add_library(tekki-asset INTERFACE)

target_include_directories(tekki-asset
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tekki-asset
    INTERFACE
        tekki-core
        OpenEXR::OpenEXR
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        tomlplusplus::tomlplusplus
        half::half
        TinyGLTF::TinyGLTF
)

# Renderer library - main rendering implementation
add_library(tekki-renderer INTERFACE)

target_include_directories(tekki-renderer
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tekki-renderer
    INTERFACE
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
