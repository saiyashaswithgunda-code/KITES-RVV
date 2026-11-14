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
    uint32_t vector_instruction)
{
    Kites::AssembledProgram program;

    program.text_buffer = {
        EncodeVsetvli(sew_bits),
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