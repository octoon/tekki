// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/bytes.rs

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace tekki::backend
{

/**
 * @brief Convert a vector of type T to a vector of bytes
 * @tparam T Must be a trivially copyable type
 * @param vec Vector to convert
 * @return Vector of bytes containing the raw data
 */
template <typename T> std::vector<uint8_t> IntoByteVec(std::vector<T>&& vec)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    const size_t item_sizeof = sizeof(T);
    const size_t len = vec.size() * item_sizeof;
    const size_t cap = vec.capacity() * item_sizeof;

    std::vector<uint8_t> result;
    result.resize(len);

    // Copy the data
    std::memcpy(result.data(), vec.data(), len);

    return result;
}

/**
 * @brief Get a byte slice view of any trivially copyable object
 * @tparam T Must be a trivially copyable type
 * @param t Object to view as bytes
 * @return Pointer to the byte representation
 */
template <typename T> const uint8_t* AsByteSlice(const T& t)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    return reinterpret_cast<const uint8_t*>(&t);
}

/**
 * @brief Get the size in bytes of a trivially copyable object
 * @tparam T Must be a trivially copyable type
 * @return Size in bytes
 */
template <typename T> constexpr size_t ByteSize()
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    return sizeof(T);
}

} // namespace tekki::backend
