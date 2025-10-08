// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <cstdint>

// Platform detection
#if defined(_WIN32)
    #define TEKKI_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define TEKKI_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define TEKKI_PLATFORM_MACOS
#endif

// Compiler detection
#if defined(_MSC_VER)
    #define TEKKI_COMPILER_MSVC
#elif defined(__clang__)
    #define TEKKI_COMPILER_CLANG
#elif defined(__GNUC__)
    #define TEKKI_COMPILER_GCC
#endif

// Configuration
#if defined(TEKKI_DEBUG)
    #define TEKKI_ENABLE_ASSERTS
#endif

// Common types
namespace tekki
{

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

using usize = size_t;

} // namespace tekki

// Assertion macros
#ifdef TEKKI_ENABLE_ASSERTS
    #include <cassert>
    #define TEKKI_ASSERT(expr) assert(expr)
    #define TEKKI_ASSERT_MSG(expr, msg) assert((expr) && (msg))
#else
    #define TEKKI_ASSERT(expr) ((void)0)
    #define TEKKI_ASSERT_MSG(expr, msg) ((void)0)
#endif

// Utility macros
#define TEKKI_UNUSED(x) ((void)(x))
#define TEKKI_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
