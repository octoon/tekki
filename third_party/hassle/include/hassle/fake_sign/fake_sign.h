#pragma once

#include <vector>
#include <cstdint>

namespace hassle {
namespace fake_sign {

/// Helper function for signing DXIL binary blobs when
/// `dxil.dll` might not be available (such as on Linux based
/// platforms).
/// This essentially performs the same functionality as `validate_dxil()`
/// but in a more cross platform way.
///
/// Ported from https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/driver/shaders/dxbc/dxbc_container.cpp#L832
///
/// @param dxil The DXIL bytecode to sign (modified in-place)
/// @return true if signing was successful, false otherwise
bool FakeSignDxilInPlace(std::vector<uint8_t>& dxil);

} // namespace fake_sign
} // namespace hassle
