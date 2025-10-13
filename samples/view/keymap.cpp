#include "tekki/view/keymap.h"
#include <unordered_map>
#include <stdexcept>

namespace tekki::view {

KeyboardMap& KeyboardMap::bind(VirtualKeyCode key, const KeyMap& map) {
    // Implementation would go here
    return *this;
}

KeymapConfig KeymapConfig::Load(const std::optional<std::filesystem::path>& path) {
    auto config_path = path.value_or("keymap.toml");
    
    if (!std::filesystem::exists(config_path)) {
        throw std::runtime_error("Failed to find keymap.toml. Make sure it is in the same directory as the executable.");
    }

    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open keymap.toml");
    }

    std::string buffer;
    std::string line;
    while (std::getline(file, line)) {
        buffer += line + "\n";
    }

    if (file.bad()) {
        throw std::runtime_error("Failed to read keymap.toml");
    }

    try {
        auto data = toml::parse(buffer);
        KeymapConfig config;
        
        // Parse movement section
        if (data.contains("movement")) {
            auto movement_table = data["movement"].as_table();
            if (movement_table->contains("forward")) config.movement.forward = ParseVirtualKeyCode(movement_table->get("forward")->as_string()->get());
            if (movement_table->contains("backward")) config.movement.backward = ParseVirtualKeyCode(movement_table->get("backward")->as_string()->get());
            if (movement_table->contains("left")) config.movement.left = ParseVirtualKeyCode(movement_table->get("left")->as_string()->get());
            if (movement_table->contains("right")) config.movement.right = ParseVirtualKeyCode(movement_table->get("right")->as_string()->get());
            if (movement_table->contains("up")) config.movement.up = ParseVirtualKeyCode(movement_table->get("up")->as_string()->get());
            if (movement_table->contains("down")) config.movement.down = ParseVirtualKeyCode(movement_table->get("down")->as_string()->get());
            if (movement_table->contains("boost")) config.movement.boost = ParseVirtualKeyCode(movement_table->get("boost")->as_string()->get());
            if (movement_table->contains("slow")) config.movement.slow = ParseVirtualKeyCode(movement_table->get("slow")->as_string()->get());
        }
        
        // Parse ui section
        if (data.contains("ui")) {
            auto ui_table = data["ui"].as_table();
            if (ui_table->contains("toggle")) config.ui.toggle = ParseVirtualKeyCode(ui_table->get("toggle")->as_string()->get());
        }
        
        // Parse sequencer section
        if (data.contains("sequencer")) {
            auto sequencer_table = data["sequencer"].as_table();
            if (sequencer_table->contains("add_keyframe")) config.sequencer.add_keyframe = ParseVirtualKeyCode(sequencer_table->get("add_keyframe")->as_string()->get());
            if (sequencer_table->contains("play")) config.sequencer.play = ParseVirtualKeyCode(sequencer_table->get("play")->as_string()->get());
        }
        
        // Parse rendering section
        if (data.contains("rendering")) {
            auto rendering_table = data["rendering"].as_table();
            if (rendering_table->contains("switch_to_reference_path_tracing")) config.rendering.switch_to_reference_path_tracing = ParseVirtualKeyCode(rendering_table->get("switch_to_reference_path_tracing")->as_string()->get());
            if (rendering_table->contains("reset_path_tracer")) config.rendering.reset_path_tracer = ParseVirtualKeyCode(rendering_table->get("reset_path_tracer")->as_string()->get());
            if (rendering_table->contains("light_enable_emissive")) config.rendering.light_enable_emissive = ParseVirtualKeyCode(rendering_table->get("light_enable_emissive")->as_string()->get());
        }
        
        // Parse misc section
        if (data.contains("misc")) {
            auto misc_table = data["misc"].as_table();
            if (misc_table->contains("print_camera_transform")) config.misc.print_camera_transform = ParseVirtualKeyCode(misc_table->get("print_camera_transform")->as_string()->get());
        }

        return config;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse keymap.toml: ") + e.what());
    }
}

KeyboardMap KeymapConfig::ToKeyboardMap() const {
    return KeyboardMap()
        .bind(movement.forward, KeyMap("move_fwd", 1.0f))
        .bind(movement.backward, KeyMap("move_fwd", -1.0f))
        .bind(movement.right, KeyMap("move_right", 1.0f))
        .bind(movement.left, KeyMap("move_right", -1.0f))
        .bind(movement.up, KeyMap("move_up", 1.0f))
        .bind(movement.down, KeyMap("move_up", -1.0f))
        .bind(movement.boost, KeyMap("boost", 1.0f).activation_time(0.25f))
        .bind(movement.slow, KeyMap("boost", -1.0f).activation_time(0.5f));
}

VirtualKeyCode KeymapConfig::ParseVirtualKeyCode(const std::string& key) {
    static const std::unordered_map<std::string, VirtualKeyCode> key_map = {
        {"W", VirtualKeyCode::W},
        {"S", VirtualKeyCode::S},
        {"A", VirtualKeyCode::A},
        {"D", VirtualKeyCode::D},
        {"E", VirtualKeyCode::E},
        {"Q", VirtualKeyCode::Q},
        {"LShift", VirtualKeyCode::LShift},
        {"LControl", VirtualKeyCode::LControl},
        {"Tab", VirtualKeyCode::Tab},
        {"K", VirtualKeyCode::K},
        {"P", VirtualKeyCode::P},
        {"Space", VirtualKeyCode::Space},
        {"Back", VirtualKeyCode::Back},
        {"L", VirtualKeyCode::L},
        {"C", VirtualKeyCode::C}
    };

    auto it = key_map.find(key);
    if (it != key_map.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown virtual key code: " + key);
}

}