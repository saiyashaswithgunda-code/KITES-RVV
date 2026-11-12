
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

// Encode an unmasked vector-vector instruction.
uint32_t EncodeVectorVV(
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

template <typename T>
void TestArithmetic(
    uint32_t funct6,
    const std::vector<T> &lhs,
    const std::vector<T> &rhs,
    const std::vector<T> &expected)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor,
        sizeof(T) * 8,
        static_cast<unsigned int>(lhs.size()),
        EncodeVectorVV(funct6)
    );

    WriteElements(processor.registers_, 1, lhs);
    WriteElements(processor.registers_, 2, rhs);

    processor.Step();

    ExpectElements(processor.registers_, 3, expected);
}
} // namespace

// ---------------------------------------------------------
// 1–5. Basic arithmetic operations (SEW = 32)
// ---------------------------------------------------------

TEST(VectorArithmeticTest, VaddVV)
{
    TestArithmetic<uint32_t>(
        0b000000,
        {1, 10, 100, 1000},
        {2, 20, 200, 2000},
        {3, 30, 300, 3000}
    );
}

TEST(VectorArithmeticTest, VsubVV)
{
    TestArithmetic<uint32_t>(
        0b000010,
        {10, 100, 1000, 5000},
        {3, 20, 300, 1000},
        {7, 80, 700, 4000}
    );
}

TEST(VectorArithmeticTest, VandVV)
{
    TestArithmetic<uint32_t>(
        0b001001,
        {0xFF, 0xF0, 0xAA, 0x55},
        {0x0F, 0x0F, 0x55, 0xFF},
        {0x0F, 0x00, 0x00, 0x55}
    );
}

TEST(VectorArithmeticTest, VorVV)
{
    TestArithmetic<uint32_t>(
        0b001010,
        {0xF0, 0xF0, 0xAA, 0x55},
        {0x0F, 0x0F, 0x55, 0xFF},
        {0xFF, 0xFF, 0xFF, 0xFF}
    );
}

TEST(VectorArithmeticTest, VxorVV)
{
    TestArithmetic<uint32_t>(
        0b001011,
        {0xFF, 0xF0, 0xAA, 0x55},
        {0x0F, 0x0F, 0x55, 0xFF},
        {0xF0, 0xFF, 0xFF, 0xAA}
    );
}

// ---------------------------------------------------------
// 6–9. SEW variations
// ---------------------------------------------------------

TEST(VectorArithmeticTest, VaddSEW8)
{
    TestArithmetic<uint8_t>(
        0b000000,
        {10, 20, 30, 40},
        {1, 2, 3, 4},
        {11, 22, 33, 44}
    );
}

TEST(VectorArithmeticTest, VaddSEW16)
{
    TestArithmetic<uint16_t>(
        0b000000,
        {100, 200, 300, 400},
        {10, 20, 30, 40},
        {110, 220, 330, 440}
    );
}

TEST(VectorArithmeticTest, VaddSEW32)
{
    TestArithmetic<uint32_t>(
        0b000000,
        {1000, 2000, 3000, 4000},
        {10, 20, 30, 40},
        {1010, 2020, 3030, 4040}
    );
}

TEST(VectorArithmeticTest, VaddSEW64)
{
    TestArithmetic<uint64_t>(
        0b000000,
        {10000000000ULL, 20000000000ULL},
        {10, 20},
        {10000000010ULL, 20000000020ULL}
    );
}

// ---------------------------------------------------------
// 10. VL: inactive elements must remain unchanged
// ---------------------------------------------------------

TEST(VectorArithmeticTest, RespectsVectorLength)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4}
    );
    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40}
    );
    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {11, 22, 99, 99}
    );
}

// ---------------------------------------------------------
// 11. vstart: execution begins at the specified element
// ---------------------------------------------------------

TEST(VectorArithmeticTest, RespectsVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4}
    );
    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40}
    );
    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {99, 99, 33, 44}
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART),
        0
    );
}

// ---------------------------------------------------------
// 12. Destination aliases a source register
// ---------------------------------------------------------

TEST(VectorArithmeticTest, SupportsDestinationSourceOverlap)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd = vs1 = v2
    uint32_t instruction = EncodeVectorVV(
        0b000000, 2, 1, 2
    );

    LoadVectorProgram(processor, 32, 4, instruction);

    WriteElements<uint32_t>(
        processor.registers_, 1, {10, 20, 30, 40}
    );
    WriteElements<uint32_t>(
        processor.registers_, 2, {1, 2, 3, 4}
    );

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 2, {11, 22, 33, 44}
    );
}

// ---------------------------------------------------------
// 13. Undo
// ---------------------------------------------------------

TEST(VectorArithmeticTest, UndoRestoresVectorDestination)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4}
    );
    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40}
    );
    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {11, 22, 33, 44}
    );

    processor.Undo();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );
}

// ---------------------------------------------------------
// 14. Redo
// ---------------------------------------------------------

TEST(VectorArithmeticTest, RedoReappliesVectorDestination)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4}
    );
    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40}
    );
    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();
    processor.Undo();
    processor.Redo();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {11, 22, 33, 44}
    );
}
TEST(VectorArithmeticTest, MaskedExecution)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorVV(0b000000, 3, 1, 2, 0));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99});

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {11, 99, 33, 99});
}
TEST(VectorArithmeticTest, MaskAgnostic)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(32, false, true),
        EncodeVectorVV(0b000000, 3, 1, 2, 0)
    };

    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 4);
    processor.Step();

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99});

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3,
        {11, UINT32_MAX, 33, UINT32_MAX});
}
TEST(VectorArithmeticTest, TailUndisturbed)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 2, EncodeVectorVV(0b000000));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99});

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {11, 22, 99, 99});
}
TEST(VectorArithmeticTest, TailAgnostic)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(32, true, false),
        EncodeVectorVV(0b000000)
    };

    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 2);
    processor.Step();

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99});

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3,
        {11, 22, UINT32_MAX, UINT32_MAX});
}
TEST(VectorArithmeticTest, MaskedExecutionRespectsVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorVV(0b000000, 3, 1, 2, 0));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    WriteElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99});

    SetMask(processor.registers_, 0b00000101);

    processor.registers_.WriteCsr(
        RegisterFile::CSR_VSTART, 1);

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 3, {99, 99, 33, 99});

    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART),
        0);
}
TEST(VectorArithmeticTest, SupportsDestinationMaskOverlap)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorProgram(
        processor, 32, 4,
        EncodeVectorVV(0b000000, 0, 1, 2, 0));

    WriteElements<uint32_t>(
        processor.registers_, 1, {1, 2, 3, 4});

    WriteElements<uint32_t>(
        processor.registers_, 2, {10, 20, 30, 40});

    // Mask bits 0 and 2 are active.
    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectElements<uint32_t>(
        processor.registers_, 0,
        {11, 0, 33, 0});
}