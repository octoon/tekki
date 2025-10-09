# Test configuration for tekki project
# Unit tests using Catch2 framework

if(NOT TEKKI_BUILD_TESTS)
    return()
endif()

# Test executables
# We create separate test executables for each module to allow parallel testing
# and better organization

# Core module tests
add_executable(tekki-test-core
    test_main.cpp
    core/test_log.cpp
    core/test_mmap.cpp
    core/test_math.cpp
)

target_link_libraries(tekki-test-core
    PRIVATE
        tekki-core
        Catch2::Catch2
)

# Render graph tests
add_executable(tekki-test-render-graph
    test_main.cpp
    render_graph/test_resource.cpp
    render_graph/test_temporal.cpp
)

target_link_libraries(tekki-test-render-graph
    PRIVATE
        tekki-rg
        tekki-core
        Catch2::Catch2
)

# Asset module tests
add_executable(tekki-test-asset
    test_main.cpp
    asset/test_mesh.cpp
    asset/test_image.cpp
)

target_link_libraries(tekki-test-asset
    PRIVATE
        tekki-asset
        tekki-core
        Catch2::Catch2
)

# Renderer module tests
add_executable(tekki-test-renderer
    test_main.cpp
    renderer/test_world_renderer.cpp
    renderer/test_half_res.cpp
)

target_link_libraries(tekki-test-renderer
    PRIVATE
        tekki-renderer
        tekki-rg
        tekki-core
        Catch2::Catch2
)

# Register tests with CTest
include(CTest)
include(Catch)

catch_discover_tests(tekki-test-core)
catch_discover_tests(tekki-test-render-graph)
catch_discover_tests(tekki-test-asset)
catch_discover_tests(tekki-test-renderer)

# Add a custom target to run all tests
add_custom_target(run-all-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel 4
    DEPENDS tekki-test-core tekki-test-render-graph tekki-test-asset tekki-test-renderer
    COMMENT "Running all tekki unit tests"
)

# Installation (optional - install test executables for debugging)
if(TEKKI_INSTALL_TESTS)
    install(TARGETS
        tekki-test-core
        tekki-test-render-graph
        tekki-test-asset
        tekki-test-renderer
        RUNTIME DESTINATION bin/tests
    )
endif()
