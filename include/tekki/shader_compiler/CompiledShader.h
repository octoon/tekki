#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace tekki::backend {

struct CompiledShader {
    std::string name;
    std::vector<uint8_t> spirv;
};

} // namespace tekki::backend
