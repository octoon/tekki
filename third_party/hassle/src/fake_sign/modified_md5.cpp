#include "hassle/fake_sign/modified_md5.h"
#include <cassert>

namespace hassle {
namespace fake_sign {

void ModifiedMd5Context::Transform(uint32_t state[4], const uint32_t input[16]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];

    // Round 1
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define T(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (ac); \
    (a) = ((a) << (s)) | ((a) >> (32 - (s))); \
    (a) += (b); \
}

    constexpr uint32_t S1 = 7;
    constexpr uint32_t S2 = 12;
    constexpr uint32_t S3 = 17;
    constexpr uint32_t S4 = 22;

    T(a, b, c, d, input[0], S1, 3614090360);
    T(d, a, b, c, input[1], S2, 3905402710);
    T(c, d, a, b, input[2], S3, 606105819);
    T(b, c, d, a, input[3], S4, 3250441966);
    T(a, b, c, d, input[4], S1, 4118548399);
    T(d, a, b, c, input[5], S2, 1200080426);
    T(c, d, a, b, input[6], S3, 2821735955);
    T(b, c, d, a, input[7], S4, 4249261313);
    T(a, b, c, d, input[8], S1, 1770035416);
    T(d, a, b, c, input[9], S2, 2336552879);
    T(c, d, a, b, input[10], S3, 4294925233);
    T(b, c, d, a, input[11], S4, 2304563134);
    T(a, b, c, d, input[12], S1, 1804603682);
    T(d, a, b, c, input[13], S2, 4254626195);
    T(c, d, a, b, input[14], S3, 2792965006);
    T(b, c, d, a, input[15], S4, 1236535329);

#undef F
#undef T

    // Round 2
#define F(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define T(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (ac); \
    (a) = ((a) << (s)) | ((a) >> (32 - (s))); \
    (a) += (b); \
}

    constexpr uint32_t S5 = 5;
    constexpr uint32_t S6 = 9;
    constexpr uint32_t S7 = 14;
    constexpr uint32_t S8 = 20;

    T(a, b, c, d, input[1], S5, 4129170786);
    T(d, a, b, c, input[6], S6, 3225465664);
    T(c, d, a, b, input[11], S7, 643717713);
    T(b, c, d, a, input[0], S8, 3921069994);
    T(a, b, c, d, input[5], S5, 3593408605);
    T(d, a, b, c, input[10], S6, 38016083);
    T(c, d, a, b, input[15], S7, 3634488961);
    T(b, c, d, a, input[4], S8, 3889429448);
    T(a, b, c, d, input[9], S5, 568446438);
    T(d, a, b, c, input[14], S6, 3275163606);
    T(c, d, a, b, input[3], S7, 4107603335);
    T(b, c, d, a, input[8], S8, 1163531501);
    T(a, b, c, d, input[13], S5, 2850285829);
    T(d, a, b, c, input[2], S6, 4243563512);
    T(c, d, a, b, input[7], S7, 1735328473);
    T(b, c, d, a, input[12], S8, 2368359562);

#undef F
#undef T

    // Round 3
#define F(x, y, z) ((x) ^ (y) ^ (z))
#define T(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (ac); \
    (a) = ((a) << (s)) | ((a) >> (32 - (s))); \
    (a) += (b); \
}

    constexpr uint32_t S9 = 4;
    constexpr uint32_t S10 = 11;
    constexpr uint32_t S11 = 16;
    constexpr uint32_t S12 = 23;

    T(a, b, c, d, input[5], S9, 4294588738);
    T(d, a, b, c, input[8], S10, 2272392833);
    T(c, d, a, b, input[11], S11, 1839030562);
    T(b, c, d, a, input[14], S12, 4259657740);
    T(a, b, c, d, input[1], S9, 2763975236);
    T(d, a, b, c, input[4], S10, 1272893353);
    T(c, d, a, b, input[7], S11, 4139469664);
    T(b, c, d, a, input[10], S12, 3200236656);
    T(a, b, c, d, input[13], S9, 681279174);
    T(d, a, b, c, input[0], S10, 3936430074);
    T(c, d, a, b, input[3], S11, 3572445317);
    T(b, c, d, a, input[6], S12, 76029189);
    T(a, b, c, d, input[9], S9, 3654602809);
    T(d, a, b, c, input[12], S10, 3873151461);
    T(c, d, a, b, input[15], S11, 530742520);
    T(b, c, d, a, input[2], S12, 3299628645);

#undef F
#undef T

    // Round 4
#define F(x, y, z) ((y) ^ ((x) | ~(z)))
#define T(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (ac); \
    (a) = ((a) << (s)) | ((a) >> (32 - (s))); \
    (a) += (b); \
}

    constexpr uint32_t S13 = 6;
    constexpr uint32_t S14 = 10;
    constexpr uint32_t S15 = 15;
    constexpr uint32_t S16 = 21;

    T(a, b, c, d, input[0], S13, 4096336452);
    T(d, a, b, c, input[7], S14, 1126891415);
    T(c, d, a, b, input[14], S15, 2878612391);
    T(b, c, d, a, input[5], S16, 4237533241);
    T(a, b, c, d, input[12], S13, 1700485571);
    T(d, a, b, c, input[3], S14, 2399980690);
    T(c, d, a, b, input[10], S15, 4293915773);
    T(b, c, d, a, input[1], S16, 2240044497);
    T(a, b, c, d, input[8], S13, 1873313359);
    T(d, a, b, c, input[15], S14, 4264355552);
    T(c, d, a, b, input[6], S15, 2734768916);
    T(b, c, d, a, input[13], S16, 1309151649);
    T(a, b, c, d, input[4], S13, 4149444226);
    T(d, a, b, c, input[11], S14, 3174756917);
    T(c, d, a, b, input[2], S15, 718787259);
    T(b, c, d, a, input[9], S16, 3951481745);

#undef F
#undef T

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void ModifiedMd5Context::ConsumeChunk(ModifiedMd5Context* ctx, const uint8_t* data, size_t length) {
    uint32_t input[16] = {0};
    size_t k = ((ctx->count[0] >> 3) & 0x3f);
    uint32_t len = static_cast<uint32_t>(length);

    ctx->count[0] += len << 3;
    if (ctx->count[0] < (len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += len >> 29;

    for (size_t i = 0; i < length; ++i) {
        ctx->buffer[k++] = data[i];
        if (k == 64) {
            size_t j = 0;
            for (size_t v = 0; v < 16; ++v) {
                input[v] = (static_cast<uint32_t>(ctx->buffer[j + 3]) << 24) |
                          (static_cast<uint32_t>(ctx->buffer[j + 2]) << 16) |
                          (static_cast<uint32_t>(ctx->buffer[j + 1]) << 8) |
                          (static_cast<uint32_t>(ctx->buffer[j]));
                j += 4;
            }
            Transform(ctx->state, input);
            k = 0;
        }
    }
}

void ModifiedMd5Context::Consume(const uint8_t* data, size_t length) {
    if (length == 0) return;

#ifdef _WIN32
    // On 32-bit Windows, process in chunks
    constexpr size_t MAX_CHUNK_SIZE = UINT32_MAX;
    while (length > 0) {
        size_t chunkSize = (length > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : length;
        ConsumeChunk(this, data, chunkSize);
        data += chunkSize;
        length -= chunkSize;
    }
#else
    // On 64-bit systems, process all at once
    ConsumeChunk(this, data, length);
#endif
}

void ModifiedMd5Context::Consume(const std::vector<uint8_t>& data) {
    Consume(data.data(), data.size());
}

} // namespace fake_sign
} // namespace hassle
