#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <string>
#include "tekki/backend/file.h"
#include "tekki/renderer/camera.h"
#include "tekki/renderer/frame_desc.h"
#include "tekki/renderer/math.h"
#include "tekki/renderer/world_renderer.h"
#include "tekki/input/input.h"
#include "tekki/main_loop/main_loop.h"

namespace tekki::kajiya_simple {

using namespace glm;
using tekki::input::*;
using tekki::backend::file::SetStandardVfsMountPoints;
using tekki::backend::file::SetVfsMountPoint;
using tekki::backend::*;
using tekki::renderer::*;
using tekki::renderer::WorldFrameDesc;
using tekki::renderer::RenderDebugMode;
using tekki::renderer::RenderMode;
using tekki::main_loop::*;
using winit::WindowBuilder;
using winit::event::ElementState;
using winit::event::KeyboardInput;
using winit::event::MouseButton;
using winit::event::WindowEvent;

} // namespace tekki::kajiya_simple