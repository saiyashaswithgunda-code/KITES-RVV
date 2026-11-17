#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "common/assembled_program.h"
#include "common/globals.h"
#include "processor/rvss/rvss_processor.h"
#include "processor/registers.h"
#include "utils/utils.h"

namespace
{
using Processor = Kites::RVSSProcessor;
using RegisterFile = Kites::RegisterFile;

void SetMask(RegisterFile &registers, uint8_t mask)
{
    registers.WriteVregElement<uint8_t>(0, 0, mask);
}

// Encode: vsetvli x0, x1, eSEW, m1, tu, mu
uint32_t EncodeVsetvli(
    unsigned int sew_bits,
    bool tail_agnostic = false,
    bool mask_agnostic = false)
{
    uint32_t vsew = 0;

    switch (sew_bits)
    {
    case 8:  vsew = 0; break;
    case 16: vsew = 1; break;
    case 32: vsew = 2; break;
    case 64: vsew = 3; break;
    default: return 0;
    }

    uint32_t vtype = (vsew << 3) |
                     (static_cast<uint32_t>(tail_agnostic) << 6) |
                     (static_cast<uint32_t>(mask_agnostic) << 7);

    return (vtype << 20) |
           (1u << 15) |
           (7u << 12) |
           0x57u;
}

// Encode a reduction (OP-VV with funct3=0b010) instruction.
uint32_t EncodeVectorReduction(
    uint32_t funct6,
    uint32_t vd = 3,
    uint32_t vs2 = 1,
    uint32_t vs1 = 2,
    uint32_t vm = 1)
{
    return (funct6 << 26) |
           (vm << 25) |
           (vs2 << 20) |
           (vs1 << 15) |
           (0b010 << 12) |
           (vd << 7) |
           0x57u;
}

void LoadVectorProgram(
    Processor &processor,
    unsigned int sew_bits,
    unsigned int avl,
    uint32_t vector_instruction,
    bool tail_agnostic = false,
    bool mask_agnostic = false)
{
    Kites::AssembledProgram program;

    program.text_buffer = {
        EncodeVsetvli(sew_bits, tail_agnostic, mask_agnostic),
        vector_instruction
    };

    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, avl);

    // Configure VL and VTYPE.
    processor.Step();
}

template <typename T>
void WriteElements(
    RegisterFile &registers,
    unsigned int reg,
    const std::vector<T> &values)
{
    for (size_t i = 0; i < values.size(); ++i)
        registers.WriteVregElement<T>(reg, i, values[i]);
}

template <typename T>
void ExpectElements(
    const RegisterFile &registers,
    unsigned int reg,
    const std::vector<T> &expected)
{
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(
            registers.ReadVregElement<T>(reg, i),
            expected[i]
        ) << "Mismatch at vector element " << i;
    }
}
} // namespace

// ---------------------------------------------------------
// 1. vredsum.vs: sum of vector elements plus scalar [vs1]
// ---------------------------------------------------------

TEST(VectorReductionTest, VredsumVS)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        20);
}

TEST(VectorReductionTest, VredsumMasked)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000000, 3, 1, 2, 0));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    // Only elements 0 and 2 are active.
    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        14);
}

TEST(VectorReductionTest, VredsumTailUndisturbed)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 2,
        EncodeVectorReduction(0b000000));

    // Initial accumulator and source.
    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2});

    // Initialize destination.
    WriteElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0), 13);

    ExpectElements<uint32_t>(
        processor.registers_, 3, {13, 200, 300, 400});
}

TEST(VectorReductionTest, VredsumTailAgnostic)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(32, true),  // vta=1
        EncodeVectorReduction(0b000000)
    };

    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 2);
    processor.Step();

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2});

    WriteElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0), 13);

    ExpectElements<uint32_t>(
        processor.registers_, 3,
        {13, UINT32_MAX, UINT32_MAX, UINT32_MAX});
}

TEST(VectorReductionTest, VredsumZeroVL)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 0,
        EncodeVectorReduction(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});
}

TEST(VectorReductionTest, VredsumNonzeroVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    processor.registers_.WriteCsr(
        RegisterFile::CSR_VSTART, 1);

    processor.Step();

    // Destination must remain unchanged.
    ExpectElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    // vstart must also remain unchanged.
    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1);
}

TEST(VectorReductionTest, VredsumSupportsAllSEW)
{
    Kites::setupVmStateDirectory();

    for (unsigned int sew : {8, 16, 64})
    {
        Processor processor;

        LoadVectorProgram(
            processor, sew, 2,
            EncodeVectorReduction(0b000000));

        if (sew == 8)
        {
            WriteElements<uint8_t>(processor.registers_, 2, {10});
            WriteElements<uint8_t>(processor.registers_, 1, {1, 2});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 13);
        }
        else if (sew == 16)
        {
            WriteElements<uint16_t>(processor.registers_, 2, {10});
            WriteElements<uint16_t>(processor.registers_, 1, {1, 2});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 13);
        }
        else
        {
            WriteElements<uint64_t>(processor.registers_, 2, {10});
            WriteElements<uint64_t>(processor.registers_, 1, {1, 2});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 13);
        }
    }
}

TEST(VectorReductionTest, VredsumWrapsAroundOnOverflow)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 2,
        EncodeVectorReduction(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 2, {UINT32_MAX});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        2);
}

TEST(VectorReductionTest, VredsumDestinationOverlapsVS2)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd=v1, vs2=v1, vs1=v2
    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000000, 1, 1, 2));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(1, 0), 16);
}

TEST(VectorReductionTest, VredsumDestinationOverlapsVS1)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd=v2, vs2=v1, vs1=v2
    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000000, 2, 1, 2));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(2, 0), 16);
}

TEST(VectorReductionTest, VredsumUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 2, {10});

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3});

    WriteElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    // Execute reduction.
    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {16, 200, 300, 400});

    // Undo reduction.
    processor.Undo();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400});

    // Redo reduction.
    processor.Redo();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {16, 200, 300, 400});
}

// ---------------------------------------------------------
// 2. vredmax.vs: signed maximum reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredmaxVS_SignedComparison)
{
    Kites::setupVmStateDirectory();
    {
        Processor processor;

        // vredmax.vs v3, v1, v2 (funct6=0b000111)
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000111));

        // vs1[0] = -5
        WriteElements<uint32_t>(
            processor.registers_, 2, {static_cast<uint32_t>(-5)});

        // vs2 = {-10, 20, -1, 15} -> max is 20
        WriteElements<uint32_t>(
            processor.registers_, 1, {static_cast<uint32_t>(-10), 20u, static_cast<uint32_t>(-1), 15u});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            20);
    }

    {
        Processor processor;

        // Also test strictly negative values: vs1[0] = -50, vs2 = {-20, -10, -5, -30} -> max is -5
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000111));

        WriteElements<uint32_t>(
            processor.registers_, 2, {static_cast<uint32_t>(-50)});
        WriteElements<uint32_t>(
            processor.registers_, 1, {static_cast<uint32_t>(-20), static_cast<uint32_t>(-10), static_cast<uint32_t>(-5), static_cast<uint32_t>(-30)});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            -5);
    }
}

TEST(VectorReductionTest, VredmaxSignedBoundaryValues)
{
    Kites::setupVmStateDirectory();
    const int32_t min_val = INT32_MIN;
    const int32_t max_val = INT32_MAX;

    {
        Processor processor;
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000111));

        // vs1[0] = INT32_MIN, vs2 = {-1, 0, INT32_MAX, -100} -> max is INT32_MAX
        WriteElements<uint32_t>(
            processor.registers_, 2, {static_cast<uint32_t>(min_val)});
        WriteElements<uint32_t>(
            processor.registers_, 1, {static_cast<uint32_t>(-1), 0u, static_cast<uint32_t>(max_val), static_cast<uint32_t>(-100)});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            max_val);
    }

    {
        Processor processor;
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000111));

        // Initial scalar is INT32_MAX, all vs2 elements are negative -> max remains INT32_MAX
        WriteElements<uint32_t>(
            processor.registers_, 2, {static_cast<uint32_t>(max_val)});
        WriteElements<uint32_t>(
            processor.registers_, 1, {static_cast<uint32_t>(min_val), static_cast<uint32_t>(-1), static_cast<uint32_t>(-500), static_cast<uint32_t>(-1000)});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            max_val);
    }
}

// ---------------------------------------------------------
// 3. vredmaxu.vs: unsigned maximum reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredmaxuVS_UnsignedComparison)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredmaxu.vs v3, v1, v2 (funct6=0b000110)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000110));

    // In unsigned arithmetic, 0xFFFFFFFFU is 4294967295U, which is > 100U
    WriteElements<uint32_t>(
        processor.registers_, 2, {100u});
    WriteElements<uint32_t>(
        processor.registers_, 1, {1u, 0xFFFFFFFFu, 50u, 10u});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        0xFFFFFFFFu);
}

// ---------------------------------------------------------
// 4. vredmin.vs: signed minimum reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredminVS_SignedComparison)
{
    Kites::setupVmStateDirectory();
    {
        Processor processor;

        // vredmin.vs v3, v1, v2 (funct6=0b000101)
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000101));

        WriteElements<uint32_t>(
            processor.registers_, 2, {5u});
        WriteElements<uint32_t>(
            processor.registers_, 1, {10u, static_cast<uint32_t>(-3), 2u, static_cast<uint32_t>(-15)});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            -15);
    }

    {
        Processor processor;

        // Case where vs1[0] is the minimum
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000101));

        WriteElements<uint32_t>(
            processor.registers_, 2, {static_cast<uint32_t>(-100)});
        WriteElements<uint32_t>(
            processor.registers_, 1, {static_cast<uint32_t>(-10), static_cast<uint32_t>(-3), 2u, static_cast<uint32_t>(-15)});

        processor.Step();

        EXPECT_EQ(
            static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
            -100);
    }
}

TEST(VectorReductionTest, VredminSignedBoundaryValues)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000101));

    const int32_t min_val = INT32_MIN;
    const int32_t max_val = INT32_MAX;

    // vs1[0] = INT32_MAX, vs2 = {0, -1, INT32_MIN, 100} -> min is INT32_MIN
    WriteElements<uint32_t>(
        processor.registers_, 2, {static_cast<uint32_t>(max_val)});
    WriteElements<uint32_t>(
        processor.registers_, 1, {0u, static_cast<uint32_t>(-1), static_cast<uint32_t>(min_val), 100u});

    processor.Step();

    EXPECT_EQ(
        static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)),
        min_val);
}

// ---------------------------------------------------------
// 5. vredminu.vs: unsigned minimum reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredminuVS_UnsignedComparison)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredminu.vs v3, v1, v2 (funct6=0b000100)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000100));

    // 0xFFFFFFFFu and 0x80000000u are large unsigned values; min should be 10
    WriteElements<uint32_t>(
        processor.registers_, 2, {0xFFFFFFFFu});
    WriteElements<uint32_t>(
        processor.registers_, 1, {0x80000000u, 50u, 0xFFFFFFFEu, 10u});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        10u);
}

// ---------------------------------------------------------
// 6. vredand.vs: bitwise AND reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredandVS_BitwiseOperation)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredand.vs v3, v1, v2 (funct6=0b000001)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000001));

    // Initial scalar = identity ~0 (0xFFFFFFFF)
    WriteElements<uint32_t>(
        processor.registers_, 2, {0xFFFFFFFFu});
    WriteElements<uint32_t>(
        processor.registers_, 1, {0x00FF00FFu, 0x0F0F0F0Fu, 0x00FF0F0Fu, 0xFFFFFFFFu});

    processor.Step();

    // 0x00FF00FF & 0x0F0F0F0F = 0x000F000F
    // 0x000F000F & 0x00FF0F0F = 0x000F000F
    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        0x000F000Fu);
}

// ---------------------------------------------------------
// 7. vredor.vs: bitwise OR reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredorVS_BitwiseOperation)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredor.vs v3, v1, v2 (funct6=0b000010)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000010));

    // Initial scalar = identity 0
    WriteElements<uint32_t>(
        processor.registers_, 2, {0x00000000u});
    WriteElements<uint32_t>(
        processor.registers_, 1, {0x00000001u, 0x00000020u, 0x00000300u, 0x00004000u});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        0x00004321u);
}

// ---------------------------------------------------------
// 8. vredxor.vs: bitwise XOR reduction
// ---------------------------------------------------------

TEST(VectorReductionTest, VredxorVS_BitwiseOperation)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredxor.vs v3, v1, v2 (funct6=0b000011)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000011));

    // Parity and cancellation: vs1[0] = 0xAA, vs2 = {0x55, 0xAA, 0x55, 0x12}
    // Result should be 0x12 because 0xAA and 0x55 cancel out in pairs
    WriteElements<uint32_t>(
        processor.registers_, 2, {0xAAu});
    WriteElements<uint32_t>(
        processor.registers_, 1, {0x55u, 0xAAu, 0x55u, 0x12u});

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadVregElement<uint32_t>(3, 0),
        0x12u);
}

// ---------------------------------------------------------
// 9. All SEW Support (8, 16, 32, 64)
// ---------------------------------------------------------

TEST(VectorReductionTest, ReductionSupportsAllSEW)
{
    Kites::setupVmStateDirectory();

    // Verify all 7 new instructions across 8, 16, 32, 64-bit SEW
    for (unsigned int sew : {8, 16, 32, 64})
    {
        // 1. vredand
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000001));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {0xFF});
                WriteElements<uint8_t>(processor.registers_, 1, {0x0F, 0x07});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x07);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {0xFFFF});
                WriteElements<uint16_t>(processor.registers_, 1, {0x00FF, 0x000F});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 0x000F);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {0xFFFFFFFFu});
                WriteElements<uint32_t>(processor.registers_, 1, {0x0000FFFFu, 0x000000FFu});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x000000FFu);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {UINT64_MAX});
                WriteElements<uint64_t>(processor.registers_, 1, {0x00000000FFFFFFFFULL, 0x000000000000FFFFULL});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 0x000000000000FFFFULL);
            }
        }

        // 2. vredor
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000010));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {0x00});
                WriteElements<uint8_t>(processor.registers_, 1, {0x01, 0x02});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x03);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {0x0000});
                WriteElements<uint16_t>(processor.registers_, 1, {0x0001, 0x0020});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 0x0021);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {0u});
                WriteElements<uint32_t>(processor.registers_, 1, {0x0001u, 0x0200u});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x0201u);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {0ULL});
                WriteElements<uint64_t>(processor.registers_, 1, {0x0001ULL, 0x2000ULL});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 0x2001ULL);
            }
        }

        // 3. vredxor
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000011));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {0xAA});
                WriteElements<uint8_t>(processor.registers_, 1, {0xAA, 0x55});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x55);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {0xAAAA});
                WriteElements<uint16_t>(processor.registers_, 1, {0xAAAA, 0x5555});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 0x5555);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {0xAAAAAAAAu});
                WriteElements<uint32_t>(processor.registers_, 1, {0xAAAAAAAAu, 0x55555555u});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x55555555u);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {0xAAAAAAAAAAAAAAAAULL});
                WriteElements<uint64_t>(processor.registers_, 1, {0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 0x5555555555555555ULL);
            }
        }

        // 4. vredminu
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000100));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {0xFF});
                WriteElements<uint8_t>(processor.registers_, 1, {0x10, 0x05});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x05);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {0xFFFF});
                WriteElements<uint16_t>(processor.registers_, 1, {0x10, 0x05});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 0x05);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {0xFFFFFFFFu});
                WriteElements<uint32_t>(processor.registers_, 1, {0x10u, 0x05u});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x05u);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {UINT64_MAX});
                WriteElements<uint64_t>(processor.registers_, 1, {0x10ULL, 0x05ULL});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 0x05ULL);
            }
        }

        // 5. vredmin (signed negative comparisons)
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000101));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {static_cast<uint8_t>(-5)});
                WriteElements<uint8_t>(processor.registers_, 1, {static_cast<uint8_t>(-10), static_cast<uint8_t>(15)});
                processor.Step();
                EXPECT_EQ(static_cast<int8_t>(processor.registers_.ReadVregElement<uint8_t>(3, 0)), -10);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {static_cast<uint16_t>(-5)});
                WriteElements<uint16_t>(processor.registers_, 1, {static_cast<uint16_t>(-10), static_cast<uint16_t>(15)});
                processor.Step();
                EXPECT_EQ(static_cast<int16_t>(processor.registers_.ReadVregElement<uint16_t>(3, 0)), -10);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {static_cast<uint32_t>(-5)});
                WriteElements<uint32_t>(processor.registers_, 1, {static_cast<uint32_t>(-10), 15u});
                processor.Step();
                EXPECT_EQ(static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)), -10);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {static_cast<uint64_t>(-5LL)});
                WriteElements<uint64_t>(processor.registers_, 1, {static_cast<uint64_t>(-10LL), 15ULL});
                processor.Step();
                EXPECT_EQ(static_cast<int64_t>(processor.registers_.ReadVregElement<uint64_t>(3, 0)), -10LL);
            }
        }

        // 6. vredmaxu
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000110));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {0x01});
                WriteElements<uint8_t>(processor.registers_, 1, {0xFF, 0x05});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {0x0001});
                WriteElements<uint16_t>(processor.registers_, 1, {0xFFFF, 0x0005});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 0xFFFF);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {1u});
                WriteElements<uint32_t>(processor.registers_, 1, {0xFFFFFFFFu, 5u});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0xFFFFFFFFu);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {1ULL});
                WriteElements<uint64_t>(processor.registers_, 1, {UINT64_MAX, 5ULL});
                processor.Step();
                EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), UINT64_MAX);
            }
        }

        // 7. vredmax (signed negative comparisons)
        {
            Processor processor;
            LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000111));
            if (sew == 8) {
                WriteElements<uint8_t>(processor.registers_, 2, {static_cast<uint8_t>(-50)});
                WriteElements<uint8_t>(processor.registers_, 1, {static_cast<uint8_t>(-10), static_cast<uint8_t>(-20)});
                processor.Step();
                EXPECT_EQ(static_cast<int8_t>(processor.registers_.ReadVregElement<uint8_t>(3, 0)), -10);
            } else if (sew == 16) {
                WriteElements<uint16_t>(processor.registers_, 2, {static_cast<uint16_t>(-50)});
                WriteElements<uint16_t>(processor.registers_, 1, {static_cast<uint16_t>(-10), static_cast<uint16_t>(-20)});
                processor.Step();
                EXPECT_EQ(static_cast<int16_t>(processor.registers_.ReadVregElement<uint16_t>(3, 0)), -10);
            } else if (sew == 32) {
                WriteElements<uint32_t>(processor.registers_, 2, {static_cast<uint32_t>(-50)});
                WriteElements<uint32_t>(processor.registers_, 1, {static_cast<uint32_t>(-10), static_cast<uint32_t>(-20)});
                processor.Step();
                EXPECT_EQ(static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)), -10);
            } else {
                WriteElements<uint64_t>(processor.registers_, 2, {static_cast<uint64_t>(-50LL)});
                WriteElements<uint64_t>(processor.registers_, 1, {static_cast<uint64_t>(-10LL), static_cast<uint64_t>(-20LL)});
                processor.Step();
                EXPECT_EQ(static_cast<int64_t>(processor.registers_.ReadVregElement<uint64_t>(3, 0)), -10LL);
            }
        }
    }
}

// ---------------------------------------------------------
// 10. Masking Tests
// ---------------------------------------------------------

TEST(VectorReductionTest, ReductionMaskedAcrossAllSEW)
{
    Kites::setupVmStateDirectory();

    for (unsigned int sew : {8, 16, 32, 64})
    {
        Processor processor;
        // vredminu.vs masked (funct6=0b000100, vm=0)
        LoadVectorProgram(processor, sew, 2, EncodeVectorReduction(0b000100, 3, 1, 2, 0));

        // Mask: only element 1 is active (0b00000010)
        SetMask(processor.registers_, 0b00000010);

        if (sew == 8)
        {
            WriteElements<uint8_t>(processor.registers_, 2, {50});
            // element 0 (100) is masked off; active element 1 is 20
            WriteElements<uint8_t>(processor.registers_, 1, {100, 20});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 20);
        }
        else if (sew == 16)
        {
            WriteElements<uint16_t>(processor.registers_, 2, {50});
            WriteElements<uint16_t>(processor.registers_, 1, {100, 20});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint16_t>(3, 0), 20);
        }
        else if (sew == 32)
        {
            WriteElements<uint32_t>(processor.registers_, 2, {50u});
            WriteElements<uint32_t>(processor.registers_, 1, {100u, 20u});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 20u);
        }
        else
        {
            WriteElements<uint64_t>(processor.registers_, 2, {50ULL});
            WriteElements<uint64_t>(processor.registers_, 1, {100ULL, 20ULL});
            processor.Step();
            EXPECT_EQ(processor.registers_.ReadVregElement<uint64_t>(3, 0), 20ULL);
        }
    }
}

TEST(VectorReductionTest, ReductionSingleActiveElement)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredmax.vs v3, v1, v2, v0.t (funct6=0b000111, vm=0)
    LoadVectorProgram(processor, 32, 4, EncodeVectorReduction(0b000111, 3, 1, 2, 0));

    // Only element 2 is active: mask = 0b00000100
    SetMask(processor.registers_, 0b00000100);

    WriteElements<uint32_t>(processor.registers_, 2, {static_cast<uint32_t>(-10)});
    // Inactive elements have 100, active element 2 has -20
    WriteElements<uint32_t>(processor.registers_, 1, {100u, 200u, static_cast<uint32_t>(-20), 300u});

    processor.Step();

    // max(-10, -20) = -10
    EXPECT_EQ(static_cast<int32_t>(processor.registers_.ReadVregElement<uint32_t>(3, 0)), -10);
}

TEST(VectorReductionTest, ReductionNoActiveElementsAllMaskedOff)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredand.vs v3, v1, v2, v0.t (funct6=0b000001, vm=0)
    LoadVectorProgram(processor, 32, 4, EncodeVectorReduction(0b000001, 3, 1, 2, 0));

    // All elements in [0, vl) masked off: mask = 0
    SetMask(processor.registers_, 0b00000000);

    WriteElements<uint32_t>(processor.registers_, 2, {0x12345678u});
    WriteElements<uint32_t>(processor.registers_, 1, {0u, 0u, 0u, 0u});

    processor.Step();

    // RVV 1.0 section 13.1: If all elements masked off, destination element 0 receives initial scalar value
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x12345678u);
}

TEST(VectorReductionTest, ReductionScalarInitialValueIncludedRegardlessOfMask)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredor.vs v3, v1, v2, v0.t (funct6=0b000010, vm=0)
    LoadVectorProgram(processor, 32, 4, EncodeVectorReduction(0b000010, 3, 1, 2, 0));

    // Mask has bit 0 = 0 (element 0 inactive), but elements 1..3 active (0b00001110)
    SetMask(processor.registers_, 0b00001110);

    // Initial scalar has bit 0 set (0x1)
    WriteElements<uint32_t>(processor.registers_, 2, {0x0001u});
    WriteElements<uint32_t>(processor.registers_, 1, {0x0000u, 0x0010u, 0x0100u, 0x1000u});

    processor.Step();

    // Initial scalar vs1[0]=0x0001 is included even though v0[0]==0
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x1111u);
}

TEST(VectorReductionTest, ReductionIgnoresVmaPolicy)
{
    Kites::setupVmStateDirectory();

    for (bool mask_agnostic : {false, true})
    {
        Processor processor;

        // vredxor.vs v3, v1, v2, v0.t (funct6=0b000011, vm=0)
        LoadVectorProgram(
            processor, 32, 4,
            EncodeVectorReduction(0b000011, 3, 1, 2, 0),
            false, mask_agnostic);

        SetMask(processor.registers_, 0b00000101); // elements 0, 2 active

        WriteElements<uint32_t>(processor.registers_, 2, {0xFFu});
        WriteElements<uint32_t>(processor.registers_, 1, {0x0Fu, 0xF0u, 0x33u, 0x55u});
        WriteElements<uint32_t>(processor.registers_, 3, {10u, 20u, 30u, 40u});

        processor.Step();

        // 0xFF ^ 0x0F ^ 0x33 = 0xC3
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0xC3u);
        // vma does not alter tail or scalar result
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 20u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 2), 30u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 3), 40u);
    }
}

// ---------------------------------------------------------
// 11. Tail, Boundary and State Policy Tests
// ---------------------------------------------------------

TEST(VectorReductionTest, ReductionTailUndisturbed)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredmax.vs with avl = 2, vta = 0
    LoadVectorProgram(
        processor, 32, 2,
        EncodeVectorReduction(0b000111),
        false); // tail undisturbed

    WriteElements<uint32_t>(processor.registers_, 2, {5u});
    WriteElements<uint32_t>(processor.registers_, 1, {10u, 20u});
    WriteElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 20u);
    // Tail elements [1, VLMAX) remain undisturbed
    ExpectElements<uint32_t>(processor.registers_, 3, {20u, 200u, 300u, 400u});
}

TEST(VectorReductionTest, ReductionTailAgnostic)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredand.vs with avl = 2, vta = 1
    LoadVectorProgram(
        processor, 32, 2,
        EncodeVectorReduction(0b000001),
        true); // tail agnostic

    WriteElements<uint32_t>(processor.registers_, 2, {0xFFFFFFFFu});
    WriteElements<uint32_t>(processor.registers_, 1, {0x00FF00FFu, 0x0000FFFFu});
    WriteElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x000000FFu);
    // Tail elements [1, VLMAX) are overwritten with all 1s
    ExpectElements<uint32_t>(
        processor.registers_, 3,
        {0x000000FFu, UINT32_MAX, UINT32_MAX, UINT32_MAX});
}

TEST(VectorReductionTest, ReductionZeroVL)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredor.vs with vl = 0
    LoadVectorProgram(
        processor, 32, 0,
        EncodeVectorReduction(0b000010));

    WriteElements<uint32_t>(processor.registers_, 2, {0x1234u});
    WriteElements<uint32_t>(processor.registers_, 1, {1u, 2u, 3u, 4u});
    WriteElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    processor.Step();

    // Destination must remain unchanged when vl = 0
    ExpectElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});
}

TEST(VectorReductionTest, ReductionNonzeroVstartLeavesStateUnchanged)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000101)); // vredmin.vs

    WriteElements<uint32_t>(processor.registers_, 2, {10u});
    WriteElements<uint32_t>(processor.registers_, 1, {1u, 2u, 3u, 4u});
    WriteElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);

    processor.Step();

    // Destination remains unchanged
    ExpectElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    // vstart remains unchanged
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1);
}

// ---------------------------------------------------------
// 12. Register Overlap Tests
// ---------------------------------------------------------

TEST(VectorReductionTest, ReductionDestinationOverlapsV0Mask)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd = 0 (v0), vs2 = 1, vs1 = 2, masked (vm=0)
    // vredmax.vs v0, v1, v2, v0.t (funct6=0b000111, vd=0, vs2=1, vs1=2, vm=0)
    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorReduction(0b000111, 0, 1, 2, 0));

    // Initial mask in v0: elements 0 and 2 active (mask bits 0 and 2 set: 0b00000101)
    SetMask(processor.registers_, 0b00000101);

    WriteElements<uint32_t>(processor.registers_, 2, {0u});
    // Active element 0 is 10, active element 2 is 50. Inactive element 1 is 1000.
    WriteElements<uint32_t>(processor.registers_, 1, {10u, 1000u, 50u, static_cast<uint32_t>(-500)});

    processor.Step();

    // Result max(0, 10, 50) = 50 should be in v0[0]
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 0), 50u);
}

TEST(VectorReductionTest, ReductionDestinationOverlapsVS2)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd = 1, vs2 = 1, vs1 = 2
    // vredand.vs v1, v1, v2 (funct6=0b000001, vd=1, vs2=1, vs1=2)
    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000001, 1, 1, 2));

    WriteElements<uint32_t>(processor.registers_, 2, {0xFFFFFFFFu});
    WriteElements<uint32_t>(processor.registers_, 1, {0xFF00u, 0x0FF0u, 0x00FFu});

    processor.Step();

    // 0xFF00 & 0x0FF0 & 0x00FF = 0
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 0), 0u);
}

TEST(VectorReductionTest, ReductionDestinationOverlapsVS1)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd = 2, vs2 = 1, vs1 = 2
    // vredxor.vs v2, v1, v2 (funct6=0b000011, vd=2, vs2=1, vs1=2)
    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000011, 2, 1, 2));

    WriteElements<uint32_t>(processor.registers_, 2, {0x10u});
    WriteElements<uint32_t>(processor.registers_, 1, {0x20u, 0x40u, 0x80u});

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 0), 0xF0u);
}

// ---------------------------------------------------------
// 13. Reversibility / Undo-Redo Test
// ---------------------------------------------------------

TEST(VectorReductionTest, ReductionUndoRedoFidelity)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vredmax.vs v3, v1, v2
    LoadVectorProgram(
        processor, 32, 3,
        EncodeVectorReduction(0b000111));

    WriteElements<uint32_t>(processor.registers_, 2, {static_cast<uint32_t>(-50)});
    WriteElements<uint32_t>(processor.registers_, 1, {static_cast<uint32_t>(-10), static_cast<uint32_t>(-5), static_cast<uint32_t>(-20)});
    WriteElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    processor.Step();

    // Result should be -5
    ExpectElements<uint32_t>(processor.registers_, 3, {static_cast<uint32_t>(-5), 200u, 300u, 400u});

    // Undo
    processor.Undo();
    ExpectElements<uint32_t>(processor.registers_, 3, {100u, 200u, 300u, 400u});

    // Redo
    processor.Redo();
    ExpectElements<uint32_t>(processor.registers_, 3, {static_cast<uint32_t>(-5), 200u, 300u, 400u});
}

