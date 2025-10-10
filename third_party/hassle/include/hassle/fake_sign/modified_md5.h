#pragma once

// Modified MD5 implementation for DXIL fake signing
// Based on the original Rust implementation from stainless-steel/md5
// Licensed under Apache 2.0 and MIT dual license

#include <cstdint>
#include <cstring>
#include <vector>

namespace hassle {
namespace fake_sign {

class ModifiedMd5Context {
private:
    uint8_t buffer[64] = {0};
    uint32_t count[2] = {0, 0};
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};

    static void Transform(uint32_t state[4], const uint32_t input[16]);
    static void ConsumeChunk(ModifiedMd5Context* ctx, const uint8_t* data, size_t length);

public:
    ModifiedMd5Context() = default;

    void Consume(const uint8_t* data, size_t length);
    void Consume(const std::vector<uint8_t>& data);

    const uint32_t* GetState() const { return state; }
};

} // namespace fake_sign
} // namespace hassle
