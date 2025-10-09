#pragma once

#include <string>

namespace tekki::backend::rspirv_reflect {

enum class DescriptorType {
    UNIFORM_BUFFER,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    SAMPLED_IMAGE,
    STORAGE_IMAGE,
    SAMPLER,
    COMBINED_IMAGE_SAMPLER,
    INPUT_ATTACHMENT,
    ACCELERATION_STRUCTURE_KHR
};

enum class DescriptorDimensionality {
    Single,
    Array
};

struct DescriptorInfo {
    DescriptorType ty;
    DescriptorDimensionality dimensionality;
    std::string name;
};

} // namespace tekki::backend::rspirv_reflect