cmake_minimum_required(VERSION 3.15)
project(rspirv-reflect LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library configuration
add_library(rspirv-reflect
    src/reflection.cpp
)

# Include directories
target_include_directories(rspirv-reflect
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Dependencies
find_package(spirv-cross REQUIRED)
find_package(SPIRV-Headers REQUIRED)

target_link_libraries(rspirv-reflect
    PUBLIC
        spirv-cross-cpp
        SPIRV-Headers::SPIRV-Headers
)

# Compiler options
target_compile_options(rspirv-reflect
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror>
)

# Install targets
install(TARGETS rspirv-reflect
    EXPORT rspirv-reflect-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

# Export package
install(EXPORT rspirv-reflect-targets
    FILE rspirv-reflect-config.cmake
    NAMESPACE rspirv-reflect::
    DESTINATION lib/cmake/rspirv-reflect
)