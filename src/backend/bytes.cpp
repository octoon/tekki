#include "tekki/backend/bytes.h"

namespace tekki::backend {

// Explicit template instantiations for common types
// Template definitions are in the header file

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
