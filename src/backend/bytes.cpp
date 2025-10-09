#include "tekki/backend/bytes.h"
#include <stdexcept>
#include <string>
#include <type_traits>

namespace tekki::backend {

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

// Explicit template instantiations for common types
template std::vector<uint8_t> IntoByteVec<int8_t>(std::vector<int8_t> v);
template std::vector<uint8_t> IntoByteVec<uint8_t>(std::vector<uint8_t> v);
template std::vector<uint8_t> IntoByteVec<int16_t>(std::vector<int16_t> v);
template std::vector<uint8_t> IntoByteVec<uint16_t>(std::vector<uint16_t> v);
template std::vector<uint8_t> IntoByteVec<int32_t>(std::vector<int32_t> v);
template std::vector<uint8_t> IntoByteVec<uint32_t>(std::vector<uint32_t> v);
template std::vector<uint8_t> IntoByteVec<int64_t>(std::vector<int64_t> v);
template std::vector<uint8_t> IntoByteVec<uint64_t>(std::vector<uint64_t> v);
template std::vector<uint8_t> IntoByteVec<float>(std::vector<float> v);
template std::vector<uint8_t> IntoByteVec<double>(std::vector<double> v);
template std::vector<uint8_t> IntoByteVec<glm::vec2>(std::vector<glm::vec2> v);
template std::vector<uint8_t> IntoByteVec<glm::vec3>(std::vector<glm::vec3> v);
template std::vector<uint8_t> IntoByteVec<glm::vec4>(std::vector<glm::vec4> v);
template std::vector<uint8_t> IntoByteVec<glm::mat2>(std::vector<glm::mat2> v);
template std::vector<uint8_t> IntoByteVec<glm::mat3>(std::vector<glm::mat3> v);
template std::vector<uint8_t> IntoByteVec<glm::mat4>(std::vector<glm::mat4> v);

template std::pair<const uint8_t*, size_t> AsByteSlice<int8_t>(const int8_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<uint8_t>(const uint8_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<int16_t>(const int16_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<uint16_t>(const uint16_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<int32_t>(const int32_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<uint32_t>(const uint32_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<int64_t>(const int64_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<uint64_t>(const uint64_t& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<float>(const float& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<double>(const double& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::vec2>(const glm::vec2& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::vec3>(const glm::vec3& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::vec4>(const glm::vec4& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::mat2>(const glm::mat2& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::mat3>(const glm::mat3& t);
template std::pair<const uint8_t*, size_t> AsByteSlice<glm::mat4>(const glm::mat4& t);

} // namespace tekki::backend