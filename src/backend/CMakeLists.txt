# Backend module CMake configuration

add_library(tekki-backend
    # Backend core files
    file.cpp
    shader_compiler.cpp
    spirv_reflect_helper.cpp

    # Vulkan backend files
    vulkan/allocator.cpp
    vulkan/barrier.cpp
    vulkan/buffer.cpp
    vulkan/buffer_builder.cpp
    vulkan/device.cpp
    vulkan/dynamic_constants.cpp
    vulkan/image.cpp
    vulkan/instance.cpp
    vulkan/physical_device.cpp
    vulkan/pipeline_cache.cpp
    vulkan/profiler.cpp
    vulkan/ray_tracing.cpp
    vulkan/shader.cpp
    vulkan/surface.cpp
    vulkan/swapchain.cpp
    vulkan/transient_resource_cache.cpp
)

target_include_directories(tekki-backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tekki-backend
    PUBLIC
        tekki-core
        Vulkan::Vulkan
        spdlog::spdlog
)

# Add definitions for Windows
if(WIN32)
    target_compile_definitions(tekki-backend
        PRIVATE
            VK_USE_PLATFORM_WIN32_KHR
    )
endif()