// All implementations are inline in tekki/rust_shader_builder/main.h
#include "tekki/rust_shader_builder/main.h"

int main() {
    try {
        tekki::rust_shader_builder::RustShaderBuilder::Build();
        return 0;
    } catch (const std::exception&) {
        // Error handling - no need to use the exception variable
        return 1;
    }
}