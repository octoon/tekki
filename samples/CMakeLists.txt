# View library - shared viewer code
add_library(tekki-view-lib STATIC
    view/keymap.cpp
    view/misc.cpp
    view/opt.cpp
)

target_include_directories(tekki-view-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-view-lib
    PUBLIC
        tekki-backend
        tekki-simple
        CLI11::CLI11
)

# Viewer sample application
add_executable(tekki-view
    view/main.cpp
)

target_link_libraries(tekki-view
    PRIVATE
        tekki-view-lib
        tekki-renderer
        tekki-simple
        tekki-imgui
        imgui::imgui
        glfw
        CLI11::CLI11
)

# Hello sample application - minimal demonstration
add_executable(tekki-hello
    hello/main.cpp
)

target_include_directories(tekki-hello
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-hello
    PRIVATE
        tekki-renderer
        tekki-simple
        tekki-asset
        tekki-backend
        glm::glm
)

# Set output directory for samples
set_target_properties(tekki-view tekki-hello PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/samples
)
