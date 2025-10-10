#include "hassle/fake_sign/fake_sign.h"
#include "hassle/fake_sign/modified_md5.h"
#include <cstring>
#include <cassert>

namespace hassle {
namespace fake_sign {

#pragma pack(push, 1)
struct FileHeader {
    uint32_t fourcc;
    uint32_t hash_value[4];
    uint32_t container_version;
    uint32_t file_length;
    uint32_t num_chunks;
};
#pragma pack(pop)

constexpr size_t DXIL_HEADER_CONTAINER_VERSION_OFFSET = 20;
constexpr uint32_t DXBC_FOURCC = ((uint32_t)'D') | (((uint32_t)'X') << 8) | (((uint32_t)'B') << 16) | (((uint32_t)'C') << 24);

static uint32_t ReadFourCC(const std::vector<uint8_t>& dxil) {
    if (dxil.size() < sizeof(FileHeader)) {
        return 0;
    }
    const FileHeader* header = reinterpret_cast<const FileHeader*>(dxil.data());
    return header->fourcc;
}

static uint32_t ReadFileLength(const std::vector<uint8_t>& dxil) {
    if (dxil.size() < sizeof(FileHeader)) {
        return 0;
    }
    const FileHeader* header = reinterpret_cast<const FileHeader*>(dxil.data());
    return header->file_length;
}

static void WriteHashValue(std::vector<uint8_t>& dxil, const uint32_t state[4]) {
    if (dxil.size() < sizeof(FileHeader)) {
        return;
    }
    FileHeader* header = reinterpret_cast<FileHeader*>(dxil.data());
    memcpy(header->hash_value, state, sizeof(uint32_t) * 4);
}

bool FakeSignDxilInPlace(std::vector<uint8_t>& dxil) {
    if (ReadFourCC(dxil) != DXBC_FOURCC) {
        return false;
    }

    if (ReadFileLength(dxil) != static_cast<uint32_t>(dxil.size())) {
        return false;
    }

    // The hashable data starts immediately after the hash.
    const uint8_t* data = dxil.data() + DXIL_HEADER_CONTAINER_VERSION_OFFSET;
    size_t data_len = dxil.size() - DXIL_HEADER_CONTAINER_VERSION_OFFSET;

    uint32_t num_bits = static_cast<uint32_t>(data_len) * 8;
    uint32_t num_bits_part_2 = (num_bits >> 2) | 1;
    uint32_t left_over_len = static_cast<uint32_t>(data_len) % 64;

    size_t first_part_len = data_len - left_over_len;
    const uint8_t* first_part = data;
    const uint8_t* padding_part = data + first_part_len;

    ModifiedMd5Context ctx;
    ctx.Consume(first_part, first_part_len);

    uint8_t block[64] = {0};

    if (left_over_len >= 56) {
        assert(left_over_len <= data_len);
        ctx.Consume(padding_part, left_over_len);

        uint32_t marker = 0x80;
        memcpy(block, &marker, 4);
        ctx.Consume(block, 64 - left_over_len);

        // The final block contains the number of bits in the first dword, and the weird upper bits
        memset(block, 0, sizeof(block));
        memcpy(block, &num_bits, 4);

        // Write to last dword
        memcpy(block + 15 * 4, &num_bits_part_2, 4);

        ctx.Consume(block, 64);
    } else {
        ctx.Consume(reinterpret_cast<const uint8_t*>(&num_bits), 4);

        if (left_over_len != 0) {
            ctx.Consume(padding_part, left_over_len);
        }

        size_t padding_bytes = 64 - left_over_len - 4;

        memset(block, 0, sizeof(block));
        block[0] = 0x80;
        memcpy(block + padding_bytes - 4, &num_bits_part_2, 4);
        ctx.Consume(block, padding_bytes);
    }

    // DXIL signing is odd - it doesn't run the finalization step of the md5
    // algorithm but instead pokes the hasher state directly into container
    WriteHashValue(dxil, ctx.GetState());

    return true;
}

} // namespace fake_sign
} // namespace hassle
