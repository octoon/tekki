cmake_minimum_required(VERSION 3.15)
project(hassle LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library options
option(HASSLE_BUILD_SHARED "Build shared library" OFF)
option(HASSLE_BUILD_STATIC "Build static library" ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source files
set(HASSLE_SOURCES
    src/utils.cpp
    src/fake_sign/modified_md5.cpp
    src/fake_sign/fake_sign.cpp
)

# Header files
set(HASSLE_HEADERS
    include/hassle/os.h
    include/hassle/ffi.h
    include/hassle/utils.h
    include/hassle/wrapper.h
    include/hassle/fake_sign/fake_sign.h
    include/hassle/fake_sign/modified_md5.h
    include/hassle/intellisense/ffi.h
    include/hassle/intellisense/wrapper.h
)

# Platform-specific settings
if(WIN32)
    # Windows specific settings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)

    # Disable specific warnings for hassle library
    # C4099: type name first seen using 'struct' now seen using 'class'
    # C4100: unreferenced parameter
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4099 /wd4100")
    endif()

    # Link Windows libraries
    set(PLATFORM_LIBS
        ole32
        oleaut32
    )
else()
    # Linux/Unix specific settings
    find_library(DL_LIBRARY dl)
    set(PLATFORM_LIBS
        ${DL_LIBRARY}
    )
endif()

# Create the library
if(HASSLE_BUILD_SHARED)
    add_library(hassle SHARED ${HASSLE_SOURCES})
    target_compile_definitions(hassle PRIVATE HASSLE_SHARED)
elseif(HASSLE_BUILD_STATIC)
    add_library(hassle STATIC ${HASSLE_SOURCES})
endif()

# Set library properties
target_include_directories(hassle PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link platform libraries
target_link_libraries(hassle PRIVATE ${PLATFORM_LIBS})

# Set target properties
set_target_properties(hassle PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Install targets
install(TARGETS hassle
    EXPORT hassle-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/hassle
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT hassle-targets
    FILE hassle-config.cmake
    NAMESPACE hassle::
    DESTINATION lib/cmake/hassle
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/hassle-config-version.cmake
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/hassle-config-version.cmake
    DESTINATION lib/cmake/hassle
)

# Tests (if enabled)
option(HASSLE_BUILD_TESTS "Build tests" OFF)
if(HASSLE_BUILD_TESTS)
    enable_testing()

    # Add test executable
    add_executable(hassle_test
        test/test_main.cpp
        # Add test files here
    )

    target_link_libraries(hassle_test PRIVATE hassle)

    # Add tests
    add_test(NAME hassle_basic_test COMMAND hassle_test)
endif()

# Documentation (if enabled)
option(HASSLE_BUILD_DOCS "Build documentation" OFF)
if(HASSLE_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)

        doxygen_add_docs(docs
            ${DOXYGEN_INPUT}
            COMMENT "Generating API documentation with Doxygen"
        )
    endif()
endif()