#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>

namespace tekki::backend {

/**
 * Convert a vector of type T to a vector of bytes
 * @tparam T The type of elements in the input vector (must be copyable)
 * @param v The input vector to convert
 * @return A vector containing the byte representation of the input vector
 */
template<typename T>
std::vector<uint8_t> IntoByteVec(std::vector<T> v)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    
    if (v.empty()) {
        return std::vector<uint8_t>();
    }
    
    auto* p = v.data();
    const size_t item_sizeof = sizeof(T);
    const size_t len = v.size() * item_sizeof;
    const size_t cap = v.capacity() * item_sizeof;
    
    std::vector<uint8_t> result;
    try {
        result = std::vector<uint8_t>(reinterpret_cast<uint8_t*>(p), reinterpret_cast<uint8_t*>(p) + len);
        result.reserve(cap);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to convert vector to byte vector: " + std::string(e.what()));
    }
    
    return result;
}

/**
 * Get a byte slice view of an object
 * @tparam T The type of the object (must be copyable)
 * @param t Reference to the object
 * @return A pointer to the byte representation of the object and its size
 */
template<typename T>
std::pair<const uint8_t*, size_t> AsByteSlice(const T& t)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    
    try {
        const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(&t);
        const size_t size = sizeof(T);
        return std::make_pair(byte_ptr, size);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to get byte slice of object: " + std::string(e.what()));
    }
}

} // namespace tekki::backend