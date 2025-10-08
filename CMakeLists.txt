cmake_minimum_required(VERSION 3.20)
project(tekki VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create a common compile definitions interface
add_library(tekki_compile_options INTERFACE)

# Enable filesystem library
if(MSVC)
    target_compile_options(tekki_compile_options INTERFACE /std:c++latest)
else()
    target_compile_options(tekki_compile_options INTERFACE -std=c++20)
endif()

# Options
option(TEKKI_WITH_DLSS "Enable DLSS support" OFF)
option(TEKKI_WITH_VALIDATION "Enable Vulkan validation layers" ON)
option(TEKKI_BUILD_TESTS "Build tests" OFF)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(CLI11 REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(spirv-cross REQUIRED)
find_package(SPIRV-Tools REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)
find_package(half REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(efsw REQUIRED)

# Global compile options
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(tekki_compile_options INTERFACE
    $<$<CONFIG:Debug>:TEKKI_DEBUG>
    $<$<CONFIG:Release>:TEKKI_RELEASE>
    $<$<BOOL:${TEKKI_WITH_VALIDATION}>:TEKKI_ENABLE_VALIDATION>
    $<$<BOOL:${TEKKI_WITH_DLSS}>:TEKKI_WITH_DLSS>
)

# Subdirectories
add_subdirectory(src)

# Installation
install(TARGETS tekki-view
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Copy assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)

# Copy shaders
install(DIRECTORY ${CMAKE_SOURCE_DIR}/shaders
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)
