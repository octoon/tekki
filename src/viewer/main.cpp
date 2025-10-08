// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "core/common.hpp"
#include "core/config.h"
#include "core/log.h"
#include "core/time.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

VkInstance create_vulkan_instance(bool enable_validation,
                                  const std::vector<const char*>& extensions) {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "tekki-view";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "tekki";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    if (enable_validation) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    }

    VkInstance instance = VK_NULL_HANDLE;
    const auto result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    return instance;
}

} // namespace

int main(int argc, char** argv) {
    TEKKI_UNUSED(argc);
    TEKKI_UNUSED(argv);

    const auto config = tekki::config::load_from_file("data/config/viewer.json");

    tekki::log::init(config.log_level);
    TEKKI_LOG_INFO("tekki viewer starting - window {}x{}", config.window_width, config.window_height);

    if (!glfwInit()) {
        TEKKI_LOG_CRITICAL("Failed to initialize GLFW");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        config.window_width,
        config.window_height,
        config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!window) {
        TEKKI_LOG_CRITICAL("Failed to create GLFW window");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    if (!glfwVulkanSupported()) {
        TEKKI_LOG_CRITICAL("GLFW reports Vulkan unsupported on this platform");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    uint32_t extension_count = 0;
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    if (!required_extensions || extension_count == 0) {
        TEKKI_LOG_CRITICAL("Failed to query required Vulkan extensions from GLFW");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::vector<const char*> extensions(required_extensions, required_extensions + extension_count);

    VkInstance instance = VK_NULL_HANDLE;
    try {
        instance = create_vulkan_instance(config.enable_validation, extensions);
        TEKKI_LOG_INFO("Vulkan instance created with {} extensions", extension_count);
    } catch (const std::exception& err) {
        TEKKI_LOG_CRITICAL("{}", err.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    tekki::time::FrameTimer frame_timer;
    bool running = true;
    unsigned frame_count = 0;

    while (running) {
        glfwPollEvents();

        if (glfwWindowShouldClose(window)) {
            running = false;
        }

        const double dt = frame_timer.tick();
        if (frame_count % 60 == 0) {
            TEKKI_LOG_INFO("Frame {} | dt = {:.2f} ms", frame_count, dt * 1000.0);
        }

        if (config.bootstrap_frames > 0 && frame_count >= config.bootstrap_frames) {
            TEKKI_LOG_INFO("Bootstrap frame limit {} reached, exiting", config.bootstrap_frames);
            running = false;
        }

        ++frame_count;
    }

    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    TEKKI_LOG_INFO("tekki viewer shutdown complete");
    return EXIT_SUCCESS;
}
