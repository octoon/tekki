# GPU Allocator library
# A Rust gpu-allocator port to C++ for Vulkan memory allocation

add_library(tekki-gpu-allocator STATIC
    src/result.cpp
    src/allocator.cpp
    src/allocator/dedicated_block_allocator.cpp
    src/allocator/free_list_allocator.cpp
    src/vulkan/allocator.cpp
)

target_include_directories(tekki-gpu-allocator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tekki-gpu-allocator
    PUBLIC
        tekki_compile_options
        Vulkan::Vulkan
        spdlog::spdlog
)

# Set library properties
set_target_properties(tekki-gpu-allocator PROPERTIES
    FOLDER "third_party"
)
