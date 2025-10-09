#include "tekki/kajiya_simple/input.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace tekki::kajiya_simple {

KeyboardState::KeyboardState() = default;

bool KeyboardState::IsDown(VirtualKeyCode key) const {
    return GetDown(key) != nullptr;
}

bool KeyboardState::WasJustPressed(VirtualKeyCode key) const {
    auto state = GetDown(key);
    return state && state->Ticks == 1;
}

const KeyState* KeyboardState::GetDown(VirtualKeyCode key) const {
    auto it = keys_down_.find(key);
    return it != keys_down_.end() ? &it->second : nullptr;
}

void KeyboardState::Update(const std::vector<void*>& events) {
    for (auto event : events) {
        // Implementation depends on specific event system
        // This is a placeholder for the actual event processing
        // In a real implementation, we would process window events and device events
        // similar to the Rust version
    }

    for (auto& pair : keys_down_) {
        pair.second.Ticks++;
    }
}

MouseState::MouseState() 
    : PhysicalPosition(0.0, 0.0)
    , Delta(0.0f, 0.0f)
    , ButtonsHeld(0)
    , ButtonsPressed(0)
    , ButtonsReleased(0) {
}

void MouseState::Update(const std::vector<void*>& events) {
    ButtonsPressed = 0;
    ButtonsReleased = 0;
    Delta = glm::vec2(0.0f, 0.0f);

    for (auto event : events) {
        // Implementation depends on specific event system
        // This is a placeholder for the actual event processing
        // In a real implementation, we would process:
        // - CursorMoved events to update PhysicalPosition
        // - MouseInput events to update button states
        // - DeviceEvent::MouseMotion events to update Delta
    }
}

KeyMap::KeyMap(InputAxis axis, float multiplier)
    : Axis(axis)
    , Multiplier(multiplier)
    , ActivationTime(0.15f) {
}

KeyMap& KeyMap::ActivationTime(float value) {
    ActivationTime = value;
    return *this;
}

KeyboardMap::KeyboardMap() = default;

KeyboardMap& KeyboardMap::Bind(VirtualKeyCode key, const KeyMap& map) {
    bindings_.emplace_back(key, KeyMapState{map, 0.0f});
    return *this;
}

std::unordered_map<InputAxis, float> KeyboardMap::Map(const KeyboardState& keyboard, float dt) {
    std::unordered_map<InputAxis, float> result;

    for (auto& binding : bindings_) {
        auto& key_state = binding.second;
        
        if (key_state.Map.ActivationTime > 1e-10f) {
            float change = keyboard.IsDown(binding.first) ? dt : -dt;
            key_state.Activation = std::clamp(
                key_state.Activation + change / key_state.Map.ActivationTime, 
                0.0f, 1.0f
            );
        } else {
            key_state.Activation = keyboard.IsDown(binding.first) ? 1.0f : 0.0f;
        }

        result[key_state.Map.Axis] += std::pow(key_state.Activation, 2.0f) * key_state.Map.Multiplier;
    }

    for (auto& pair : result) {
        pair.second = std::clamp(pair.second, -1.0f, 1.0f);
    }

    return result;
}

} // namespace tekki::kajiya_simple