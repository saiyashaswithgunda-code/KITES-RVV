#ifndef VECTOR_CONFIG_H
#define VECTOR_CONFIG_H

#include <cstdint>

namespace Kites
{

struct VectorConfig
{
    uint16_t sew_bits = 0; //selected element width
    uint8_t lmul = 0; 

    bool tail_agnostic = false;
    bool mask_agnostic = false;
    bool vill = true;

    uint16_t vlmax = 0;
};

// Decode and validate an RV64 vtype value.
[[nodiscard]] VectorConfig DecodeVectorConfig(uint64_t vtype);

[[nodiscard]] uint64_t CalculateVectorLength(
    uint64_t avl,
    uint16_t vlmax
);

} // namespace Kites



#endif