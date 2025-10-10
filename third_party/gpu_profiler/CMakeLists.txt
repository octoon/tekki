# GPU Profiler library
# A Rust gpu-profiler port to C++ for GPU performance profiling

add_library(tekki-gpu-profiler STATIC
    src/gpu_profiler.cpp
    src/backend/vulkan.cpp
)

target_include_directories(tekki-gpu-profiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tekki-gpu-profiler
    PUBLIC
        tekki_compile_options
        Vulkan::Vulkan
)

# Set library properties
set_target_properties(tekki-gpu-profiler PROPERTIES
    FOLDER "third_party"
)
