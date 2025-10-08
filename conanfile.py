from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class TekkiRecipe(ConanFile):
    name = "tekki"
    version = "0.1.0"
    package_type = "application"

    # Optional metadata
    license = "MIT OR Apache-2.0"
    author = "tekki Contributors"
    url = "https://github.com/yourusername/tekki"
    description = "C++ port of kajiya - Experimental real-time global illumination renderer"
    topics = ("rendering", "vulkan", "raytracing", "global-illumination")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "shaders/*", "assets/*", "data/*"

    options = {
        "with_dlss": [True, False],
        "with_validation": [True, False],
    }

    default_options = {
        "with_dlss": False,
        "with_validation": True,
    }

    def requirements(self):
        # Vulkan
        self.requires("vulkan-headers/1.3.243.0")
        self.requires("vulkan-loader/1.3.243.0")
        self.requires("vulkan-memory-allocator/3.0.1")

        # Math
        self.requires("glm/1.0.1")

        # Image loading
        self.requires("stb/cci.20230920")
        self.requires("openexr/3.2.4")

        # JSON/Serialization
        self.requires("nlohmann_json/3.11.3")
        self.requires("yaml-cpp/0.8.0")
        self.requires("tomlplusplus/3.4.0")

        # GUI
        self.requires("imgui/1.90.4")

        # Window management
        self.requires("glfw/3.4")

        # SPIRV tools
        self.requires("spirv-cross/1.3.243.0")
        self.requires("spirv-tools/1.3.243.0")

        # Utilities
        self.requires("spdlog/1.14.1")
        self.requires("fmt/10.2.1")
        self.requires("cli11/2.4.1")

        # Half precision
        self.requires("half/2.2.0")

        # glTF loading
        self.requires("tinygltf/2.9.0")

        # File watching
        self.requires("efsw/1.4.0")

    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["TEKKI_WITH_DLSS"] = self.options.with_dlss
        tc.variables["TEKKI_WITH_VALIDATION"] = self.options.with_validation
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
