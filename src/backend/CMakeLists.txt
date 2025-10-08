# Backend module CMake configuration

add_library(tekki-backend
    vulkan/instance.cpp
    vulkan/device.cpp
    vulkan/buffer.cpp
    vulkan/image.cpp
    vulkan/ray_tracing.cpp
    vulkan/shader.cpp
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