# Core library - fundamental utilities and types
add_library(tekki-core STATIC
    core/log.cpp
    core/log.hpp
    core/error.hpp
    core/common.hpp
    # Will be populated during translation
)

target_include_directories(tekki-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tekki-core
    PUBLIC
        tekki_compile_options
        glm::glm
        fmt::fmt
        spdlog::spdlog
)

# Backend library - Vulkan wrapper and GPU management
add_library(tekki-backend STATIC
    # Will be populated during translation
)

target_include_directories(tekki-backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tekki-backend
    PUBLIC
        tekki-core
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        spirv-cross-core
        SPIRV-Tools
        efsw::efsw
)

# Render graph library
add_library(tekki-rg STATIC
    # Will be populated during translation
)

target_include_directories(tekki-rg
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tekki-rg
    PUBLIC
        tekki-backend
)

# Asset library - loading and processing
add_library(tekki-asset STATIC
    # Will be populated during translation
)

target_include_directories(tekki-asset
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tekki-asset
    PUBLIC
        tekki-core
        OpenEXR::OpenEXR
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        tomlplusplus::tomlplusplus
        half::half
        TinyGLTF::TinyGLTF
)

# Renderer library - main rendering implementation
add_library(tekki-renderer STATIC
    # Will be populated during translation
)

target_include_directories(tekki-renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
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
