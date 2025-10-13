# Rust shader builder tool - compiles and processes shaders
add_executable(tekki-shader-builder
    shader_builder/main.cpp
)

target_include_directories(tekki-shader-builder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-shader-builder
    PRIVATE
        tekki-backend
)

# Asset baker tool - processes GLTF to .mesh/.image format
add_executable(tekki-asset-baker
    asset_baker/main.cpp
)

target_include_directories(tekki-asset-baker
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(tekki-asset-baker
    PRIVATE
        tekki-asset-pipe
        tekki-asset
        tekki-backend
        CLI11::CLI11
)

# Set output directory for tools
set_target_properties(tekki-shader-builder tekki-asset-baker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tools
)
