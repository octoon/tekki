# Tekki Unit Tests
cmake_minimum_required(VERSION 3.20)

# Test executable
add_executable(tekki-tests
    # Main test runner
    test_main.cpp

    # Core tests
    core/test_result.cpp

    # Backend tests
    backend/test_bytes.cpp
    backend/test_chunky_list.cpp
    backend/test_dynamic_constants.cpp
    backend/test_file.cpp
    backend/test_transient_resource_cache.cpp

    # Render graph tests
    render_graph/test_graph.cpp
    render_graph/test_temporal.cpp

    # Asset tests
    asset/test_image.cpp
    asset/test_mesh.cpp

    # Renderer tests
    renderer/test_camera.cpp
    renderer/test_math.cpp
    renderer/test_buffer_builder.cpp
)

target_link_libraries(tekki-tests
    PRIVATE
        Catch2::Catch2
        tekki-backend
        tekki-rg
        tekki-asset
        tekki-renderer
        tekki-rust-shaders-shared
)

target_include_directories(tekki-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Discover and register tests with CTest
include(Catch)
catch_discover_tests(tekki-tests)

# Copy test data if needed
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
    add_custom_command(TARGET tekki-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:tekki-tests>/test_data
    )
endif()
