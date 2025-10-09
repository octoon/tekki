#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace tekki::kajiya_simple {

enum class VirtualKeyCode {
    // Add actual key codes as needed
};

enum class ElementState {
    Pressed,
    Released
};

struct KeyState {
    std::uint32_t Ticks;
};

class KeyboardState {
private:
    std::unordered_map<VirtualKeyCode, KeyState> keys_down_;

public:
    KeyboardState() = default;
    
    bool IsDown(VirtualKeyCode key) const {
        return GetDown(key) != nullptr;
    }

    bool WasJustPressed(VirtualKeyCode key) const {
        auto state = GetDown(key);
        return state && state->Ticks == 1;
    }

    const KeyState* GetDown(VirtualKeyCode key) const {
        auto it = keys_down_.find(key);
        return it != keys_down_.end() ? &it->second : nullptr;
    }

    void Update(const std::vector<void*>& events) {
        // Implementation depends on specific event system
        // This is a placeholder for the actual event processing
        
        for (auto& pair : keys_down_) {
            pair.second.Ticks++;
        }
    }
};

struct MouseState {
    glm::dvec2 PhysicalPosition;
    glm::vec2 Delta;
    std::uint32_t ButtonsHeld;
    std::uint32_t ButtonsPressed;
    std::uint32_t ButtonsReleased;

    MouseState() 
        : PhysicalPosition(0.0, 0.0)
        , Delta(0.0f, 0.0f)
        , ButtonsHeld(0)
        , ButtonsPressed(0)
        , ButtonsReleased(0) {
    }

    void Update(const std::vector<void*>& events) {
        ButtonsPressed = 0;
        ButtonsReleased = 0;
        Delta = glm::vec2(0.0f, 0.0f);

        // Implementation depends on specific event system
        // This is a placeholder for the actual event processing
    }
};

using InputAxis = const char*;

struct KeyMap {
    InputAxis Axis;
    float Multiplier;
    float ActivationTime;

    KeyMap(InputAxis axis, float multiplier)
        : Axis(axis)
        , Multiplier(multiplier)
        , ActivationTime(0.15f) {
    }

    KeyMap& ActivationTime(float value) {
        ActivationTime = value;
        return *this;
    }
};

struct KeyMapState {
    KeyMap Map;
    float Activation;
};

class KeyboardMap {
private:
    std::vector<std::pair<VirtualKeyCode, KeyMapState>> bindings_;

public:
    KeyboardMap() = default;
    
    KeyboardMap& Bind(VirtualKeyCode key, const KeyMap& map) {
        bindings_.emplace_back(key, KeyMapState{map, 0.0f});
        return *this;
    }

    std::unordered_map<InputAxis, float> Map(const KeyboardState& keyboard, float dt) {
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
};

} // namespace tekki::kajiya_simple