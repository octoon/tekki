#include "rspirv-reflect/reflection.h"
#include <iostream>
#include <vector>

using namespace rspirv_reflect;

void TestBasicReflection() {
    std::cout << "Testing rspirv-reflect C++ translation..." << std::endl;

    // Test DescriptorType
    DescriptorType sampler_type = DescriptorType::SAMPLER;
    DescriptorType buffer_type = DescriptorType::UNIFORM_BUFFER;

    std::cout << "Sampler type: " << sampler_type.ToString() << std::endl;
    std::cout << "Buffer type: " << buffer_type.ToString() << std::endl;

    // Test BindingCount
    auto one_binding = BindingCount::One();
    auto static_binding = BindingCount::StaticSized(4);
    auto unbounded_binding = BindingCount::Unbounded();

    std::cout << "Binding count types created successfully" << std::endl;

    // Test DescriptorInfo
    DescriptorInfo info{
        DescriptorType::STORAGE_BUFFER,
        BindingCount::One(),
        "test_buffer"
    };

    std::cout << "DescriptorInfo created: " << info.name << std::endl;

    std::cout << "Basic type tests passed!" << std::endl;
}

int main() {
    try {
        TestBasicReflection();
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}