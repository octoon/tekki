#include "tekki/backend/dynamic_constants.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/device.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cstring>

namespace tekki::backend {

// Explicit template instantiations for common types
template uint32_t DynamicConstants::Push<int>(const int&);
template uint32_t DynamicConstants::Push<float>(const float&);
template uint32_t DynamicConstants::Push<double>(const double&);
template uint32_t DynamicConstants::Push<glm::vec2>(const glm::vec2&);
template uint32_t DynamicConstants::Push<glm::vec3>(const glm::vec3&);
template uint32_t DynamicConstants::Push<glm::vec4>(const glm::vec4&);
template uint32_t DynamicConstants::Push<glm::mat3>(const glm::mat3&);
template uint32_t DynamicConstants::Push<glm::mat4>(const glm::mat4&);

template uint32_t DynamicConstants::PushFromIter<int, std::vector<int>::iterator>(std::vector<int>::iterator, std::vector<int>::iterator);
template uint32_t DynamicConstants::PushFromIter<float, std::vector<float>::iterator>(std::vector<float>::iterator, std::vector<float>::iterator);
template uint32_t DynamicConstants::PushFromIter<double, std::vector<double>::iterator>(std::vector<double>::iterator, std::vector<double>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec2, std::vector<glm::vec2>::iterator>(std::vector<glm::vec2>::iterator, std::vector<glm::vec2>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec3, std::vector<glm::vec3>::iterator>(std::vector<glm::vec3>::iterator, std::vector<glm::vec3>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec4, std::vector<glm::vec4>::iterator>(std::vector<glm::vec4>::iterator, std::vector<glm::vec4>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::mat3, std::vector<glm::mat3>::iterator>(std::vector<glm::mat3>::iterator, std::vector<glm::mat3>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::mat4, std::vector<glm::mat4>::iterator>(std::vector<glm::mat4>::iterator, std::vector<glm::mat4>::iterator);

} // namespace tekki::backend