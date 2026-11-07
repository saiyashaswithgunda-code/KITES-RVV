#include "processor/vector_config.h"
#include "processor/registers.h"

namespace Kites
{

VectorConfig DecodeVectorConfig(uint64_t vtype)
{
    VectorConfig config{};

    // RV64: vill is bit 63.
    constexpr uint64_t VILL_MASK = uint64_t{1} << 63;

    // Bits 62:8 are reserved and must be zero.
    constexpr uint64_t RESERVED_MASK = 0x7FFFFFFFFFFFFF00ULL;

    if ((vtype & VILL_MASK) != 0 ||
        (vtype & RESERVED_MASK) != 0)
    {
        return config;
    }

    // Extract vtype fields.
    uint8_t vlmul = vtype & 0b111;
    uint8_t vsew = (vtype >> 3) & 0b111;

    config.tail_agnostic = (vtype >> 6) & 1;
    config.mask_agnostic = (vtype >> 7) & 1;

    // Initially, support only LMUL = m1.
    if (vlmul != 0)
    {
        return VectorConfig{};
    }

    // Initially, support SEW = 8, 16, 32, or 64.
    if (vsew > 3)
    {
        return VectorConfig{};
    }

    config.sew_bits = 8 << vsew;
    config.lmul = 1;

    // VLMAX = (VLEN / SEW) * LMUL.
    config.vlmax =
        (RegisterFile::VLEN_BITS / config.sew_bits) * config.lmul;

    config.vill = false;

    return config;
}

uint64_t CalculateVectorLength(uint64_t avl, uint16_t vlmax)
{
    return (avl < vlmax) ? avl : vlmax;
}

} // namespace Kites