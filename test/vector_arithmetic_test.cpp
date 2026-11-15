
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

uint32_t EncodeVectorVX(
    uint32_t funct6,
    uint32_t vd = 3,
    uint32_t vs2 = 1,
    uint32_t rs1 = 2,
    uint32_t vm = 1)
{
    return (funct6 << 26) |
           (vm << 25) |
           (vs2 << 20) |
           (rs1 << 15) |
           (0b100 << 12) |
           (vd << 7) |
           0x57u;
}

uint32_t EncodeVectorVI(
    uint32_t funct6,
    uint32_t vd = 3,
    uint32_t vs2 = 1,
    int32_t simm5 = 0,
    uint32_t vm = 1)
{
    return (funct6 << 26) |
           (vm << 25) |
           (vs2 << 20) |
           ((static_cast<uint32_t>(simm5) & 0x1Fu) << 15) |
           (0b011 << 12) |
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

void LoadVectorVXProgram(
    Processor &processor,
    unsigned int sew_bits,
    unsigned int avl,
    uint32_t vector_instruction,
    uint64_t scalar_val,
    uint32_t rs1 = 2,
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
    processor.registers_.WriteGpr(rs1, scalar_val);

    // Configure VL and VTYPE.
    processor.Step();
}

template <typename T>
void TestArithmeticVX(
    uint32_t funct6,
    const std::vector<T> &lhs,
    uint64_t scalar_val,
    const std::vector<T> &expected,
    unsigned int avl = 0)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    if (avl == 0)
        avl = static_cast<unsigned int>(lhs.size());

    LoadVectorVXProgram(
        processor,
        sizeof(T) * 8,
        avl,
        EncodeVectorVX(funct6),
        scalar_val
    );

    WriteElements(processor.registers_, 1, lhs);

    processor.Step();

    ExpectElements(processor.registers_, 3, expected);
}

void LoadVectorVIProgram(
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
void TestArithmeticVI(
    uint32_t funct6,
    const std::vector<T> &lhs,
    int32_t simm5,
    const std::vector<T> &expected,
    unsigned int avl = 0)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    if (avl == 0)
        avl = static_cast<unsigned int>(lhs.size());

    LoadVectorVIProgram(
        processor,
        sizeof(T) * 8,
        avl,
        EncodeVectorVI(funct6, 3, 1, simm5)
    );

    WriteElements(processor.registers_, 1, lhs);

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

// ---------------------------------------------------------
// Vector-Scalar (.vx) tests
// ---------------------------------------------------------

TEST(VectorArithmeticTest, VaddVXBasic)
{
    TestArithmeticVX<uint32_t>(
        0b000000,
        {10, 20, 30, 40},
        5,
        {15, 25, 35, 45}
    );
}

TEST(VectorArithmeticTest, VsubVXBasic)
{
    TestArithmeticVX<uint32_t>(
        0b000010,
        {10, 20, 30, 40},
        5,
        {5, 15, 25, 35}
    );
}

TEST(VectorArithmeticTest, VaddVXSEW8)
{
    std::vector<uint8_t> input(16, 10);
    std::vector<uint8_t> expected(16, 15);
    TestArithmeticVX<uint8_t>(0b000000, input, 5, expected);
}

TEST(VectorArithmeticTest, VaddVXSEW16)
{
    std::vector<uint16_t> input = {100, 200, 300, 400, 500, 600, 700, 800};
    std::vector<uint16_t> expected = {150, 250, 350, 450, 550, 650, 750, 850};
    TestArithmeticVX<uint16_t>(0b000000, input, 50, expected);
}

TEST(VectorArithmeticTest, VaddVXSEW32)
{
    std::vector<uint32_t> input = {1000, 2000, 3000, 4000};
    std::vector<uint32_t> expected = {1500, 2500, 3500, 4500};
    TestArithmeticVX<uint32_t>(0b000000, input, 500, expected);
}

TEST(VectorArithmeticTest, VaddVXSEW64)
{
    std::vector<uint64_t> input = {1000000ULL, 2000000ULL};
    std::vector<uint64_t> expected = {1500000ULL, 2500000ULL};
    TestArithmeticVX<uint64_t>(0b000000, input, 500000ULL, expected);
}

TEST(VectorArithmeticTest, VsubVXSEW8)
{
    std::vector<uint8_t> input = {20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170};
    std::vector<uint8_t> expected = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};
    TestArithmeticVX<uint8_t>(0b000010, input, 10, expected);
}

TEST(VectorArithmeticTest, VsubVXSEW64)
{
    std::vector<uint64_t> input = {1000000ULL, 2000000ULL};
    std::vector<uint64_t> expected = {900000ULL, 1900000ULL};
    TestArithmeticVX<uint64_t>(0b000010, input, 100000ULL, expected);
}

TEST(VectorArithmeticTest, VaddVXScalarTruncation)
{
    const uint64_t scalar = 0x123456789ABCDEF0ULL;

    // SEW = 8: lowest 8 bits is 0xF0 (240).
    // (10 + 240) % 256 = 250.
    {
        std::vector<uint8_t> input(16, 10);
        std::vector<uint8_t> expected(16, 250);
        TestArithmeticVX<uint8_t>(0b000000, input, scalar, expected);
    }

    // SEW = 16: lowest 16 bits is 0xDEF0 (57072).
    // (100 + 57072) % 65536 = 57172.
    {
        std::vector<uint16_t> input(8, 100);
        std::vector<uint16_t> expected(8, 57172);
        TestArithmeticVX<uint16_t>(0b000000, input, scalar, expected);
    }

    // SEW = 32: lowest 32 bits is 0x9ABCDEF0.
    {
        std::vector<uint32_t> input(4, 1000);
        std::vector<uint32_t> expected(4, static_cast<uint32_t>(1000 + 0x9ABCDEF0ULL));
        TestArithmeticVX<uint32_t>(0b000000, input, scalar, expected);
    }
}

TEST(VectorArithmeticTest, VaddVXOverflowAndUnderflow)
{
    // Addition overflow wrapping modulo 2^32.
    TestArithmeticVX<uint32_t>(
        0b000000,
        {0xFFFFFFFFu, 0xFFFFFFFEu},
        1,
        {0u, 0xFFFFFFFFu},
        2
    );

    // Subtraction underflow wrapping modulo 2^32.
    TestArithmeticVX<uint32_t>(
        0b000010,
        {0u, 1u},
        1,
        {0xFFFFFFFFu, 0u},
        2
    );
}

TEST(VectorArithmeticTest, VaddVXNegativeScalar)
{
    // Negative scalar in two's complement.
    const uint64_t neg_scalar = static_cast<uint64_t>(-5LL);

    TestArithmeticVX<uint32_t>(
        0b000000,
        {10, 20, 30, 40},
        neg_scalar,
        {5, 15, 25, 35}
    );
}

TEST(VectorArithmeticTest, VaddVXVaryingVL)
{
    // vl = 0: no elements should be modified.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 0, EncodeVectorVX(0b000000), 5);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});
    }

    // vl = 1: only element 0 modified.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 1, EncodeVectorVX(0b000000), 5);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 99, 99, 99});
    }

    // vl = 2: elements 0 and 1 modified.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b000000), 5);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 25, 99, 99});
    }
}

TEST(VectorArithmeticTest, VaddVXVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000), 10);

    WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
    WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 100});

    // Start at element 2.
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

    processor.Step();

    // Elements 0 and 1 remain undisturbed, 2 and 3 computed.
    ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 13, 14});

    // vstart must be reset to 0.
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0);
}

TEST(VectorArithmeticTest, VaddVXVstartEqualToVL)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000), 10);

    WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
    WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 100});

    // vstart == vl: no active elements.
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 4);

    processor.Step();

    ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 100});
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0);
}

TEST(VectorArithmeticTest, VaddVXMaskedExecution)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vm = 0: masked execution.
    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 10);

    WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
    WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});

    // Mask: elements 0 and 2 active.
    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectElements<uint32_t>(processor.registers_, 3, {11, 77, 13, 77});
}

TEST(VectorArithmeticTest, VaddVXMaskPatterns)
{
    // All inactive mask: 0x00.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 10);
        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
        SetMask(processor.registers_, 0x00);

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
    }

    // All active mask: 0xFF.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 10);
        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
        SetMask(processor.registers_, 0xFF);

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {11, 12, 13, 14});
    }

    // Alternating mask: 0b00001010 (elements 1 and 3 active).
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 10);
        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
        SetMask(processor.registers_, 0b00001010);

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {77, 12, 77, 14});
    }
}

TEST(VectorArithmeticTest, VaddVXDestinationSourceAliasing)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd == vs2 (v1 = v1 + x2).
    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 1, 1, 2, 1), 10);

    WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});

    processor.Step();

    ExpectElements<uint32_t>(processor.registers_, 1, {11, 12, 13, 14});
}

TEST(VectorArithmeticTest, VaddVXDestinationMaskOverlapMasked)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vd == v0 with vm=0: destination is the mask register itself.
    // Mask in v0 = 0b00000101 (elements 0 and 2 active).
    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 0, 1, 2, 0), 10);

    WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});

    // Initialize v0 with the mask in byte 0 and zeros in subsequent bytes.
    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    // Even though element 0 modifies v0, the snapshotted mask register must ensure
    // that element 2 was correctly executed as active.
    // Inactive elements 1 and 3 remain 0.
    ExpectElements<uint32_t>(processor.registers_, 0, {11, 0, 13, 0});
}

TEST(VectorArithmeticTest, VaddVXBoundaryRegisters)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // rs1 = x0 (always zero): adding x0 leaves values identical.
    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 31, 31, 0, 1), 999, 0);

    WriteElements<uint32_t>(processor.registers_, 31, {10, 20, 30, 40});

    processor.Step();

    ExpectElements<uint32_t>(processor.registers_, 31, {10, 20, 30, 40});
}

TEST(VectorArithmeticTest, VaddVXTailPolicy)
{
    Kites::setupVmStateDirectory();

    // Tail-undisturbed (vta = 0): elements [vl, VLMAX) are untouched.
    {
        Processor processor;
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b000000), 5, 2, false, false);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 25, 99, 99});
    }

    // Tail-agnostic (vta = 1): elements [vl, VLMAX) are set to all-ones.
    {
        Processor processor;
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b000000), 5, 2, true, false);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 25, 0xFFFFFFFFu, 0xFFFFFFFFu});
    }
}

TEST(VectorArithmeticTest, VaddVXMaskPolicy)
{
    Kites::setupVmStateDirectory();

    // Mask-undisturbed (vma = 0): inactive elements retain old values.
    {
        Processor processor;
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 5, 2, false, false);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
        SetMask(processor.registers_, 0b00000101);

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 77, 35, 77});
    }

    // Mask-agnostic (vma = 1): inactive elements set to all-ones.
    {
        Processor processor;
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 0), 5, 2, false, true);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 77, 77, 77});
        SetMask(processor.registers_, 0b00000101);

        processor.Step();

        ExpectElements<uint32_t>(processor.registers_, 3, {15, 0xFFFFFFFFu, 35, 0xFFFFFFFFu});
    }
}

TEST(VectorArithmeticTest, VaddVXPreservesUnrelatedState)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 1), 5);

    WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
    WriteElements<uint32_t>(processor.registers_, 4, {111, 222, 333, 444});

    processor.Step();

    // Register v4 must be completely untouched.
    ExpectElements<uint32_t>(processor.registers_, 4, {111, 222, 333, 444});
}

TEST(VectorArithmeticTest, VaddVXUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b000000, 3, 1, 2, 1), 5);

    WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
    WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

    processor.Step();

    // Verify computed state.
    ExpectElements<uint32_t>(processor.registers_, 3, {15, 25, 35, 45});

    // Undo: should restore original destination.
    processor.Undo();
    ExpectElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

    // Redo: should reapply computed state.
    processor.Redo();
    ExpectElements<uint32_t>(processor.registers_, 3, {15, 25, 35, 45});
}

TEST(VectorArithmeticTest, VaddVXInvalidVtypeDoesNotExecute)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Load program with an unsupported configuration (vill = true).
    Kites::AssembledProgram program;
    // vsetvli with illegal LMUL = 2 sets vill.
    program.text_buffer = {
        (0xD1u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u,
        EncodeVectorVX(0b000000, 3, 1, 2, 1)
    };

    processor.LoadProgram(program);
    processor.registers_.WriteGpr(6, 10);
    processor.registers_.WriteGpr(2, 5);

    WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
    WriteElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});

    // Step vsetvli -> vill is set.
    processor.Step();
    uint64_t vtype = processor.registers_.ReadCsr(RegisterFile::CSR_VTYPE);
    EXPECT_NE(vtype & (uint64_t{1} << 63), 0);

    // Step vadd.vx -> must not execute and leave destination untouched.
    processor.Step();
    ExpectElements<uint32_t>(processor.registers_, 3, {99, 99, 99, 99});
}

// =========================================================================
// Batch 1 Additional Tests: vadd.vi, vrsub.vx/vi, vmin/vminu, vmax/vmaxu
// =========================================================================

TEST(VectorArithmeticTest, VaddVIBasicAndExtrema)
{
    // SEW = 8: 16 elements
    TestArithmeticVI<uint8_t>(0b000000, {10, 20, 250, 0}, 5, {15, 25, 255, 5});
    // Negative immediate -1 (wraps 0 to 255)
    TestArithmeticVI<uint8_t>(0b000000, {10, 20, 0, 1}, -1, {9, 19, 255, 0});
    // Negative boundary -16
    TestArithmeticVI<uint8_t>(0b000000, {16, 20, 0, 10}, -16, {0, 4, 240, 250});
    // Positive boundary +15
    TestArithmeticVI<uint8_t>(0b000000, {0, 10, 240, 241}, 15, {15, 25, 255, 0});

    // SEW = 16
    TestArithmeticVI<uint16_t>(0b000000, {1000, 2000, 65535}, -1, {999, 1999, 65534});

    // SEW = 32
    TestArithmeticVI<uint32_t>(0b000000, {100000, 0, 4294967295u}, 10, {100010, 10, 9});

    // SEW = 64
    TestArithmeticVI<uint64_t>(0b000000, {10000000000ULL, 0ULL}, -16, {10000000000ULL - 16, UINT64_MAX - 15});
}

TEST(VectorArithmeticTest, VrsubVXBasicAndOrder)
{
    // vrsub.vx: vd[i] = rs1 - vs2[i]
    // SEW = 8: rs1 = 30, vs2 = {10, 20, 30, 40} -> vd = {20, 10, 0, 246 (-10 mod 256)}
    TestArithmeticVX<uint8_t>(0b000011, {10, 20, 30, 40}, 30, {20, 10, 0, 246});

    // SEW = 16: rs1 = 100, vs2 = {50, 150} -> vd = {50, 65486 (-50 mod 65536)}
    TestArithmeticVX<uint16_t>(0b000011, {50, 150}, 100, {50, 65486});

    // SEW = 32
    TestArithmeticVX<uint32_t>(0b000011, {1000, 5000}, 2000, {1000, 4294964296u});

    // SEW = 64
    TestArithmeticVX<uint64_t>(0b000011, {100ULL, 500ULL}, 200ULL, {100ULL, UINT64_MAX - 299});
}

TEST(VectorArithmeticTest, VrsubVIBasicAndOrder)
{
    // vrsub.vi: vd[i] = simm5 - vs2[i]
    // SEW = 8: simm5 = 10, vs2 = {4, 10, 15, 0} -> vd = {6, 0, 251 (-5 mod 256), 10}
    TestArithmeticVI<uint8_t>(0b000011, {4, 10, 15, 0}, 10, {6, 0, 251, 10});

    // Negative immediate: simm5 = -1, vs2 = {0, 1, 255} -> vd = {255 (-1), 254 (-2), 0}
    TestArithmeticVI<uint8_t>(0b000011, {0, 1, 255}, -1, {255, 254, 0});

    // SEW = 32: simm5 = -16, vs2 = {0, 10} -> vd = {4294967280u (-16), 4294967270u (-26)}
    TestArithmeticVI<uint32_t>(0b000011, {0, 10}, -16, {4294967280u, 4294967270u});

    // SEW = 64: simm5 = 15, vs2 = {5ULL, 20ULL} -> vd = {10ULL, UINT64_MAX - 4}
    TestArithmeticVI<uint64_t>(0b000011, {5ULL, 20ULL}, 15, {10ULL, UINT64_MAX - 4});
}

TEST(VectorArithmeticTest, VminuVVAndVX)
{
    // Unsigned minimum: 0xFF (255) vs 0x01 (1) -> 0x01
    // SEW = 8: .vv
    TestArithmetic<uint8_t>(0b000100, {0xFF, 10, 0, 50}, {0x01, 20, 100, 50}, {0x01, 10, 0, 50});
    // SEW = 8: .vx with scalar = 10
    TestArithmeticVX<uint8_t>(0b000100, {0xFF, 5, 10, 20}, 10, {10, 5, 10, 10});

    // SEW = 16
    TestArithmetic<uint16_t>(0b000100, {0xFFFF, 500}, {1, 600}, {1, 500});
    TestArithmeticVX<uint16_t>(0b000100, {0xFFFF, 500}, 1000, {1000, 500});

    // SEW = 32
    TestArithmetic<uint32_t>(0b000100, {0xFFFFFFFFu, 100}, {2u, 50}, {2u, 50});
    TestArithmeticVX<uint32_t>(0b000100, {0xFFFFFFFFu, 100}, 500, {500, 100});

    // SEW = 64
    TestArithmetic<uint64_t>(0b000100, {UINT64_MAX, 1000ULL}, {5ULL, 2000ULL}, {5ULL, 1000ULL});
    TestArithmeticVX<uint64_t>(0b000100, {UINT64_MAX, 1000ULL}, 500ULL, {500ULL, 500ULL});
}

TEST(VectorArithmeticTest, VminVVAndVXSignedExtrema)
{
    // Signed minimum:
    // SEW = 8: 0xFF (-1) vs 0x01 (+1) -> 0xFF (-1)
    // 0x80 (-128, INT8_MIN) vs 0x7F (127, INT8_MAX) -> 0x80 (-128)
    TestArithmetic<uint8_t>(0b000101, {0xFF, 0x80, 0, 10}, {0x01, 0x7F, 0xFF, 5}, {0xFF, 0x80, 0xFF, 5});
    TestArithmeticVX<uint8_t>(0b000101, {0x01, 0x7F, 0x80, 0}, 0xFF, {0xFF, 0xFF, 0x80, 0xFF});

    // SEW = 16: INT16_MIN (0x8000) vs INT16_MAX (0x7FFF)
    TestArithmetic<uint16_t>(0b000101, {0xFFFF, 0x8000}, {0x0001, 0x7FFF}, {0xFFFF, 0x8000});
    TestArithmeticVX<uint16_t>(0b000101, {0x0001, 0x8000}, 0xFFFF, {0xFFFF, 0x8000});

    // SEW = 32: INT32_MIN (0x80000000) vs INT32_MAX (0x7FFFFFFF)
    TestArithmetic<uint32_t>(0b000101, {0xFFFFFFFFu, 0x80000000u}, {1u, 0x7FFFFFFFu}, {0xFFFFFFFFu, 0x80000000u});
    TestArithmeticVX<uint32_t>(0b000101, {1u, 0x7FFFFFFFu}, 0xFFFFFFFFu, {0xFFFFFFFFu, 0xFFFFFFFFu});

    // SEW = 64: INT64_MIN vs INT64_MAX
    TestArithmetic<uint64_t>(0b000101, {UINT64_MAX, 0x8000000000000000ULL}, {1ULL, 0x7FFFFFFFFFFFFFFFULL}, {UINT64_MAX, 0x8000000000000000ULL});
    TestArithmeticVX<uint64_t>(0b000101, {1ULL, 0x7FFFFFFFFFFFFFFFULL}, UINT64_MAX, {UINT64_MAX, UINT64_MAX});
}

TEST(VectorArithmeticTest, VmaxuVVAndVX)
{
    // Unsigned maximum: 0xFF (255) vs 0x01 (1) -> 0xFF (255)
    TestArithmetic<uint8_t>(0b000110, {0xFF, 10, 0, 50}, {0x01, 20, 100, 50}, {0xFF, 20, 100, 50});
    TestArithmeticVX<uint8_t>(0b000110, {0xFF, 5, 10, 20}, 10, {0xFF, 10, 10, 20});

    // SEW = 16
    TestArithmetic<uint16_t>(0b000110, {0xFFFF, 500}, {1, 600}, {0xFFFF, 600});
    TestArithmeticVX<uint16_t>(0b000110, {0xFFFF, 500}, 1000, {0xFFFF, 1000});

    // SEW = 32
    TestArithmetic<uint32_t>(0b000110, {0xFFFFFFFFu, 100}, {2u, 50}, {0xFFFFFFFFu, 100});
    TestArithmeticVX<uint32_t>(0b000110, {0xFFFFFFFFu, 100}, 500, {0xFFFFFFFFu, 500});

    // SEW = 64
    TestArithmetic<uint64_t>(0b000110, {UINT64_MAX, 1000ULL}, {5ULL, 2000ULL}, {UINT64_MAX, 2000ULL});
    TestArithmeticVX<uint64_t>(0b000110, {UINT64_MAX, 1000ULL}, 500ULL, {UINT64_MAX, 1000ULL});
}

TEST(VectorArithmeticTest, VmaxVVAndVXSignedExtrema)
{
    // Signed maximum:
    // SEW = 8: 0xFF (-1) vs 0x01 (+1) -> 0x01 (+1)
    // 0x80 (-128) vs 0x7F (127) -> 0x7F (127)
    TestArithmetic<uint8_t>(0b000111, {0xFF, 0x80, 0, 10}, {0x01, 0x7F, 0xFF, 5}, {0x01, 0x7F, 0, 10});
    TestArithmeticVX<uint8_t>(0b000111, {0x01, 0x7F, 0x80, 0xFF}, 0, {0x01, 0x7F, 0, 0});

    // SEW = 16: INT16_MIN (0x8000) vs INT16_MAX (0x7FFF)
    TestArithmetic<uint16_t>(0b000111, {0xFFFF, 0x8000}, {0x0001, 0x7FFF}, {0x0001, 0x7FFF});
    TestArithmeticVX<uint16_t>(0b000111, {0x8000, 0xFFFF}, 0x7FFF, {0x7FFF, 0x7FFF});

    // SEW = 32
    TestArithmetic<uint32_t>(0b000111, {0xFFFFFFFFu, 0x80000000u}, {1u, 0x7FFFFFFFu}, {1u, 0x7FFFFFFFu});
    TestArithmeticVX<uint32_t>(0b000111, {0x80000000u, 0xFFFFFFFFu}, 0, {0, 0});

    // SEW = 64
    TestArithmetic<uint64_t>(0b000111, {UINT64_MAX, 0x8000000000000000ULL}, {1ULL, 0x7FFFFFFFFFFFFFFFULL}, {1ULL, 0x7FFFFFFFFFFFFFFFULL});
    TestArithmeticVX<uint64_t>(0b000111, {0x8000000000000000ULL, UINT64_MAX}, 10ULL, {10ULL, 10ULL});
}

TEST(VectorArithmeticTest, MinMaxScalarTruncation)
{
    // High bits outside active SEW in GPR must be ignored:
    // rs1 = 0x11223344556677FF
    // SEW = 8: lowest 8 bits is 0xFF (-1 signed, 255 unsigned)
    // vmin.vx: min(1, -1) = -1 (0xFF)
    TestArithmeticVX<uint8_t>(0b000101, {1}, 0x11223344556677FFULL, {0xFF});
    // vminu.vx: min(1, 255) = 1 (0x01)
    TestArithmeticVX<uint8_t>(0b000100, {1}, 0x11223344556677FFULL, {1});

    // SEW = 16: rs1 = 0xAABBCCDD00000005 -> scalar = 5
    TestArithmeticVX<uint16_t>(0b000101, {10}, 0xAABBCCDD00000005ULL, {5});
    TestArithmeticVX<uint16_t>(0b000110, {10}, 0xAABBCCDD00000005ULL, {10});
}

TEST(VectorArithmeticTest, VstartEqualToVL)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // SEW = 32, AVL = 2, vstart = 2 (vstart == vl)
    LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b000000, 3, 1, 5, 1));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

    WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
    WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

    processor.Step();

    // Elements 0 and 1 (prestart/active region with 0 active items) remain undisturbed (77, 88)
    // Tail undisturbed elements remain 99, 111
    ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

    // vstart must be reset to 0 after completion
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

TEST(VectorArithmeticTest, VstartGreaterThanVLAborts)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // SEW = 32, AVL = 2, vstart = 3 (vstart > vl)
    LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b000000, 3, 1, 5, 1));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

    WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
    WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

    processor.Step();

    // Aborts early: destination register untouched
    ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

    // CSR vstart preserved (not reset)
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 3u);
}

TEST(VectorArithmeticTest, Batch1MaskingAndPolicies)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vadd.vi with vm = 0 (masked), vta = true (tail-agnostic), vma = false (mask-undisturbed)
    LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b000000, 3, 1, 10, 0), true, false);

    // Mask in v0: bit 0 = 1 (active), bit 1 = 0 (masked-off)
    processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00000001);

    WriteElements<uint32_t>(processor.registers_, 1, {5, 5, 5, 5});
    WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

    processor.Step();

    // Element 0: active -> 5 + 10 = 15
    // Element 1: masked-off, mask-undisturbed -> retains 200
    // Elements 2, 3: tail-agnostic -> deterministic 0xFFFFFFFF
    ExpectElements<uint32_t>(processor.registers_, 3, {15, 200, 0xFFFFFFFFu, 0xFFFFFFFFu});
}

TEST(VectorArithmeticTest, Batch1AliasingAndHazardSafety)
{
    // Test 1: vd == vs2 with vrsub.vx
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vrsub.vx v1, v1, x2 (vd = 1, vs2 = 1, rs1 = 2)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b000011, 1, 1, 2, 1), 50);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});

        processor.Step();

        // 50 - 10 = 40, 50 - 20 = 30
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 0), 40u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 1), 30u);
    }

    // Test 2: vd == vs1 with vmin.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmin.vv v2, v1, v2 (vd = 2, vs2 = 1, vs1 = 2)
        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b000101, 2, 1, 2, 1));
        WriteElements<uint32_t>(processor.registers_, 1, {10, 50});
        WriteElements<uint32_t>(processor.registers_, 2, {30, 20});

        processor.Step();

        // min(10, 30) = 10, min(50, 20) = 20
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 0), 10u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 1), 20u);
    }

    // Test 3: vd == v0 with vm = 0 (masked execution)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vadd.vi v0, v1, 1, v0.t (vd = 0, vs2 = 1, simm5 = 1, vm = 0)
        LoadVectorVIProgram(processor, 32, 4, EncodeVectorVI(0b000000, 0, 1, 1, 0), false, false);

        // Initial mask in v0: bit 0=1, bit 1=1, bit 2=0, bit 3=1 -> 0b00001011 = 0x0B
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0B);

        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        // Preserved destination in v0 (bytes 4..15):
        processor.registers_.WriteVregElement<uint32_t>(0, 1, 100);
        processor.registers_.WriteVregElement<uint32_t>(0, 2, 200);
        processor.registers_.WriteVregElement<uint32_t>(0, 3, 300);

        processor.Step();

        // Element 0: active (mask bit 0 was 1) -> 10 + 1 = 11
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 0), 11u);
        // Element 1: active (mask bit 1 was 1) -> 20 + 1 = 21
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 1), 21u);
        // Element 2: inactive (mask bit 2 was 0) -> mask-undisturbed retains 200
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 2), 200u);
        // Element 3: active (mask bit 3 was 1) -> 40 + 1 = 41
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 3), 41u);
    }
}

TEST(VectorArithmeticTest, Batch1UndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vrsub.vi v3, v1, 10
    LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b000011, 3, 1, 10, 1));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);

    WriteElements<uint32_t>(processor.registers_, 1, {4, 6});
    WriteElements<uint32_t>(processor.registers_, 3, {100, 200});

    // Step vrsub.vi -> element 0 prestart preserved (100), element 1 -> 10 - 6 = 4. vstart reset to 0.
    processor.Step();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 4u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

    // Undo -> restores element 1 to 200, vstart to 1
    processor.Undo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 200u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1u);

    // Redo -> restores element 1 to 4, vstart to 0
    processor.Redo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 4u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}
