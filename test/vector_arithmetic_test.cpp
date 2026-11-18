
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

bool ReadMaskBit(const RegisterFile &registers, unsigned int reg, unsigned int index)
{
    const uint8_t byte = registers.ReadVregElement<uint8_t>(reg, index / 8);
    return ((byte >> (index % 8)) & 1) != 0;
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
    uint32_t vm = 1,
    uint32_t funct3 = 0b000)
{
    return (funct6 << 26) |
           (vm << 25) |
           (vs2 << 20) |
           (vs1 << 15) |
           (funct3 << 12) |
           (vd << 7) |
           0x57u;
}

uint32_t EncodeVectorVX(
    uint32_t funct6,
    uint32_t vd = 3,
    uint32_t vs2 = 1,
    uint32_t rs1 = 2,
    uint32_t vm = 1,
    uint32_t funct3 = 0b100)
{
    return (funct6 << 26) |
           (vm << 25) |
           (vs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
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

template <typename T>
void TestMultiplyVV(
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
        EncodeVectorVV(funct6, 3, 1, 2, 1, 0b010)
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

template <typename T>
void TestMultiplyVX(
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
        EncodeVectorVX(funct6, 3, 1, 2, 1, 0b110),
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

    // With vstart >= vl, no elements are executed: destination register untouched
    ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

    // Successfully completed instruction resets CSR vstart to 0
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
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

TEST(VectorArithmeticTest, VsllBasicAllSEWAndHighBits)
{
    // SEW = 8 (funct6 = 0b100101)
    TestArithmetic<uint8_t>(0b100101, {0x01, 0x03, 0x80, 0x0F}, {0, 1, 1, 4}, {0x01, 0x06, 0x00, 0xF0});
    // SEW = 8: shift amount low log2(SEW) bits -> 0x83 & 7 = 3, 0xF0 & 7 = 0
    TestArithmetic<uint8_t>(0b100101, {0x01, 0x07}, {0x83, 0xF0}, {0x08, 0x07});

    // SEW = 16
    TestArithmetic<uint16_t>(0b100101, {0x0001, 0x00FF, 0x8000, 0x000F}, {0, 8, 1, 12}, {0x0001, 0xFF00, 0x0000, 0xF000});

    // SEW = 32
    TestArithmetic<uint32_t>(0b100101, {0x00000001, 0x0000FFFF, 0x80000000, 0x00000001}, {0, 16, 1, 31}, {0x00000001, 0xFFFF0000, 0x00000000, 0x80000000});

    // SEW = 64
    TestArithmetic<uint64_t>(0b100101, {0x0000000000000001ULL, 0x8000000000000000ULL}, {32, 1}, {0x0000000100000000ULL, 0x0000000000000000ULL});
}

TEST(VectorArithmeticTest, VsllVXAndVI)
{
    // vsll.vx SEW = 8
    TestArithmeticVX<uint8_t>(0b100101, {0x01, 0x02, 0x03, 0x04}, 2, {0x04, 0x08, 0x0C, 0x10});
    // vsll.vx with scalar operand high bits set (0xFFFFFFFFFFFFFF02 on SEW 8 -> shift distance 2)
    TestArithmeticVX<uint8_t>(0b100101, {0x01, 0x02, 0x03, 0x04}, 0xFFFFFFFFFFFFFF02ULL, {0x04, 0x08, 0x0C, 0x10});

    // vsll.vx SEW = 16
    TestArithmeticVX<uint16_t>(0b100101, {0x0001, 0x0002, 0x0003, 0x0004}, 4, {0x0010, 0x0020, 0x0030, 0x0040});

    // vsll.vx SEW = 32
    TestArithmeticVX<uint32_t>(0b100101, {0x00000001, 0x00000002}, 4, {0x00000010, 0x00000020});

    // vsll.vx SEW = 64
    TestArithmeticVX<uint64_t>(0b100101, {0x0000000000000001ULL, 0x0000000000000002ULL}, 32, {0x0000000100000000ULL, 0x0000000200000000ULL});

    // vsll.vi SEW = 8: shift by 3
    TestArithmeticVI<uint8_t>(0b100101, {0x01, 0x02, 0x03, 0x04}, 3, {0x08, 0x10, 0x18, 0x20});
    // vsll.vi SEW = 8: shift by 0
    TestArithmeticVI<uint8_t>(0b100101, {0x55, 0xAA}, 0, {0x55, 0xAA});
    // vsll.vi SEW = 8: uimm5 = 31 -> 31 & 7 = 7
    TestArithmeticVI<uint8_t>(0b100101, {0x01, 0x03}, 31, {0x80, 0x80});

    // vsll.vi SEW = 16: shift by 8
    TestArithmeticVI<uint16_t>(0b100101, {0x0001, 0x0003}, 8, {0x0100, 0x0300});

    // vsll.vi SEW = 32: shift by 16
    TestArithmeticVI<uint32_t>(0b100101, {0x00000001, 0x00000003}, 16, {0x00010000, 0x00030000});
    // vsll.vi SEW = 32: uimm5 = 31
    TestArithmeticVI<uint32_t>(0b100101, {0x00000001, 0x00000002}, 31, {0x80000000, 0x00000000});

    // vsll.vi SEW = 64: uimm5 = 31
    TestArithmeticVI<uint64_t>(0b100101, {0x0000000000000001ULL, 0x0000000000000003ULL}, 31, {0x0000000080000000ULL, 0x0000000180000000ULL});
}

TEST(VectorArithmeticTest, VsrlBasicAllSEWAndHighBits)
{
    // SEW = 8 (funct6 = 0b101000)
    TestArithmetic<uint8_t>(0b101000, {0x80, 0xFF, 0x01, 0xAA}, {1, 4, 1, 0}, {0x40, 0x0F, 0x00, 0xAA});
    // SEW = 8: shift amount low log2(SEW) bits -> 0x81 & 7 = 1
    TestArithmetic<uint8_t>(0b101000, {0x80, 0x08}, {0x81, 0x00}, {0x40, 0x08});

    // SEW = 16
    TestArithmetic<uint16_t>(0b101000, {0x8000, 0xFFFF, 0x0001}, {1, 8, 0}, {0x4000, 0x00FF, 0x0001});

    // SEW = 32
    TestArithmetic<uint32_t>(0b101000, {0x80000000, 0xFFFFFFFF, 0x12345678}, {1, 16, 4}, {0x40000000, 0x0000FFFF, 0x01234567});

    // SEW = 64
    TestArithmetic<uint64_t>(0b101000, {0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL}, {1, 32}, {0x4000000000000000ULL, 0x00000000FFFFFFFFULL});
}

TEST(VectorArithmeticTest, VsrlVXAndVI)
{
    // vsrl.vx SEW = 8
    TestArithmeticVX<uint8_t>(0b101000, {0x40, 0x80, 0x10, 0x20}, 2, {0x10, 0x20, 0x04, 0x08});
    // vsrl.vx scalar with high bits set
    TestArithmeticVX<uint8_t>(0b101000, {0x40, 0x80, 0x10, 0x20}, 0xFFFFFFFFFFFFFF02ULL, {0x10, 0x20, 0x04, 0x08});

    // vsrl.vx SEW = 16
    TestArithmeticVX<uint16_t>(0b101000, {0x0100, 0x0200, 0x0300, 0x0400}, 4, {0x0010, 0x0020, 0x0030, 0x0040});

    // vsrl.vx SEW = 32
    TestArithmeticVX<uint32_t>(0b101000, {0x12345678, 0x80000000}, 4, {0x01234567, 0x08000000});

    // vsrl.vx SEW = 64
    TestArithmeticVX<uint64_t>(0b101000, {0x0000000100000000ULL, 0x0000000200000000ULL}, 32, {0x0000000000000001ULL, 0x0000000000000002ULL});

    // vsrl.vi SEW = 8: shift by 2
    TestArithmeticVI<uint8_t>(0b101000, {0x40, 0x80, 0x10, 0x20}, 2, {0x10, 0x20, 0x04, 0x08});
    // vsrl.vi SEW = 8: shift by 0
    TestArithmeticVI<uint8_t>(0b101000, {0x40, 0x80}, 0, {0x40, 0x80});
    // vsrl.vi SEW = 8: uimm5 = 31 -> 31 & 7 = 7
    TestArithmeticVI<uint8_t>(0b101000, {0x80, 0xFF}, 31, {0x01, 0x01});

    // vsrl.vi SEW = 16: shift by 4
    TestArithmeticVI<uint16_t>(0b101000, {0x1234, 0x8000}, 4, {0x0123, 0x0800});

    // vsrl.vi SEW = 32: shift by 8
    TestArithmeticVI<uint32_t>(0b101000, {0x12345678, 0x80000000}, 8, {0x00123456, 0x00800000});
    // vsrl.vi SEW = 32: uimm5 = 31
    TestArithmeticVI<uint32_t>(0b101000, {0x80000000, 0xFFFFFFFF}, 31, {0x00000001, 0x00000001});

    // vsrl.vi SEW = 64: shift by 16
    TestArithmeticVI<uint64_t>(0b101000, {0x1234567800000000ULL, 0x8000000000000000ULL}, 16, {0x0000123456780000ULL, 0x0000800000000000ULL});
}

TEST(VectorArithmeticTest, VsraBasicAllSEWAndSignPropagation)
{
    // SEW = 8 (funct6 = 0b101001)
    // 0x40 (positive) >> 1 = 0x20
    // 0x80 (-128) >> 1 = 0xC0 (-64)
    // 0x80 (-128) >> 7 = 0xFF (-1)
    // 0xFF (-1) >> 3 = 0xFF (-1)
    TestArithmetic<uint8_t>(0b101001, {0x40, 0x80, 0x80, 0xFF}, {1, 1, 7, 3}, {0x20, 0xC0, 0xFF, 0xFF});
    // SEW = 8: shift amount low log2(SEW) bits -> 0x81 & 7 = 1
    TestArithmetic<uint8_t>(0b101001, {0x80, 0x40}, {0x81, 0xF0}, {0xC0, 0x40});

    // SEW = 16
    // 0x8000 (-32768) >> 1 = 0xC000 (-16384)
    // 0x8000 >> 15 = 0xFFFF (-1)
    // 0x7FFF (+32767) >> 1 = 0x3FFF
    TestArithmetic<uint16_t>(0b101001, {0x8000, 0x8000, 0x7FFF}, {1, 15, 1}, {0xC000, 0xFFFF, 0x3FFF});

    // SEW = 32
    // 0x80000000 >> 1 = 0xC0000000
    // 0x80000000 >> 31 = 0xFFFFFFFF
    // 0x7FFFFFFF >> 1 = 0x3FFFFFFF
    TestArithmetic<uint32_t>(0b101001, {0x80000000, 0x80000000, 0x7FFFFFFF}, {1, 31, 1}, {0xC0000000, 0xFFFFFFFF, 0x3FFFFFFF});

    // SEW = 64
    // 0x8000000000000000ULL >> 1 = 0xC000000000000000ULL
    // 0x8000000000000000ULL >> 63 = 0xFFFFFFFFFFFFFFFFULL
    TestArithmetic<uint64_t>(0b101001, {0x8000000000000000ULL, 0x8000000000000000ULL}, {1, 63}, {0xC000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL});
}

TEST(VectorArithmeticTest, VsraVXAndVI)
{
    // vsra.vx SEW = 8: shift by 1 with sign propagation
    TestArithmeticVX<uint8_t>(0b101001, {0x80, 0x40, 0xFE, 0x02}, 1, {0xC0, 0x20, 0xFF, 0x01});
    // vsra.vx scalar with high bits set (0xFFFFFFFFFFFFFF01 -> shift distance 1)
    TestArithmeticVX<uint8_t>(0b101001, {0x80, 0x40}, 0xFFFFFFFFFFFFFF01ULL, {0xC0, 0x20});

    // vsra.vx SEW = 16: shift by 2
    TestArithmeticVX<uint16_t>(0b101001, {0x8000, 0x4000}, 2, {0xE000, 0x1000});

    // vsra.vx SEW = 32: shift by 2
    TestArithmeticVX<uint32_t>(0b101001, {0x80000000, 0x40000000}, 2, {0xE0000000, 0x10000000});

    // vsra.vx SEW = 64: shift by 2
    TestArithmeticVX<uint64_t>(0b101001, {0x8000000000000000ULL, 0x4000000000000000ULL}, 2, {0xE000000000000000ULL, 0x1000000000000000ULL});

    // vsra.vi SEW = 8: shift by 1
    TestArithmeticVI<uint8_t>(0b101001, {0x80, 0x40, 0xFE, 0x02}, 1, {0xC0, 0x20, 0xFF, 0x01});
    // vsra.vi SEW = 8: shift by 0
    TestArithmeticVI<uint8_t>(0b101001, {0x80, 0x7F}, 0, {0x80, 0x7F});
    // vsra.vi SEW = 8: uimm5 = 31 -> 31 & 7 = 7 -> all sign bits
    TestArithmeticVI<uint8_t>(0b101001, {0x80, 0x7F}, 31, {0xFF, 0x00});

    // vsra.vi SEW = 16: shift by 4
    TestArithmeticVI<uint16_t>(0b101001, {0x8000, 0x7FFF}, 4, {0xF800, 0x07FF});

    // vsra.vi SEW = 32: shift by 4
    TestArithmeticVI<uint32_t>(0b101001, {0xF0000000, 0x10000000}, 4, {0xFF000000, 0x01000000});
    // vsra.vi SEW = 32: uimm5 = 31
    TestArithmeticVI<uint32_t>(0b101001, {0x80000000, 0x7FFFFFFF}, 31, {0xFFFFFFFF, 0x00000000});

    // vsra.vi SEW = 64: shift by 4
    TestArithmeticVI<uint64_t>(0b101001, {0x8000000000000000ULL, 0x7000000000000000ULL}, 4, {0xF800000000000000ULL, 0x0700000000000000ULL});
}

TEST(VectorArithmeticTest, VectorShiftMaskingAndPolicies)
{
    // Test 1: vsll.vi masked with mu (mask-undisturbed)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsll.vi v3, v1, 2, v0.t (vm = 0, ta = false, ma = false)
        LoadVectorVIProgram(processor, 32, 4, EncodeVectorVI(0b100101, 3, 1, 2, 0), false, false);

        // Mask in v0: bit 0 = 1, bit 1 = 0, bit 2 = 1, bit 3 = 0 -> 0b00000101
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00000101);

        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Element 0: active -> 1 << 2 = 4
        // Element 1: masked-off, mu -> retains 200
        // Element 2: active -> 3 << 2 = 12
        // Element 3: masked-off, mu -> retains 400
        ExpectElements<uint32_t>(processor.registers_, 3, {4, 200, 12, 400});
    }

    // Test 2: vsra.vx masked with ma (mask-agnostic, inactive elements filled with 1s)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsra.vx v3, v1, x2, v0.t (vm = 0, ta = false, ma = true)
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b101001, 3, 1, 2, 0), 1, 2, false, true);

        // Mask in v0: bit 0 = 0, bit 1 = 1, bit 2 = 0, bit 3 = 1 -> 0b00001010
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00001010);

        WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x80000000, 0x40, 0x40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Element 0: inactive, ma -> 0xFFFFFFFF
        // Element 1: active -> 0x80000000 >> 1 = 0xC0000000
        // Element 2: inactive, ma -> 0xFFFFFFFF
        // Element 3: active -> 0x40 >> 1 = 0x20
        ExpectElements<uint32_t>(processor.registers_, 3, {0xFFFFFFFFu, 0xC0000000u, 0xFFFFFFFFu, 0x00000020u});
    }
}

TEST(VectorArithmeticTest, VectorShiftTailPolicy)
{
    // Test 1: tail-agnostic (ta = true) for vsrl.vi
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsrl.vi v3, v1, 1 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = true (tail-agnostic)
        LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b101000, 3, 1, 1, 1), true, false);

        WriteElements<uint32_t>(processor.registers_, 1, {0x10, 0x20, 0x30, 0x40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1 active: 0x10 >> 1 = 0x08, 0x20 >> 1 = 0x10
        // Tail elements 2, 3: ta -> 0xFFFFFFFF
        ExpectElements<uint32_t>(processor.registers_, 3, {0x08, 0x10, 0xFFFFFFFFu, 0xFFFFFFFFu});
    }

    // Test 2: tail-undisturbed (ta = false) for vsll.vx -> inactive tail elements retain original destination values
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsll.vx v3, v1, x2 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = false (tail-undisturbed)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100101, 3, 1, 2, 1), 2, 2, false, false);

        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1 active: 10 << 2 = 40, 20 << 2 = 80
        // Tail elements 2, 3: undisturbed -> retain 300, 400
        ExpectElements<uint32_t>(processor.registers_, 3, {40, 80, 300, 400});
    }
}

TEST(VectorArithmeticTest, VectorShiftVstart)
{
    // Test 1: 0 < vstart < vl with vsll.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsll.vv v3, v1, v2 (vl = 4)
        LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b100101, 3, 1, 2, 1));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 2, {1, 1, 1, 1});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1: prestart undisturbed -> retain 100, 200
        // Elements 2, 3: active -> 3 << 1 = 6, 4 << 1 = 8
        // vstart reset to 0
        ExpectElements<uint32_t>(processor.registers_, 3, {100, 200, 6, 8});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 2: vstart == vl with vsrl.vi -> no active elements computed, destination undisturbed, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b101000, 3, 1, 1, 1));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {0x10, 0x20, 0x30, 0x40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 3: vstart > vl with vsra.vx -> executes no elements, destination unchanged, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b101001, 3, 1, 2, 1), 2);
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

        WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x40000000});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, VectorShiftAliasingAndHazardSafety)
{
    // Test 1: vd == vs2 (in-place shift)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsra.vi v1, v1, 2
        LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b101001, 1, 1, 2, 1));
        WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x20});

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 0), 0xE0000000u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 1), 0x08u);
    }

    // Test 2: vd == vs1 with vsll.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsll.vv v2, v1, v2 (vd = 2, vs2 = 1, vs1 = 2)
        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100101, 2, 1, 2, 1));
        WriteElements<uint32_t>(processor.registers_, 1, {0x01, 0x02});
        WriteElements<uint32_t>(processor.registers_, 2, {3, 4});

        processor.Step();

        // 1 << 3 = 8, 2 << 4 = 32
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 0), 8u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 1), 32u);
    }

    // Test 3: vd == v0 with vm = 0 (destination is mask register)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vsrl.vi v0, v1, 1, v0.t (vd = 0, vs2 = 1, uimm5 = 1, vm = 0)
        LoadVectorVIProgram(processor, 32, 4, EncodeVectorVI(0b101000, 0, 1, 1, 0), false, false);

        // Initial mask in v0: bit 0=1, bit 1=1, bit 2=0, bit 3=1 -> 0x0B
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0B);

        WriteElements<uint32_t>(processor.registers_, 1, {0x10, 0x20, 0x30, 0x40});
        processor.registers_.WriteVregElement<uint32_t>(0, 1, 100);
        processor.registers_.WriteVregElement<uint32_t>(0, 2, 200);
        processor.registers_.WriteVregElement<uint32_t>(0, 3, 300);

        processor.Step();

        // Element 0: active -> 0x10 >> 1 = 0x08
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 0), 0x08u);
        // Element 1: active -> 0x20 >> 1 = 0x10
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 1), 0x10u);
        // Element 2: inactive (mask bit 2 was 0) -> retains 200
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 2), 200u);
        // Element 3: active -> 0x40 >> 1 = 0x20
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 3), 0x20u);
    }
}

TEST(VectorArithmeticTest, VectorShiftUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsra.vi v3, v1, 2
    LoadVectorVIProgram(processor, 32, 2, EncodeVectorVI(0b101001, 3, 1, 2, 1));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);

    WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x80000000});
    WriteElements<uint32_t>(processor.registers_, 3, {100, 200});

    // Step vsra.vi: element 0 prestart preserved (100), element 1 -> 0x80000000 >> 2 = 0xE0000000
    processor.Step();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 0xE0000000u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

    // Undo -> restores element 1 to 200, vstart to 1
    processor.Undo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 200u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1u);

    // Redo -> restores element 1 to 0xE0000000, vstart to 0
    processor.Redo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 0xE0000000u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

// ---------------------------------------------------------
// Batch 4: Vector Integer Multiplication Tests
// ---------------------------------------------------------

TEST(VectorArithmeticTest, VmulBasicAllSEW)
{
    // vmul.vv (funct6 = 0b100101, funct3 = 0b010)
    // SEW = 8
    TestMultiplyVV<uint8_t>(0b100101, {2, 10, 255, 128}, {3, 20, 2, 2}, {6, 200, 254, 0});
    // SEW = 16
    TestMultiplyVV<uint16_t>(0b100101, {2, 300, 0xFFFF, 0x8000}, {5, 100, 3, 2}, {10, 30000, 0xFFFD, 0x0000});
    // SEW = 32
    TestMultiplyVV<uint32_t>(0b100101, {3, 10000, 0xFFFFFFFF, 0x80000000}, {7, 5000, 2, 2}, {21, 50000000, 0xFFFFFFFE, 0x00000000});
    // SEW = 64
    TestMultiplyVV<uint64_t>(0b100101, {10, 0xFFFFFFFFFFFFFFFFULL}, {20, 3}, {200, 0xFFFFFFFFFFFFFFFDULL});

    // vmul.vx (funct6 = 0b100101, funct3 = 0b110)
    TestMultiplyVX<uint8_t>(0b100101, {1, 2, 3, 4}, 10, {10, 20, 30, 40});
    // SEW = 8: scalar with high bits set (0xFFFFFFFFFFFFFF03ULL -> 3)
    TestMultiplyVX<uint8_t>(0b100101, {1, 2, 3, 4}, 0xFFFFFFFFFFFFFF03ULL, {3, 6, 9, 12});
    TestMultiplyVX<uint16_t>(0b100101, {10, 20}, 5, {50, 100});
    TestMultiplyVX<uint32_t>(0b100101, {100, 200}, 3, {300, 600});
    TestMultiplyVX<uint64_t>(0b100101, {1000, 2000}, 4, {4000, 8000});
}

TEST(VectorArithmeticTest, VmulhuBasicAllSEW)
{
    // vmulhu.vv (funct6 = 0b100100, funct3 = 0b010)
    // SEW = 8: 255 * 255 = 65025 = 0xFE01 -> high 0xFE (254)
    //          200 * 200 = 40000 = 0x9C40 -> high 0x9C (156)
    //          128 * 2 = 256 = 0x0100 -> high 1
    //          2 * 3 = 6 -> high 0
    TestMultiplyVV<uint8_t>(0b100100, {255, 200, 128, 2}, {255, 200, 2, 3}, {254, 156, 1, 0});

    // SEW = 16: 0xFFFF * 0xFFFF = 0xFFFE0001 -> high 0xFFFE
    //           0x8000 * 2 = 0x00010000 -> high 1
    TestMultiplyVV<uint16_t>(0b100100, {0xFFFF, 0x8000}, {0xFFFF, 2}, {0xFFFE, 1});

    // SEW = 32: 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001 -> high 0xFFFFFFFE
    TestMultiplyVV<uint32_t>(0b100100, {0xFFFFFFFF, 0x80000000}, {0xFFFFFFFF, 2}, {0xFFFFFFFE, 1});

    // SEW = 64: 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF -> high 0xFFFFFFFFFFFFFFFE
    TestMultiplyVV<uint64_t>(0b100100, {0xFFFFFFFFFFFFFFFFULL, 0x8000000000000000ULL}, {0xFFFFFFFFFFFFFFFFULL, 2}, {0xFFFFFFFFFFFFFFFEULL, 1});

    // vmulhu.vx (funct6 = 0b100100, funct3 = 0b110)
    TestMultiplyVX<uint8_t>(0b100100, {255, 200}, 255, {254, 199});
    TestMultiplyVX<uint16_t>(0b100100, {0xFFFF, 0x8000}, 0xFFFF, {0xFFFE, 0x7FFF});
    TestMultiplyVX<uint32_t>(0b100100, {0xFFFFFFFF, 0x80000000}, 0xFFFFFFFF, {0xFFFFFFFE, 0x7FFFFFFF});
    TestMultiplyVX<uint64_t>(0b100100, {0xFFFFFFFFFFFFFFFFULL, 0x8000000000000000ULL}, 0xFFFFFFFFFFFFFFFFULL, {0xFFFFFFFFFFFFFFFEULL, 0x7FFFFFFFFFFFFFFFULL});
}

TEST(VectorArithmeticTest, VmulhBasicAllSEW)
{
    // vmulh.vv (funct6 = 0b100111, funct3 = 0b010)
    // SEW = 8: (-1) * (-1) = 1 -> high 0
    //          (-128) * (-128) = 16384 = 0x4000 -> high 0x40 (64)
    //          (-1) * 2 = -2 = 0xFFFE -> high 0xFF (-1)
    //          64 * 4 = 256 = 0x0100 -> high 1
    TestMultiplyVV<uint8_t>(0b100111, {0xFF, 0x80, 0xFF, 64}, {0xFF, 0x80, 2, 4}, {0, 0x40, 0xFF, 1});

    // SEW = 16: (-32768) * (-32768) = 2^30 = 0x40000000 -> high 0x4000
    //           (-1) * 2 = -2 -> high 0xFFFF
    TestMultiplyVV<uint16_t>(0b100111, {0x8000, 0xFFFF}, {0x8000, 2}, {0x4000, 0xFFFF});

    // SEW = 32: INT32_MIN * INT32_MIN -> high 0x40000000
    //           (-1) * 5 -> high 0xFFFFFFFF
    TestMultiplyVV<uint32_t>(0b100111, {0x80000000, 0xFFFFFFFF}, {0x80000000, 5}, {0x40000000, 0xFFFFFFFF});

    // SEW = 64: INT64_MIN * INT64_MIN -> high 0x4000000000000000
    //           (-1) * 10 -> high 0xFFFFFFFFFFFFFFFF
    TestMultiplyVV<uint64_t>(0b100111, {0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL}, {0x8000000000000000ULL, 10}, {0x4000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL});

    // vmulh.vx (funct6 = 0b100111, funct3 = 0b110)
    TestMultiplyVX<uint8_t>(0b100111, {0x80, 0xFF}, 0x80, {0x40, 0});
    TestMultiplyVX<uint16_t>(0b100111, {0x8000, 0xFFFF}, 0x8000, {0x4000, 0});
    TestMultiplyVX<uint32_t>(0b100111, {0x80000000, 0xFFFFFFFF}, 0x80000000, {0x40000000, 0});
    TestMultiplyVX<uint64_t>(0b100111, {0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL}, 0x8000000000000000ULL, {0x4000000000000000ULL, 0});
}

TEST(VectorArithmeticTest, VmulhsuBasicAllSEWAndAsymmetry)
{
    // vmulhsu: high_half(signed(vs2) * unsigned(vs1))
    // funct6 = 0b100110, funct3 = 0b010 (.vv), 0b110 (.vx)

    // SEW = 8:
    // Case 1: vs2 = -1 (0xFF), vs1 = 2 -> (-1) * 2 = -2 -> high 0xFF
    // Case 2: vs2 = 2, vs1 = 255 (0xFF) -> 2 * 255 = 510 = 0x01FE -> high 1
    TestMultiplyVV<uint8_t>(0b100110, {0xFF, 2}, {2, 0xFF}, {0xFF, 1});
    TestMultiplyVX<uint8_t>(0b100110, {0xFF}, 2, {0xFF});
    TestMultiplyVX<uint8_t>(0b100110, {2}, 0xFF, {1});

    // SEW = 16:
    // Case 1: vs2 = -1 (0xFFFF), vs1 = 2 -> high 0xFFFF
    // Case 2: vs2 = 2, vs1 = 0xFFFF (65535) -> 2 * 65535 = 131070 -> high 1
    TestMultiplyVV<uint16_t>(0b100110, {0xFFFF, 2}, {2, 0xFFFF}, {0xFFFF, 1});
    TestMultiplyVX<uint16_t>(0b100110, {0xFFFF}, 2, {0xFFFF});
    TestMultiplyVX<uint16_t>(0b100110, {2}, 0xFFFF, {1});

    // SEW = 32:
    // Case 1: vs2 = -1, vs1 = 2 -> high 0xFFFFFFFF
    // Case 2: vs2 = 2, vs1 = 0xFFFFFFFF -> high 1
    TestMultiplyVV<uint32_t>(0b100110, {0xFFFFFFFF, 2}, {2, 0xFFFFFFFF}, {0xFFFFFFFF, 1});
    TestMultiplyVX<uint32_t>(0b100110, {0xFFFFFFFF}, 2, {0xFFFFFFFF});
    TestMultiplyVX<uint32_t>(0b100110, {2}, 0xFFFFFFFF, {1});

    // SEW = 64:
    // Case 1: vs2 = -1, vs1 = 2 -> high 0xFFFFFFFFFFFFFFFF
    // Case 2: vs2 = 2, vs1 = 0xFFFFFFFFFFFFFFFF -> high 1
    TestMultiplyVV<uint64_t>(0b100110, {0xFFFFFFFFFFFFFFFFULL, 2}, {2, 0xFFFFFFFFFFFFFFFFULL}, {0xFFFFFFFFFFFFFFFFULL, 1});
    TestMultiplyVX<uint64_t>(0b100110, {0xFFFFFFFFFFFFFFFFULL}, 2, {0xFFFFFFFFFFFFFFFFULL});
    TestMultiplyVX<uint64_t>(0b100110, {2}, 0xFFFFFFFFFFFFFFFFULL, {1});
}

TEST(VectorArithmeticTest, VectorMultiplyEdgeCasesAndExtrema)
{
    // 1. Zero and one
    // vmul.vv with 0 and 1
    TestMultiplyVV<uint32_t>(0b100101, {12345, 67890}, {0, 1}, {0, 67890});
    // vmul.vx with 0 and 1
    TestMultiplyVX<uint32_t>(0b100101, {10, 20, 30}, 0, {0, 0, 0});
    TestMultiplyVX<uint32_t>(0b100101, {10, 20, 30}, 1, {10, 20, 30});

    // 2. Positive x positive with both high and low halves nonzero
    // SEW = 8: 20 * 20 = 400 = 0x0190 -> low 0x90 (144), high 0x01 (1)
    TestMultiplyVV<uint8_t>(0b100101, {20}, {20}, {0x90});
    TestMultiplyVV<uint8_t>(0b100100, {20}, {20}, {0x01});
    TestMultiplyVV<uint8_t>(0b100111, {20}, {20}, {0x01});

    // SEW = 16: 1000 * 1000 = 1000000 = 0x000F4240 -> low 0x4240 (16960), high 0x000F (15)
    TestMultiplyVV<uint16_t>(0b100101, {1000}, {1000}, {0x4240});
    TestMultiplyVV<uint16_t>(0b100100, {1000}, {1000}, {0x000F});
    TestMultiplyVV<uint16_t>(0b100111, {1000}, {1000}, {0x000F});

    // SEW = 32: 70000 * 70000 = 4900000000 = 0x124101100 -> low 0x24101100 (605032704), high 1
    TestMultiplyVV<uint32_t>(0b100101, {70000}, {70000}, {0x24101100});
    TestMultiplyVV<uint32_t>(0b100100, {70000}, {70000}, {1});
    TestMultiplyVV<uint32_t>(0b100111, {70000}, {70000}, {1});

    // SEW = 64: 0x100000001ULL * 0x100000002ULL = 0x10000000300000002ULL
    // low: 0x0000000300000002ULL, high: 1
    TestMultiplyVV<uint64_t>(0b100101, {0x100000001ULL}, {0x100000002ULL}, {0x0000000300000002ULL});
    TestMultiplyVV<uint64_t>(0b100100, {0x100000001ULL}, {0x100000002ULL}, {1});
    TestMultiplyVV<uint64_t>(0b100111, {0x100000001ULL}, {0x100000002ULL}, {1});

    // 3. Negative x positive
    // SEW = 8: (-5) * 10 = -50 = 0xFFCE -> low 0xCE, high 0xFF (-1)
    TestMultiplyVV<uint8_t>(0b100101, {static_cast<uint8_t>(-5)}, {10}, {0xCE});
    TestMultiplyVV<uint8_t>(0b100111, {static_cast<uint8_t>(-5)}, {10}, {0xFF});

    // SEW = 32: (-1000) * 2000 = -2000000 = 0xFFFFFFFFFFE17B80 -> low 0xFFE17B80, high 0xFFFFFFFF
    TestMultiplyVV<uint32_t>(0b100101, {static_cast<uint32_t>(-1000)}, {2000}, {0xFFE17B80});
    TestMultiplyVV<uint32_t>(0b100111, {static_cast<uint32_t>(-1000)}, {2000}, {0xFFFFFFFF});

    // 4. Negative x negative
    // SEW = 8: (-2) * (-3) = 6 = 0x0006 -> low 6, high 0
    TestMultiplyVV<uint8_t>(0b100101, {static_cast<uint8_t>(-2)}, {static_cast<uint8_t>(-3)}, {6});
    TestMultiplyVV<uint8_t>(0b100111, {static_cast<uint8_t>(-2)}, {static_cast<uint8_t>(-3)}, {0});

    // SEW = 32: (-100000) * (-100000) = 10000000000 = 0x00000002540BE400 -> low 0x540BE400, high 2
    TestMultiplyVV<uint32_t>(0b100101, {static_cast<uint32_t>(-100000)}, {static_cast<uint32_t>(-100000)}, {0x540BE400});
    TestMultiplyVV<uint32_t>(0b100111, {static_cast<uint32_t>(-100000)}, {static_cast<uint32_t>(-100000)}, {2});

    // 5. Maximum unsigned values: (2^SEW - 1)^2 = (2^SEW - 2) * 2^SEW + 1
    // low is always 1, high is always 2^SEW - 2
    TestMultiplyVV<uint8_t>(0b100101, {0xFF}, {0xFF}, {1});
    TestMultiplyVV<uint8_t>(0b100100, {0xFF}, {0xFF}, {0xFE});

    TestMultiplyVV<uint16_t>(0b100101, {0xFFFF}, {0xFFFF}, {1});
    TestMultiplyVV<uint16_t>(0b100100, {0xFFFF}, {0xFFFF}, {0xFFFE});

    TestMultiplyVV<uint32_t>(0b100101, {0xFFFFFFFF}, {0xFFFFFFFF}, {1});
    TestMultiplyVV<uint32_t>(0b100100, {0xFFFFFFFF}, {0xFFFFFFFF}, {0xFFFFFFFE});

    TestMultiplyVV<uint64_t>(0b100101, {0xFFFFFFFFFFFFFFFFULL}, {0xFFFFFFFFFFFFFFFFULL}, {1});
    TestMultiplyVV<uint64_t>(0b100100, {0xFFFFFFFFFFFFFFFFULL}, {0xFFFFFFFFFFFFFFFFULL}, {0xFFFFFFFFFFFFFFFEULL});

    // 6. Minimum signed square: (-2^(SEW-1))^2 = 2^(2SEW-2) -> low is 0, high is 2^(SEW-2)
    TestMultiplyVV<uint8_t>(0b100101, {0x80}, {0x80}, {0});
    TestMultiplyVV<uint8_t>(0b100111, {0x80}, {0x80}, {0x40});

    TestMultiplyVV<uint16_t>(0b100101, {0x8000}, {0x8000}, {0});
    TestMultiplyVV<uint16_t>(0b100111, {0x8000}, {0x8000}, {0x4000});

    TestMultiplyVV<uint32_t>(0b100101, {0x80000000}, {0x80000000}, {0});
    TestMultiplyVV<uint32_t>(0b100111, {0x80000000}, {0x80000000}, {0x40000000});

    TestMultiplyVV<uint64_t>(0b100101, {0x8000000000000000ULL}, {0x8000000000000000ULL}, {0});
    TestMultiplyVV<uint64_t>(0b100111, {0x8000000000000000ULL}, {0x8000000000000000ULL}, {0x4000000000000000ULL});

    // 7. Minimum signed x Maximum signed: (-2^(SEW-1)) * (2^(SEW-1) - 1)
    // SEW = 8: (-128) * 127 = -16256 = 0xC080 -> low 0x80, high 0xC0
    TestMultiplyVV<uint8_t>(0b100101, {0x80}, {0x7F}, {0x80});
    TestMultiplyVV<uint8_t>(0b100111, {0x80}, {0x7F}, {0xC0});

    // SEW = 16: (-32768) * 32767 = 0xC0008000 -> low 0x8000, high 0xC000
    TestMultiplyVV<uint16_t>(0b100101, {0x8000}, {0x7FFF}, {0x8000});
    TestMultiplyVV<uint16_t>(0b100111, {0x8000}, {0x7FFF}, {0xC000});

    // SEW = 32: (-2^31) * (2^31 - 1) = 0xC000000080000000 -> low 0x80000000, high 0xC0000000
    TestMultiplyVV<uint32_t>(0b100101, {0x80000000}, {0x7FFFFFFF}, {0x80000000});
    TestMultiplyVV<uint32_t>(0b100111, {0x80000000}, {0x7FFFFFFF}, {0xC0000000});

    // SEW = 64: INT64_MIN * INT64_MAX -> low 0x8000000000000000, high 0xC000000000000000
    TestMultiplyVV<uint64_t>(0b100101, {0x8000000000000000ULL}, {0x7FFFFFFFFFFFFFFFULL}, {0x8000000000000000ULL});
    TestMultiplyVV<uint64_t>(0b100111, {0x8000000000000000ULL}, {0x7FFFFFFFFFFFFFFFULL}, {0xC000000000000000ULL});

    // 8. Maximum signed square: (2^(SEW-1) - 1)^2
    // SEW = 8: 127 * 127 = 16129 = 0x3F01 -> low 1, high 0x3F
    TestMultiplyVV<uint8_t>(0b100101, {0x7F}, {0x7F}, {1});
    TestMultiplyVV<uint8_t>(0b100111, {0x7F}, {0x7F}, {0x3F});

    // SEW = 16: 32767 * 32767 = 0x3FFF0001 -> low 1, high 0x3FFF
    TestMultiplyVV<uint16_t>(0b100101, {0x7FFF}, {0x7FFF}, {1});
    TestMultiplyVV<uint16_t>(0b100111, {0x7FFF}, {0x7FFF}, {0x3FFF});

    // SEW = 32: (2^31 - 1)^2 = 0x3FFFFFFF00000001 -> low 1, high 0x3FFFFFFF
    TestMultiplyVV<uint32_t>(0b100101, {0x7FFFFFFF}, {0x7FFFFFFF}, {1});
    TestMultiplyVV<uint32_t>(0b100111, {0x7FFFFFFF}, {0x7FFFFFFF}, {0x3FFFFFFF});

    // SEW = 64: INT64_MAX * INT64_MAX -> low 1, high 0x3FFFFFFFFFFFFFFF
    TestMultiplyVV<uint64_t>(0b100101, {0x7FFFFFFFFFFFFFFFULL}, {0x7FFFFFFFFFFFFFFFULL}, {1});
    TestMultiplyVV<uint64_t>(0b100111, {0x7FFFFFFFFFFFFFFFULL}, {0x7FFFFFFFFFFFFFFFULL}, {0x3FFFFFFFFFFFFFFFULL});
}

TEST(VectorArithmeticTest, VectorMultiplyScalarTruncationHighHalf)
{
    // Verify that for all high-half .vx instructions, only the low SEW bits of the scalar GPR
    // operand affect the result, and that signedness interpretation matches the instruction:
    // - vmulhu.vx: scalar treated as unsigned after truncation.
    // - vmulhsu.vx: scalar treated as unsigned after truncation.
    // - vmulh.vx: scalar treated as signed after truncation.

    // -------------------------------------------------------------
    // 1. vmulhu.vx (funct6 = 0b100100, funct3 = 0b110)
    // -------------------------------------------------------------
    // SEW = 8: scalar has nonzero upper bits (0x123456789ABC01FF -> low 8 bits = 0xFF = 255)
    // vs2 = {255, 200, 128, 2}
    // 255 * 255 = 65025 = 0xFE01 -> high 0xFE (254)
    // 200 * 255 = 51000 = 0xC738 -> high 0xC7 (199)
    // 128 * 255 = 32640 = 0x7F80 -> high 0x7F (127)
    // 2 * 255 = 510 = 0x01FE -> high 1
    TestMultiplyVX<uint8_t>(0b100100, {255, 200, 128, 2}, 0x123456789ABC01FFULL, {0xFE, 199, 127, 1});

    // SEW = 16: scalar has nonzero upper bits (0xDEADBEEF0001FFFF -> low 16 bits = 0xFFFF = 65535)
    // vs2 = {0xFFFF, 0x8000, 2}
    // 0xFFFF * 0xFFFF = 0xFFFE0001 -> high 0xFFFE
    // 0x8000 * 0xFFFF = 0x7FFF8000 -> high 0x7FFF
    // 2 * 0xFFFF = 0x0001FFFE -> high 1
    TestMultiplyVX<uint16_t>(0b100100, {0xFFFF, 0x8000, 2}, 0xDEADBEEF0001FFFFULL, {0xFFFE, 0x7FFF, 1});

    // SEW = 32: scalar has nonzero upper bits (0xCAFEBABEFFFFFFFF -> low 32 bits = 0xFFFFFFFF)
    // vs2 = {0xFFFFFFFF, 0x80000000, 2}
    // 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001 -> high 0xFFFFFFFE
    // 0x80000000 * 0xFFFFFFFF = 0x7FFFFFFF80000000 -> high 0x7FFFFFFF
    // 2 * 0xFFFFFFFF = 0x00000001FFFFFFFE -> high 1
    TestMultiplyVX<uint32_t>(0b100100, {0xFFFFFFFF, 0x80000000, 2}, 0xCAFEBABEFFFFFFFFULL, {0xFFFFFFFE, 0x7FFFFFFF, 1});

    // -------------------------------------------------------------
    // 2. vmulhsu.vx (funct6 = 0b100110, funct3 = 0b110)
    // signed(vs2) * unsigned(scalar)
    // -------------------------------------------------------------
    // SEW = 8: scalar has nonzero upper bits (0xAAAAAAAAAAAA01FF -> low 8 bits = 0xFF = 255 unsigned)
    // vs2 = {-1 (0xFF), -128 (0x80), 2}
    // (-1) * 255 = -255 = 0xFF01 -> high 0xFF (if scalar was signed, (-1)*(-1)=1 -> high 0)
    // (-128) * 255 = -32640 = 0x8080 -> high 0x80
    // 2 * 255 = 510 = 0x01FE -> high 1
    TestMultiplyVX<uint8_t>(0b100110, {0xFF, 0x80, 2}, 0xAAAAAAAAAAAA01FFULL, {0xFF, 0x80, 1});

    // SEW = 16: scalar has nonzero upper bits (0xBBBBBBBB0001FFFF -> low 16 bits = 0xFFFF = 65535 unsigned)
    // vs2 = {-1 (0xFFFF), -32768 (0x8000), 2}
    // (-1) * 65535 = -65535 = 0xFFFF0001 -> high 0xFFFF
    // (-32768) * 65535 = 0x80008000 -> high 0x8000
    // 2 * 65535 = 131070 = 0x0001FFFE -> high 1
    TestMultiplyVX<uint16_t>(0b100110, {0xFFFF, 0x8000, 2}, 0xBBBBBBBB0001FFFFULL, {0xFFFF, 0x8000, 1});

    // SEW = 32: scalar has nonzero upper bits (0x55555555FFFFFFFF -> low 32 bits = 0xFFFFFFFF = 2^32-1 unsigned)
    // vs2 = {-1, -2^31 (0x80000000), 2}
    // (-1) * (2^32-1) = 0xFFFFFFFF00000001 -> high 0xFFFFFFFF
    // (-2^31) * (2^32-1) = 0x8000000080000000 -> high 0x80000000
    // 2 * (2^32-1) = 0x00000001FFFFFFFE -> high 1
    TestMultiplyVX<uint32_t>(0b100110, {0xFFFFFFFF, 0x80000000, 2}, 0x55555555FFFFFFFFULL, {0xFFFFFFFF, 0x80000000, 1});

    // -------------------------------------------------------------
    // 3. vmulh.vx (funct6 = 0b100111, funct3 = 0b110)
    // signed(vs2) * signed(scalar)
    // -------------------------------------------------------------
    // SEW = 8: scalar has nonzero upper bits (0x11223344556601FF -> low 8 bits = 0xFF = -1 signed)
    // vs2 = {-1 (0xFF), -128 (0x80), 64 (0x40), 2}
    // (-1) * (-1) = 1 = 0x0001 -> high 0 (if scalar was unsigned, (-1)*255 -> high 0xFF)
    // (-128) * (-1) = 128 = 0x0080 -> high 0
    // 64 * (-1) = -64 = 0xFFC0 -> high 0xFF
    // 2 * (-1) = -2 = 0xFFFE -> high 0xFF
    TestMultiplyVX<uint8_t>(0b100111, {0xFF, 0x80, 0x40, 2}, 0x11223344556601FFULL, {0, 0, 0xFF, 0xFF});

    // SEW = 16: scalar has nonzero upper bits (0x998877660001FFFF -> low 16 bits = 0xFFFF = -1 signed)
    // vs2 = {-1 (0xFFFF), -32768 (0x8000), 100}
    // (-1) * (-1) = 1 -> high 0
    // (-32768) * (-1) = 32768 = 0x00008000 -> high 0
    // 100 * (-1) = -100 = 0xFFFFFF9C -> high 0xFFFF
    TestMultiplyVX<uint16_t>(0b100111, {0xFFFF, 0x8000, 100}, 0x998877660001FFFFULL, {0, 0, 0xFFFF});

    // SEW = 32: scalar has nonzero upper bits (0x12345678FFFFFFFF -> low 32 bits = 0xFFFFFFFF = -1 signed)
    // vs2 = {-1, -2^31 (0x80000000), 500}
    // (-1) * (-1) = 1 -> high 0
    // (-2^31) * (-1) = 2^31 = 0x0000000080000000 -> high 0
    // 500 * (-1) = -500 -> high 0xFFFFFFFF
    TestMultiplyVX<uint32_t>(0b100111, {0xFFFFFFFF, 0x80000000, 500}, 0x12345678FFFFFFFFULL, {0, 0, 0xFFFFFFFF});
}

TEST(VectorArithmeticTest, VectorMultiplyMaskingAndPolicies)
{
    // Test 1: vmul.vv masked with mu (mask-undisturbed)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmul.vv v3, v1, v2, v0.t (vm = 0, funct3 = 0b010, ta = false, ma = false)
        LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b100101, 3, 1, 2, 0, 0b010));

        // Mask in v0: bit 0 = 1, bit 1 = 0, bit 2 = 1, bit 3 = 0 -> 0b00000101
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00000101);

        WriteElements<uint32_t>(processor.registers_, 1, {2, 3, 4, 5});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Element 0: active -> 2 * 10 = 20
        // Element 1: masked-off, mu -> retains 200
        // Element 2: active -> 4 * 30 = 120
        // Element 3: masked-off, mu -> retains 400
        ExpectElements<uint32_t>(processor.registers_, 3, {20, 200, 120, 400});
    }

    // Test 2: vmulh.vx masked with ma (mask-agnostic, inactive elements filled with 1s)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmulh.vx v3, v1, x2, v0.t (vm = 0, funct3 = 0b110, ta = false, ma = true)
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b100111, 3, 1, 2, 0, 0b110), 0x80000000, 2, false, true);

        // Mask in v0: bit 0 = 0, bit 1 = 1, bit 2 = 0, bit 3 = 1 -> 0b00001010
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00001010);

        WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x80000000, 0x40, 0x40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Element 0: inactive, ma -> 0xFFFFFFFF
        // Element 1: active -> INT32_MIN * INT32_MIN high -> 0x40000000
        // Element 2: inactive, ma -> 0xFFFFFFFF
        // Element 3: active -> 0x40 * INT32_MIN high -> (-2^31 * 64) high = -32 = 0xFFFFFFE0
        // 64 * (-2147483648) = -137438953472 = 0xFFFFFFE0_00000000 -> high is 0xFFFFFFE0
        ExpectElements<uint32_t>(processor.registers_, 3, {0xFFFFFFFFu, 0x40000000u, 0xFFFFFFFFu, 0xFFFFFFE0u});
    }
}

TEST(VectorArithmeticTest, VectorMultiplyTailPolicy)
{
    // Test 1: tail-agnostic (ta = true) for vmulhu.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmulhu.vv v3, v1, v2 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = true (tail-agnostic)
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(32, true, false),
            EncodeVectorVV(0b100100, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 2);
        processor.Step(); // configure vtype/vl

        WriteElements<uint32_t>(processor.registers_, 1, {0xFFFFFFFF, 0x80000000, 0x10, 0x20});
        WriteElements<uint32_t>(processor.registers_, 2, {0xFFFFFFFF, 2, 0x10, 0x20});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1 active: high half of 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE, high half of 0x80000000 * 2 = 1
        // Tail elements 2, 3: ta -> 0xFFFFFFFF
        ExpectElements<uint32_t>(processor.registers_, 3, {0xFFFFFFFEu, 1u, 0xFFFFFFFFu, 0xFFFFFFFFu});
    }

    // Test 2: tail-undisturbed (ta = false) for vmul.vx
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmul.vx v3, v1, x2 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = false (tail-undisturbed)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100101, 3, 1, 2, 1, 0b110), 5, 2, false, false);

        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1 active: 10 * 5 = 50, 20 * 5 = 100
        // Tail elements 2, 3: undisturbed -> retain 300, 400
        ExpectElements<uint32_t>(processor.registers_, 3, {50, 100, 300, 400});
    }
}

TEST(VectorArithmeticTest, VectorMultiplyVstart)
{
    // Test 1: 0 < vstart < vl with vmul.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b100101, 3, 1, 2, 1, 0b010));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {2, 3, 4, 5});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1: prestart undisturbed -> retain 100, 200
        // Elements 2, 3: active -> 4 * 30 = 120, 5 * 40 = 200
        // vstart reset to 0
        ExpectElements<uint32_t>(processor.registers_, 3, {100, 200, 120, 200});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 2: vstart == vl with vmul.vx -> no body elements executed, destination undisturbed, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100101, 3, 1, 2, 1, 0b110), 10);
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {1, 2, 3, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 3: vstart > vl with vmulh.vv -> executes no elements, destination unchanged, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100111, 3, 1, 2, 1, 0b010));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

        WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x40000000});
        WriteElements<uint32_t>(processor.registers_, 2, {2, 2});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, VectorMultiplyAliasingAndHazardSafety)
{
    // Test 1: vd == vs2 (in-place multiply)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmul.vx v1, v1, x2 (vd = 1, vs2 = 1, rs1 = 2)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100101, 1, 1, 2, 1, 0b110), 3);
        WriteElements<uint32_t>(processor.registers_, 1, {10, 20});

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 0), 30u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 1), 60u);
    }

    // Test 2: vd == vs1 with vmul.vv (destination overwrites vs1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmul.vv v2, v1, v2 (vd = 2, vs2 = 1, vs1 = 2)
        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100101, 2, 1, 2, 1, 0b010));
        WriteElements<uint32_t>(processor.registers_, 1, {5, 6});
        WriteElements<uint32_t>(processor.registers_, 2, {7, 8});

        processor.Step();

        // 5 * 7 = 35, 6 * 8 = 48
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 0), 35u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 1), 48u);
    }

    // Test 3: vd == v0 with vm = 0 (destination is mask register)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vmul.vx v0, v1, x2, v0.t (vd = 0, vs2 = 1, rs1 = 2, vm = 0)
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b100101, 0, 1, 2, 0, 0b110), 3, 2, false, false);

        // Initial mask in v0: bit 0=1, bit 1=1, bit 2=0, bit 3=1 -> 0x0B
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0B);

        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        processor.registers_.WriteVregElement<uint32_t>(0, 1, 100);
        processor.registers_.WriteVregElement<uint32_t>(0, 2, 200);
        processor.registers_.WriteVregElement<uint32_t>(0, 3, 300);

        processor.Step();

        // Element 0: active -> 10 * 3 = 30
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 0), 30u);
        // Element 1: active -> 20 * 3 = 60
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 1), 60u);
        // Element 2: inactive (mask bit 2 was 0) -> retains 200
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 2), 200u);
        // Element 3: active -> 40 * 3 = 120
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 3), 120u);
    }
}

TEST(VectorArithmeticTest, VectorMultiplyUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vmulh.vv v3, v1, v2
    LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100111, 3, 1, 2, 1, 0b010));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);

    WriteElements<uint32_t>(processor.registers_, 1, {0x80000000, 0x80000000});
    WriteElements<uint32_t>(processor.registers_, 2, {0x80000000, 0x80000000});
    WriteElements<uint32_t>(processor.registers_, 3, {100, 200});

    // Step vmulh.vv: element 0 prestart preserved (100), element 1 -> INT32_MIN * INT32_MIN high = 0x40000000
    processor.Step();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 0x40000000u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

    // Undo -> restores element 1 to 200, vstart to 1
    processor.Undo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 200u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1u);

    // Redo -> restores element 1 to 0x40000000, vstart to 0
    processor.Redo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 100u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 0x40000000u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

// ---------------------------------------------------------
// Batch 5: Vector Integer Division and Remainder Tests
// ---------------------------------------------------------

template <typename T>
void TestDivisionVV(
    uint32_t funct6,
    const std::vector<T> &lhs,
    const std::vector<T> &rhs,
    const std::vector<T> &expected)
{
    TestMultiplyVV<T>(funct6, lhs, rhs, expected);
}

template <typename T>
void TestDivisionVX(
    uint32_t funct6,
    const std::vector<T> &lhs,
    uint64_t scalar_val,
    const std::vector<T> &expected,
    unsigned int avl = 0)
{
    TestMultiplyVX<T>(funct6, lhs, scalar_val, expected, avl);
}

TEST(VectorArithmeticTest, VdivBasicAllSEW)
{
    // vdiv.vv (funct6 = 0b100001, funct3 = 0b010)
    // SEW = 8: 20 / 3 = 6, -20 / 3 = -6, 20 / -3 = -6, -20 / -3 = 6
    TestDivisionVV<uint8_t>(0b100001,
        {20, static_cast<uint8_t>(-20), 20, static_cast<uint8_t>(-20)},
        {3, 3, static_cast<uint8_t>(-3), static_cast<uint8_t>(-3)},
        {6, static_cast<uint8_t>(-6), static_cast<uint8_t>(-6), 6});

    // SEW = 16: 1000 / 7 = 142, -1000 / 7 = -142
    TestDivisionVV<uint16_t>(0b100001,
        {1000, static_cast<uint16_t>(-1000)},
        {7, 7},
        {142, static_cast<uint16_t>(-142)});

    // SEW = 32: 100000 / 10 = 10000, -100000 / 10 = -10000
    TestDivisionVV<uint32_t>(0b100001,
        {100000, static_cast<uint32_t>(-100000)},
        {10, 10},
        {10000, static_cast<uint32_t>(-10000)});

    // SEW = 64: 1000000000000 / 1000 = 1000000000
    TestDivisionVV<uint64_t>(0b100001,
        {1000000000000ULL, static_cast<uint64_t>(-1000000000000LL)},
        {1000, 1000},
        {1000000000ULL, static_cast<uint64_t>(-1000000000LL)});

    // vdiv.vx (funct6 = 0b100001, funct3 = 0b110)
    TestDivisionVX<uint8_t>(0b100001, {20, static_cast<uint8_t>(-20)}, 3, {6, static_cast<uint8_t>(-6)});
    TestDivisionVX<uint16_t>(0b100001, {1000, static_cast<uint16_t>(-1000)}, 7, {142, static_cast<uint16_t>(-142)});
    TestDivisionVX<uint32_t>(0b100001, {100000, static_cast<uint32_t>(-100000)}, 10, {10000, static_cast<uint32_t>(-10000)});
    TestDivisionVX<uint64_t>(0b100001, {1000000000000ULL, static_cast<uint64_t>(-1000000000000LL)}, 1000, {1000000000ULL, static_cast<uint64_t>(-1000000000LL)});
}

TEST(VectorArithmeticTest, VdivuBasicAllSEW)
{
    // vdivu.vv (funct6 = 0b100000, funct3 = 0b010)
    // SEW = 8: 200 / 3 = 66, 20 / 3 = 6
    TestDivisionVV<uint8_t>(0b100000, {200, 20}, {3, 3}, {66, 6});

    // SEW = 16: 60000 / 7 = 8571, 1000 / 7 = 142
    TestDivisionVV<uint16_t>(0b100000, {60000, 1000}, {7, 7}, {8571, 142});

    // SEW = 32: 4000000000 / 10 = 400000000, 100000 / 10 = 10000
    TestDivisionVV<uint32_t>(0b100000, {4000000000U, 100000}, {10, 10}, {400000000U, 10000});

    // SEW = 64: 1000000000000 / 1000 = 1000000000
    TestDivisionVV<uint64_t>(0b100000, {1000000000000ULL}, {1000}, {1000000000ULL});

    // vdivu.vx (funct6 = 0b100000, funct3 = 0b110)
    TestDivisionVX<uint8_t>(0b100000, {200, 20}, 3, {66, 6});
    TestDivisionVX<uint16_t>(0b100000, {60000, 1000}, 7, {8571, 142});
    TestDivisionVX<uint32_t>(0b100000, {4000000000U, 100000}, 10, {400000000U, 10000});
    TestDivisionVX<uint64_t>(0b100000, {1000000000000ULL}, 1000, {1000000000ULL});
}

TEST(VectorArithmeticTest, VremBasicAllSEW)
{
    // vrem.vv (funct6 = 0b100011, funct3 = 0b010)
    // Nonzero remainder sign matches the dividend:
    // 20 % 3 = 2, -20 % 3 = -2, 20 % -3 = 2, -20 % -3 = -2
    TestDivisionVV<uint8_t>(0b100011,
        {20, static_cast<uint8_t>(-20), 20, static_cast<uint8_t>(-20)},
        {3, 3, static_cast<uint8_t>(-3), static_cast<uint8_t>(-3)},
        {2, static_cast<uint8_t>(-2), 2, static_cast<uint8_t>(-2)});

    // SEW = 16: 1000 % 7 = 6, -1000 % 7 = -6
    TestDivisionVV<uint16_t>(0b100011,
        {1000, static_cast<uint16_t>(-1000)},
        {7, 7},
        {6, static_cast<uint16_t>(-6)});

    // SEW = 32: 100005 % 10 = 5, -100005 % 10 = -5
    TestDivisionVV<uint32_t>(0b100011,
        {100005, static_cast<uint32_t>(-100005)},
        {10, 10},
        {5, static_cast<uint32_t>(-5)});

    // SEW = 64: 1000000000007 % 1000 = 7, -1000000000007 % 1000 = -7
    TestDivisionVV<uint64_t>(0b100011,
        {1000000000007ULL, static_cast<uint64_t>(-1000000000007LL)},
        {1000, 1000},
        {7, static_cast<uint64_t>(-7)});

    // vrem.vx (funct6 = 0b100011, funct3 = 0b110)
    TestDivisionVX<uint8_t>(0b100011, {20, static_cast<uint8_t>(-20)}, 3, {2, static_cast<uint8_t>(-2)});
    TestDivisionVX<uint16_t>(0b100011, {1000, static_cast<uint16_t>(-1000)}, 7, {6, static_cast<uint16_t>(-6)});
    TestDivisionVX<uint32_t>(0b100011, {100005, static_cast<uint32_t>(-100005)}, 10, {5, static_cast<uint32_t>(-5)});
    TestDivisionVX<uint64_t>(0b100011, {1000000000007ULL, static_cast<uint64_t>(-1000000000007LL)}, 1000, {7, static_cast<uint64_t>(-7)});
}

TEST(VectorArithmeticTest, VremuBasicAllSEW)
{
    // vremu.vv (funct6 = 0b100010, funct3 = 0b010)
    // SEW = 8: 200 % 3 = 2, 20 % 3 = 2
    TestDivisionVV<uint8_t>(0b100010, {200, 20}, {3, 3}, {2, 2});

    // SEW = 16: 60000 % 7 = 3, 1000 % 7 = 6
    TestDivisionVV<uint16_t>(0b100010, {60000, 1000}, {7, 7}, {3, 6});

    // SEW = 32: 4000000005 % 10 = 5, 100005 % 10 = 5
    TestDivisionVV<uint32_t>(0b100010, {4000000005U, 100005}, {10, 10}, {5, 5});

    // SEW = 64: 1000000000007 % 1000 = 7
    TestDivisionVV<uint64_t>(0b100010, {1000000000007ULL}, {1000}, {7});

    // vremu.vx (funct6 = 0b100010, funct3 = 0b110)
    TestDivisionVX<uint8_t>(0b100010, {200, 20}, 3, {2, 2});
    TestDivisionVX<uint16_t>(0b100010, {60000, 1000}, 7, {3, 6});
    TestDivisionVX<uint32_t>(0b100010, {4000000005U, 100005}, 10, {5, 5});
    TestDivisionVX<uint64_t>(0b100010, {1000000000007ULL}, 1000, {7});
}

TEST(VectorArithmeticTest, VectorDivisionEdgeCasesAndOverflow)
{
    // -------------------------------------------------------------
    // 1. Division and Remainder by Zero
    // vdiv, vdivu return all ones (element_mask / -1)
    // vrem, vremu return original dividend a
    // -------------------------------------------------------------

    // SEW = 8:
    // vdiv.vv: dividend / 0 -> 0xFF (-1)
    TestDivisionVV<uint8_t>(0b100001, {20, static_cast<uint8_t>(-20), 0}, {0, 0, 0}, {0xFF, 0xFF, 0xFF});
    // vdiv.vx: dividend / 0 -> 0xFF (-1)
    TestDivisionVX<uint8_t>(0b100001, {20, static_cast<uint8_t>(-20), 0}, 0, {0xFF, 0xFF, 0xFF});
    // vdivu.vv: dividend / 0 -> 0xFF
    TestDivisionVV<uint8_t>(0b100000, {200, 0}, {0, 0}, {0xFF, 0xFF});
    // vdivu.vx: dividend / 0 -> 0xFF
    TestDivisionVX<uint8_t>(0b100000, {200, 0}, 0, {0xFF, 0xFF});

    // vrem.vv: dividend % 0 -> dividend
    TestDivisionVV<uint8_t>(0b100011, {20, static_cast<uint8_t>(-20), 0}, {0, 0, 0}, {20, static_cast<uint8_t>(-20), 0});
    // vrem.vx: dividend % 0 -> dividend
    TestDivisionVX<uint8_t>(0b100011, {20, static_cast<uint8_t>(-20), 0}, 0, {20, static_cast<uint8_t>(-20), 0});
    // vremu.vv: dividend % 0 -> dividend
    TestDivisionVV<uint8_t>(0b100010, {200, 0}, {0, 0}, {200, 0});
    // vremu.vx: dividend % 0 -> dividend
    TestDivisionVX<uint8_t>(0b100010, {200, 0}, 0, {200, 0});

    // SEW = 16:
    TestDivisionVV<uint16_t>(0b100001, {1000, static_cast<uint16_t>(-1000)}, {0, 0}, {0xFFFF, 0xFFFF});
    TestDivisionVX<uint16_t>(0b100001, {1000, static_cast<uint16_t>(-1000)}, 0, {0xFFFF, 0xFFFF});
    TestDivisionVV<uint16_t>(0b100000, {50000, 0}, {0, 0}, {0xFFFF, 0xFFFF});
    TestDivisionVX<uint16_t>(0b100000, {50000, 0}, 0, {0xFFFF, 0xFFFF});

    TestDivisionVV<uint16_t>(0b100011, {1000, static_cast<uint16_t>(-1000)}, {0, 0}, {1000, static_cast<uint16_t>(-1000)});
    TestDivisionVX<uint16_t>(0b100011, {1000, static_cast<uint16_t>(-1000)}, 0, {1000, static_cast<uint16_t>(-1000)});
    TestDivisionVV<uint16_t>(0b100010, {50000, 0}, {0, 0}, {50000, 0});
    TestDivisionVX<uint16_t>(0b100010, {50000, 0}, 0, {50000, 0});

    // SEW = 32:
    TestDivisionVV<uint32_t>(0b100001, {123456}, {0}, {0xFFFFFFFFu});
    TestDivisionVX<uint32_t>(0b100001, {123456}, 0, {0xFFFFFFFFu});
    TestDivisionVV<uint32_t>(0b100000, {123456}, {0}, {0xFFFFFFFFu});
    TestDivisionVX<uint32_t>(0b100000, {123456}, 0, {0xFFFFFFFFu});

    TestDivisionVV<uint32_t>(0b100011, {123456}, {0}, {123456});
    TestDivisionVX<uint32_t>(0b100011, {123456}, 0, {123456});
    TestDivisionVV<uint32_t>(0b100010, {123456}, {0}, {123456});
    TestDivisionVX<uint32_t>(0b100010, {123456}, 0, {123456});

    // SEW = 64:
    TestDivisionVV<uint64_t>(0b100001, {1234567890123ULL}, {0}, {0xFFFFFFFFFFFFFFFFULL});
    TestDivisionVX<uint64_t>(0b100001, {1234567890123ULL}, 0, {0xFFFFFFFFFFFFFFFFULL});
    TestDivisionVV<uint64_t>(0b100000, {1234567890123ULL}, {0}, {0xFFFFFFFFFFFFFFFFULL});
    TestDivisionVX<uint64_t>(0b100000, {1234567890123ULL}, 0, {0xFFFFFFFFFFFFFFFFULL});

    TestDivisionVV<uint64_t>(0b100011, {1234567890123ULL}, {0}, {1234567890123ULL});
    TestDivisionVX<uint64_t>(0b100011, {1234567890123ULL}, 0, {1234567890123ULL});
    TestDivisionVV<uint64_t>(0b100010, {1234567890123ULL}, {0}, {1234567890123ULL});
    TestDivisionVX<uint64_t>(0b100010, {1234567890123ULL}, 0, {1234567890123ULL});

    // -------------------------------------------------------------
    // 2. Signed Overflow (INT_MIN / -1 and INT_MIN % -1)
    // In two's complement, INT_MIN / -1 overflows.
    // RVV 1.0 requires vdiv to return INT_MIN and vrem to return 0.
    // -------------------------------------------------------------

    // SEW = 8: INT8_MIN = 0x80 (-128), -1 = 0xFF
    TestDivisionVV<uint8_t>(0b100001, {0x80}, {0xFF}, {0x80});
    TestDivisionVX<uint8_t>(0b100001, {0x80}, 0xFFFFFFFFFFFFFFFFULL, {0x80});
    TestDivisionVV<uint8_t>(0b100011, {0x80}, {0xFF}, {0});
    TestDivisionVX<uint8_t>(0b100011, {0x80}, 0xFFFFFFFFFFFFFFFFULL, {0});

    // SEW = 16: INT16_MIN = 0x8000 (-32768), -1 = 0xFFFF
    TestDivisionVV<uint16_t>(0b100001, {0x8000}, {0xFFFF}, {0x8000});
    TestDivisionVX<uint16_t>(0b100001, {0x8000}, 0xFFFFFFFFFFFFFFFFULL, {0x8000});
    TestDivisionVV<uint16_t>(0b100011, {0x8000}, {0xFFFF}, {0});
    TestDivisionVX<uint16_t>(0b100011, {0x8000}, 0xFFFFFFFFFFFFFFFFULL, {0});

    // SEW = 32: INT32_MIN = 0x80000000, -1 = 0xFFFFFFFF
    TestDivisionVV<uint32_t>(0b100001, {0x80000000u}, {0xFFFFFFFFu}, {0x80000000u});
    TestDivisionVX<uint32_t>(0b100001, {0x80000000u}, 0xFFFFFFFFFFFFFFFFULL, {0x80000000u});
    TestDivisionVV<uint32_t>(0b100011, {0x80000000u}, {0xFFFFFFFFu}, {0});
    TestDivisionVX<uint32_t>(0b100011, {0x80000000u}, 0xFFFFFFFFFFFFFFFFULL, {0});

    // SEW = 64: INT64_MIN = 0x8000000000000000, -1 = 0xFFFFFFFFFFFFFFFF
    TestDivisionVV<uint64_t>(0b100001, {0x8000000000000000ULL}, {0xFFFFFFFFFFFFFFFFULL}, {0x8000000000000000ULL});
    TestDivisionVX<uint64_t>(0b100001, {0x8000000000000000ULL}, 0xFFFFFFFFFFFFFFFFULL, {0x8000000000000000ULL});
    TestDivisionVV<uint64_t>(0b100011, {0x8000000000000000ULL}, {0xFFFFFFFFFFFFFFFFULL}, {0});
    TestDivisionVX<uint64_t>(0b100011, {0x8000000000000000ULL}, 0xFFFFFFFFFFFFFFFFULL, {0});

    // -------------------------------------------------------------
    // 3. Division by 1, -1, and Division by Self
    // -------------------------------------------------------------
    TestDivisionVV<uint32_t>(0b100001, {42, static_cast<uint32_t>(-42)}, {1, 1}, {42, static_cast<uint32_t>(-42)});
    TestDivisionVV<uint32_t>(0b100001, {42, static_cast<uint32_t>(-42)}, {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)}, {static_cast<uint32_t>(-42), 42});
    TestDivisionVV<uint32_t>(0b100001, {42, static_cast<uint32_t>(-42)}, {42, static_cast<uint32_t>(-42)}, {1, 1});

    TestDivisionVV<uint32_t>(0b100011, {42, static_cast<uint32_t>(-42)}, {1, 1}, {0, 0});
    TestDivisionVV<uint32_t>(0b100011, {42, static_cast<uint32_t>(-42)}, {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)}, {0, 0});
    TestDivisionVV<uint32_t>(0b100011, {42, static_cast<uint32_t>(-42)}, {42, static_cast<uint32_t>(-42)}, {0, 0});
}

TEST(VectorArithmeticTest, VectorDivisionScalarTruncationAndSignedness)
{
    // Verify that for all four .vx instructions (vdivu.vx, vdiv.vx, vremu.vx, vrem.vx),
    // only the low SEW bits of the scalar GPR affect the result, and signedness
    // interpretation strictly matches the instruction across SEW 8, 16, 32, and 64.

    // -------------------------------------------------------------
    // 1. vdivu.vx (funct6 = 0b100000, funct3 = 0b110)
    // -------------------------------------------------------------
    // SEW = 8: scalar has nonzero upper bits (0xDEADBEEF00000103 -> low 8 bits = 3 unsigned)
    TestDivisionVX<uint8_t>(0b100000, {60, 200}, 0xDEADBEEF00000103ULL, {20, 66});
    // SEW = 8: scalar has 0xFF (255 unsigned) in low 8 bits (0xAAAAAAAAAAAA01FF)
    TestDivisionVX<uint8_t>(0b100000, {255, 100}, 0xAAAAAAAAAAAA01FFULL, {1, 0});

    // SEW = 16: scalar has nonzero upper bits (0xCAFEBABE00010007 -> low 16 bits = 7 unsigned)
    TestDivisionVX<uint16_t>(0b100000, {700, 60000}, 0xCAFEBABE00010007ULL, {100, 8571});
    // SEW = 16: scalar has 0xFFFF (65535 unsigned) in low 16 bits (0xCAFEBABE0001FFFF)
    TestDivisionVX<uint16_t>(0b100000, {65535, 30000}, 0xCAFEBABE0001FFFFULL, {1, 0});

    // SEW = 32: scalar has nonzero upper bits (0xFEEDFACE0000000A -> low 32 bits = 10 unsigned)
    TestDivisionVX<uint32_t>(0b100000, {500, 4000000000U}, 0xFEEDFACE0000000AULL, {50, 400000000U});
    // SEW = 32: scalar has 0xFFFFFFFF (2^32-1 unsigned) in low 32 bits (0xFEEDFACEFFFFFFFF)
    TestDivisionVX<uint32_t>(0b100000, {0xFFFFFFFFu, 1000}, 0xFEEDFACEFFFFFFFFULL, {1, 0});

    // SEW = 64: full 64-bit scalar
    TestDivisionVX<uint64_t>(0b100000, {5000ULL, 1000000000000ULL}, 10ULL, {500ULL, 100000000000ULL});

    // -------------------------------------------------------------
    // 2. vdiv.vx (funct6 = 0b100001, funct3 = 0b110)
    // -------------------------------------------------------------
    // SEW = 8: scalar has nonzero upper bits (0xDEADBEEF00000103 -> low 8 bits = 3 signed)
    TestDivisionVX<uint8_t>(0b100001, {60, static_cast<uint8_t>(-60)}, 0xDEADBEEF00000103ULL, {20, static_cast<uint8_t>(-20)});
    // SEW = 8: scalar low 8 bits = 0xFF (treated as signed -1!).
    // 20 / -1 = -20 (0xEC). If treated as unsigned 255, 20 / 255 = 0.
    TestDivisionVX<uint8_t>(0b100001, {20, static_cast<uint8_t>(-20), 0x80}, 0xAAAAAAAAAAAA01FFULL,
        {static_cast<uint8_t>(-20), 20, 0x80}); // 0x80 is INT8_MIN / -1 -> INT8_MIN

    // SEW = 16: scalar low 16 bits = 7 signed
    TestDivisionVX<uint16_t>(0b100001, {700, static_cast<uint16_t>(-700)}, 0xCAFEBABE00010007ULL, {100, static_cast<uint16_t>(-100)});
    // SEW = 16: scalar low 16 bits = 0xFFFF (treated as signed -1!).
    TestDivisionVX<uint16_t>(0b100001, {100, static_cast<uint16_t>(-100), 0x8000}, 0xCAFEBABE0001FFFFULL,
        {static_cast<uint16_t>(-100), 100, 0x8000});

    // SEW = 32: scalar low 32 bits = 10 signed
    TestDivisionVX<uint32_t>(0b100001, {500, static_cast<uint32_t>(-500)}, 0xFEEDFACE0000000AULL, {50, static_cast<uint32_t>(-50)});
    // SEW = 32: scalar low 32 bits = 0xFFFFFFFF (treated as signed -1!).
    TestDivisionVX<uint32_t>(0b100001, {1000, static_cast<uint32_t>(-1000), 0x80000000u}, 0xFEEDFACEFFFFFFFFULL,
        {static_cast<uint32_t>(-1000), 1000, 0x80000000u});

    // SEW = 64: full 64-bit signed scalar (-10LL)
    TestDivisionVX<uint64_t>(0b100001, {5000ULL, static_cast<uint64_t>(-5000LL)}, static_cast<uint64_t>(-10LL),
        {static_cast<uint64_t>(-500LL), 500ULL});

    // -------------------------------------------------------------
    // 3. vremu.vx (funct6 = 0b100010, funct3 = 0b110)
    // -------------------------------------------------------------
    // SEW = 8: scalar low 8 bits = 3 unsigned
    TestDivisionVX<uint8_t>(0b100010, {20, 200}, 0xDEADBEEF00000103ULL, {2, 2});
    // SEW = 8: scalar low 8 bits = 255 unsigned. 200 % 255 = 200
    TestDivisionVX<uint8_t>(0b100010, {200, 255}, 0xAAAAAAAAAAAA01FFULL, {200, 0});

    // SEW = 16: scalar low 16 bits = 7 unsigned
    TestDivisionVX<uint16_t>(0b100010, {1000, 60000}, 0xCAFEBABE00010007ULL, {6, 3});
    // SEW = 16: scalar low 16 bits = 65535 unsigned. 60000 % 65535 = 60000
    TestDivisionVX<uint16_t>(0b100010, {60000, 65535}, 0xCAFEBABE0001FFFFULL, {60000, 0});

    // SEW = 32: scalar low 32 bits = 10 unsigned
    TestDivisionVX<uint32_t>(0b100010, {100005, 4000000005U}, 0xFEEDFACE0000000AULL, {5, 5});
    // SEW = 32: scalar low 32 bits = 2^32-1 unsigned. 1000 % (2^32-1) = 1000
    TestDivisionVX<uint32_t>(0b100010, {1000, 0xFFFFFFFFu}, 0xFEEDFACEFFFFFFFFULL, {1000, 0});

    // SEW = 64: full 64-bit scalar (1000ULL)
    TestDivisionVX<uint64_t>(0b100010, {1000000000007ULL}, 1000ULL, {7});

    // -------------------------------------------------------------
    // 4. vrem.vx (funct6 = 0b100011, funct3 = 0b110)
    // -------------------------------------------------------------
    // SEW = 8: scalar low 8 bits = 3 signed. Remainder sign matches dividend.
    TestDivisionVX<uint8_t>(0b100011, {20, static_cast<uint8_t>(-20)}, 0xDEADBEEF00000103ULL, {2, static_cast<uint8_t>(-2)});
    // SEW = 8: scalar low 8 bits = 0xFF (treated as signed -1!). 20 % -1 = 0.
    // If treated as unsigned 255, 20 % 255 = 20 != 0.
    TestDivisionVX<uint8_t>(0b100011, {20, static_cast<uint8_t>(-20), 0x80}, 0xAAAAAAAAAAAA01FFULL, {0, 0, 0});

    // SEW = 16: scalar low 16 bits = 7 signed
    TestDivisionVX<uint16_t>(0b100011, {1000, static_cast<uint16_t>(-1000)}, 0xCAFEBABE00010007ULL, {6, static_cast<uint16_t>(-6)});
    // SEW = 16: scalar low 16 bits = 0xFFFF (treated as signed -1!). 100 % -1 = 0
    TestDivisionVX<uint16_t>(0b100011, {100, static_cast<uint16_t>(-100), 0x8000}, 0xCAFEBABE0001FFFFULL, {0, 0, 0});

    // SEW = 32: scalar low 32 bits = 10 signed
    TestDivisionVX<uint32_t>(0b100011, {100005, static_cast<uint32_t>(-100005)}, 0xFEEDFACE0000000AULL, {5, static_cast<uint32_t>(-5)});
    // SEW = 32: scalar low 32 bits = 0xFFFFFFFF (treated as signed -1!). 1000 % -1 = 0
    TestDivisionVX<uint32_t>(0b100011, {1000, static_cast<uint32_t>(-1000), 0x80000000u}, 0xFEEDFACEFFFFFFFFULL, {0, 0, 0});

    // SEW = 64: full 64-bit scalar (1000ULL)
    TestDivisionVX<uint64_t>(0b100011, {1000000000007ULL, static_cast<uint64_t>(-1000000000007LL)}, 1000ULL,
        {7, static_cast<uint64_t>(-7)});
}

TEST(VectorArithmeticTest, VectorDivisionMaskingAndPolicies)
{
    // Test 1: vdiv.vv masked with mu (mask-undisturbed, inactive elements retain original destination value)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vdiv.vv v3, v1, v2, v0.t (vm = 0, funct3 = 0b010, ta = false, ma = false)
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(32, false, false),
            EncodeVectorVV(0b100001, 3, 1, 2, 0, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 4);
        processor.Step(); // configure vtype/vl

        // Mask in v0: bit 0 = 0, bit 1 = 1, bit 2 = 0, bit 3 = 1 -> 0b00001010
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00001010);

        WriteElements<uint32_t>(processor.registers_, 1, {100, 100, 100, static_cast<uint32_t>(-80)});
        WriteElements<uint32_t>(processor.registers_, 2, {5, 5, 5, 4});
        WriteElements<uint32_t>(processor.registers_, 3, {111, 222, 333, 444});

        processor.Step();

        // Element 0: inactive -> retains 111
        // Element 1: active -> 100 / 5 = 20
        // Element 2: inactive -> retains 333
        // Element 3: active -> -80 / 4 = -20 = 0xFFFFFFEC
        ExpectElements<uint32_t>(processor.registers_, 3, {111u, 20u, 333u, static_cast<uint32_t>(-20)});
    }

    // Test 2: vrem.vx masked with ma (mask-agnostic, inactive elements filled with 1s)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vrem.vx v3, v1, x2, v0.t (vm = 0, funct3 = 0b110, ta = false, ma = true)
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b100011, 3, 1, 2, 0, 0b110), 10, 2, false, true);

        // Mask in v0: bit 0 = 1, bit 1 = 0, bit 2 = 1, bit 3 = 0 -> 0b00000101
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0b00000101);

        WriteElements<uint32_t>(processor.registers_, 1, {23, 34, 47, 58});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Element 0: active -> 23 % 10 = 3
        // Element 1: inactive, ma -> 0xFFFFFFFF
        // Element 2: active -> 47 % 10 = 7
        // Element 3: inactive, ma -> 0xFFFFFFFF
        ExpectElements<uint32_t>(processor.registers_, 3, {3u, 0xFFFFFFFFu, 7u, 0xFFFFFFFFu});
    }
}

TEST(VectorArithmeticTest, VectorDivisionTailPolicy)
{
    // Test 1: tail-agnostic (ta = true) for vdivu.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vdivu.vv v3, v1, v2 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = true (tail-agnostic)
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(32, true, false),
            EncodeVectorVV(0b100000, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 2);
        processor.Step(); // configure vtype/vl

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200, 10, 20});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 10, 20});
        WriteElements<uint32_t>(processor.registers_, 3, {111, 222, 333, 444});

        processor.Step();

        // Elements 0, 1 active: 100 / 10 = 10, 200 / 20 = 10
        // Tail elements 2, 3: ta -> 0xFFFFFFFF
        ExpectElements<uint32_t>(processor.registers_, 3, {10u, 10u, 0xFFFFFFFFu, 0xFFFFFFFFu});
    }

    // Test 2: tail-undisturbed (ta = false) for vremu.vx
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vremu.vx v3, v1, x2 with vl = 2 < VLMAX (VLMAX = 4 for SEW=32)
        // ta = false (tail-undisturbed)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100010, 3, 1, 2, 1, 0b110), 10, 2, false, false);

        WriteElements<uint32_t>(processor.registers_, 1, {55, 66, 77, 88});
        WriteElements<uint32_t>(processor.registers_, 3, {100, 200, 300, 400});

        processor.Step();

        // Elements 0, 1 active: 55 % 10 = 5, 66 % 10 = 6
        // Tail elements 2, 3: undisturbed -> retain 300, 400
        ExpectElements<uint32_t>(processor.registers_, 3, {5u, 6u, 300u, 400u});
    }
}

TEST(VectorArithmeticTest, VectorDivisionVstart)
{
    // Test 1: 0 < vstart < vl with vdiv.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b100001, 3, 1, 2, 1, 0b010));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200, 80, 90});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 4, 3});
        WriteElements<uint32_t>(processor.registers_, 3, {111, 222, 333, 444});

        processor.Step();

        // Elements 0, 1: prestart undisturbed -> retain 111, 222
        // Elements 2, 3: active -> 80 / 4 = 20, 90 / 3 = 30
        // vstart reset to 0
        ExpectElements<uint32_t>(processor.registers_, 3, {111u, 222u, 20u, 30u});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 2: vstart == vl with vdiv.vx -> no body elements executed, destination undisturbed, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100001, 3, 1, 2, 1, 0b110), 10);
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        WriteElements<uint32_t>(processor.registers_, 1, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 3: vstart > vl with vrem.vv -> executes no elements, destination unchanged, vstart reset to 0
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100011, 3, 1, 2, 1, 0b010));
        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // Destination unchanged
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, VectorDivisionAliasingAndHazardSafety)
{
    // Test 1: vd == vs2 (in-place division)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vdiv.vx v1, v1, x2 (vd = 1, vs2 = 1, rs1 = 2)
        LoadVectorVXProgram(processor, 32, 2, EncodeVectorVX(0b100001, 1, 1, 2, 1, 0b110), 3);
        WriteElements<uint32_t>(processor.registers_, 1, {30, 60});

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 0), 10u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(1, 1), 20u);
    }

    // Test 2: vd == vs1 with vdiv.vv (destination overwrites vs1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vdiv.vv v2, v1, v2 (vd = 2, vs2 = 1, vs1 = 2)
        LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100001, 2, 1, 2, 1, 0b010));
        WriteElements<uint32_t>(processor.registers_, 1, {50, 60});
        WriteElements<uint32_t>(processor.registers_, 2, {5, 6});

        processor.Step();

        // 50 / 5 = 10, 60 / 6 = 10
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 0), 10u);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(2, 1), 10u);
    }

    // Test 3: vd == v0 with vm = 0 (destination is mask register)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // vdiv.vx v0, v1, x2, v0.t (vd = 0, vs2 = 1, rs1 = 2, vm = 0)
        LoadVectorVXProgram(processor, 32, 4, EncodeVectorVX(0b100001, 0, 1, 2, 0, 0b110), 10, 2, false, false);

        // Initial mask in v0: bit 0=1, bit 1=1, bit 2=0, bit 3=1 -> 0x0B
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0B);

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200, 300, 400});
        processor.registers_.WriteVregElement<uint32_t>(0, 1, 100);
        processor.registers_.WriteVregElement<uint32_t>(0, 2, 200);
        processor.registers_.WriteVregElement<uint32_t>(0, 3, 300);

        processor.Step();

        // Element 0: active -> 100 / 10 = 10
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 0), 10u);
        // Element 1: active -> 200 / 10 = 20
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 1), 20u);
        // Element 2: inactive (mask bit 2 was 0) -> retains 200
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 2), 200u);
        // Element 3: active -> 400 / 10 = 40
        EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(0, 3), 40u);
    }
}

TEST(VectorArithmeticTest, VectorDivisionUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vdiv.vv v3, v1, v2
    LoadVectorProgram(processor, 32, 2, EncodeVectorVV(0b100001, 3, 1, 2, 1, 0b010));
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);

    WriteElements<uint32_t>(processor.registers_, 1, {100, 200});
    WriteElements<uint32_t>(processor.registers_, 2, {10, 4});
    WriteElements<uint32_t>(processor.registers_, 3, {555, 666});

    // Step vdiv.vv: element 0 prestart preserved (555), element 1 -> 200 / 4 = 50
    processor.Step();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 555u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 50u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

    // Undo -> restores element 1 to 666, vstart to 1
    processor.Undo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 555u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 666u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1u);

    // Redo -> restores element 1 to 50, vstart to 0
    processor.Redo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 555u);
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 1), 50u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

TEST(VectorArithmeticTest, VstartGreaterThanOrEqualToVLTailAgnosticAndUndoRedo)
{
    // Test 1: vstart == vl with tail-agnostic (ta = true)
    // Destination must remain unchanged (no tail elements overwritten),
    // vstart must become zero, and Undo/Redo must work correctly.
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // SEW = 32, AVL = 2 (vl = 2 < VLMAX = 4), ta = true, ma = false
        // vdiv.vv v3, v1, v2
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(32, true, false),
            EncodeVectorVV(0b100001, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 2);
        processor.Step(); // configure vtype/vl

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2); // vstart == vl

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200, 300, 400});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // When vstart >= vl, no elements (including tail elements) are updated
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart is reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

        // Undo -> destination still unchanged, vstart restored to 2
        processor.Undo();
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 2u);

        // Redo -> destination still unchanged, vstart reset to 0
        processor.Redo();
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test 2: vstart > vl with tail-agnostic (ta = true)
    // Destination must remain unchanged (no tail elements overwritten),
    // vstart must become zero, and Undo/Redo must work correctly.
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // SEW = 32, AVL = 2 (vl = 2 < VLMAX = 4), ta = true, ma = false
        // vrem.vv v3, v1, v2
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(32, true, false),
            EncodeVectorVV(0b100011, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 2);
        processor.Step(); // configure vtype/vl

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3); // vstart > vl

        WriteElements<uint32_t>(processor.registers_, 1, {100, 200, 300, 400});
        WriteElements<uint32_t>(processor.registers_, 2, {10, 20, 30, 40});
        WriteElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});

        processor.Step();

        // When vstart >= vl, no elements (including tail elements) are updated
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        // vstart is reset to 0
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

        // Undo -> destination still unchanged, vstart restored to 3
        processor.Undo();
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 3u);

        // Redo -> destination still unchanged, vstart reset to 0
        processor.Redo();
        ExpectElements<uint32_t>(processor.registers_, 3, {77, 88, 99, 111});
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorComparisonTest, AllComparisonInstructionsVV)
{
    // SEW = 8: 16 elements (VLMAX = 16, avl = 16)
    const std::vector<uint8_t> vs2_vals = {
        10, 20, 246, 236, 0, 127, 128, 50,
        10, 20, 246, 236, 0, 127, 128, 50
    };
    const std::vector<uint8_t> vs1_vals = {
        10, 30, 246,  10, 0, 128, 127, 206,
        15, 10,   0, 236, 255, 100, 156, 50
    };

    struct VVCase
    {
        uint32_t funct6;
        uint8_t expected_b0;
        uint8_t expected_b1;
    };

    const VVCase cases[] = {
        {0b011000, 0x15, 0x88}, // vmseq.vv
        {0b011001, 0xEA, 0x77}, // vmsne.vv
        {0b011010, 0xA2, 0x51}, // vmsltu.vv
        {0b011011, 0x4A, 0x45}, // vmslt.vv
        {0b011100, 0xB7, 0xD9}, // vmsleu.vv
        {0b011101, 0x5F, 0xCD}, // vmsle.vv
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(
            processor,
            8,
            16,
            EncodeVectorVV(c.funct6, 3, 1, 2, 1, 0b000));

        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals);
        WriteElements<uint8_t>(processor.registers_, 2, vs1_vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0xAA);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), c.expected_b1);
        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0xAA)
                << "Byte " << b << " beyond VLMAX was unexpectedly modified";
        }
    }

    // SEW = 16: 8 elements (VLMAX = 8, avl = 8)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        const std::vector<uint16_t> vs2_vals = {
            100, 200, static_cast<uint16_t>(-500), static_cast<uint16_t>(-1000),
            0, 32767, static_cast<uint16_t>(-32768), 5000
        };
        const std::vector<uint16_t> vs1_vals = {
            100, 300, static_cast<uint16_t>(-500), 1000,
            0, static_cast<uint16_t>(-32768), 32767, static_cast<uint16_t>(-5000)
        };

        LoadVectorProgram(processor, 16, 8, EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000));
        WriteElements<uint16_t>(processor.registers_, 1, vs2_vals);
        WriteElements<uint16_t>(processor.registers_, 2, vs1_vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x15);
        for (unsigned int b = 1; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0);
    }

    // SEW = 32: 4 elements (VLMAX = 4, avl = 4)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        const std::vector<uint32_t> vs2_vals = {
            1000u, static_cast<uint32_t>(-5000), 0u, 100000u
        };
        const std::vector<uint32_t> vs1_vals = {
            1000u, 5000u, static_cast<uint32_t>(-1), 50000u
        };

        LoadVectorProgram(processor, 32, 4, EncodeVectorVV(0b011011, 3, 1, 2, 1, 0b000)); // vmslt.vv
        WriteElements<uint32_t>(processor.registers_, 1, vs2_vals);
        WriteElements<uint32_t>(processor.registers_, 2, vs1_vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x0F, 0x02);
    }

    // SEW = 64: 2 elements (VLMAX = 2, avl = 2) - vmseq.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        const std::vector<uint64_t> vs2_vals = {
            static_cast<uint64_t>(-100LL), 500ULL
        };
        const std::vector<uint64_t> vs1_vals = {
            200ULL, 500ULL
        };

        LoadVectorProgram(processor, 64, 2, EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)); // vmseq.vv
        WriteElements<uint64_t>(processor.registers_, 1, vs2_vals);
        WriteElements<uint64_t>(processor.registers_, 2, vs1_vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x03, 0x02);
    }

    // SEW = 64: 2 elements (VLMAX = 2, avl = 2) - vmslt.vv
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        const std::vector<uint64_t> vs2_vals = {
            static_cast<uint64_t>(-100LL), 500ULL
        };
        const std::vector<uint64_t> vs1_vals = {
            200ULL, 500ULL
        };

        LoadVectorProgram(processor, 64, 2, EncodeVectorVV(0b011011, 3, 1, 2, 1, 0b000)); // vmslt.vv
        WriteElements<uint64_t>(processor.registers_, 1, vs2_vals);
        WriteElements<uint64_t>(processor.registers_, 2, vs1_vals);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x03, 0x01);
    }
}

TEST(VectorComparisonTest, AllComparisonInstructionsVX)
{
    // SEW = 8: 16 elements against scalar 30
    const std::vector<uint8_t> vs2_vals = {
        0, 5, 10, 15, 20, 25, 30, 35,
        40, 45, 50, 55, 60, 65, 70, 75
    };

    struct VXCase
    {
        uint32_t funct6;
        uint8_t expected_b0;
        uint8_t expected_b1;
    };

    const VXCase cases[] = {
        {0b011000, 0x40, 0x00}, // vmseq.vx (i=6)
        {0b011001, 0xBF, 0xFF}, // vmsne.vx
        {0b011010, 0x3F, 0x00}, // vmsltu.vx (i=0..5)
        {0b011011, 0x3F, 0x00}, // vmslt.vx (i=0..5)
        {0b011100, 0x7F, 0x00}, // vmsleu.vx (i=0..6)
        {0b011101, 0x7F, 0x00}, // vmsle.vx (i=0..6)
        {0b011110, 0x80, 0xFF}, // vmsgtu.vx (i=7..15)
        {0b011111, 0x80, 0xFF}, // vmsgt.vx (i=7..15)
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(
            processor,
            8,
            16,
            EncodeVectorVX(c.funct6, 3, 1, 2, 1, 0b100));

        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals);
        processor.registers_.WriteGpr(2, 30);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), c.expected_b1);
    }

    // Negative scalar test at SEW=16: x2 = -20
    const std::vector<uint16_t> vs2_sew16 = {
        10, static_cast<uint16_t>(-30), static_cast<uint16_t>(-20),
        static_cast<uint16_t>(-10), 0, 5, static_cast<uint16_t>(-1), 20
    };

    // vmslt.vx: signed < -20 -> only -30 (i=1) -> 0b00000010 = 0x02
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 16, 8, EncodeVectorVX(0b011011, 3, 1, 2, 1, 0b100));
        WriteElements<uint16_t>(processor.registers_, 1, vs2_sew16);
        processor.registers_.WriteGpr(2, static_cast<uint64_t>(-20LL));
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x02);
    }

    // vmsgt.vx: signed > -20 -> 10 (i=0), -10 (i=3), 0 (i=4), 5 (i=5), -1 (i=6), 20 (i=7) -> 0xF9
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 16, 8, EncodeVectorVX(0b011111, 3, 1, 2, 1, 0b100));
        WriteElements<uint16_t>(processor.registers_, 1, vs2_sew16);
        processor.registers_.WriteGpr(2, static_cast<uint64_t>(-20LL));
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xF9);
    }

    // SEW=64, avl=2, scalar = 100
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(processor, 64, 2, EncodeVectorVX(0b011110, 3, 1, 2, 1, 0b100)); // vmsgtu.vx
        WriteElements<uint64_t>(processor.registers_, 1, {50ULL, 150ULL});
        processor.registers_.WriteGpr(2, 100);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x03, 0x02);
    }
}

TEST(VectorComparisonTest, AllComparisonInstructionsVI)
{
    const std::vector<uint8_t> vs2_vals = {
        0, 1, 2, 3, 4, 5, 6, 7
    };

    struct VICase
    {
        uint32_t funct6;
        uint8_t expected_b0;
    };

    const VICase cases[] = {
        {0b011000, 0x08}, // vmseq.vi (i=3)
        {0b011001, 0xF7}, // vmsne.vi
        {0b011100, 0x0F}, // vmsleu.vi (i=0..3)
        {0b011101, 0x0F}, // vmsle.vi (i=0..3)
        {0b011110, 0xF0}, // vmsgtu.vi (i=4..7)
        {0b011111, 0xF0}, // vmsgt.vi (i=4..7)
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        LoadVectorProgram(
            processor,
            8,
            8,
            EncodeVectorVI(c.funct6, 3, 1, 3, 1));

        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0);

        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0);
    }
}

TEST(VectorComparisonTest, ImmediateRangeAndSignExtensionVI)
{
    // Test A1: vmsleu.vi with simm5 = -1 SEW=8
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVI(0b011100, 3, 1, -1, 1));
        WriteElements<uint8_t>(processor.registers_, 1, {0, 1, 50, 127, 128, 200, 254, 255});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    }

    // Test A2: vmsleu.vi with simm5 = -1 SEW=16
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 16, 8, EncodeVectorVI(0b011100, 3, 1, -1, 1));
        WriteElements<uint16_t>(processor.registers_, 1, {0, 1, 500, 32767, 32768, 50000, 65534, 65535});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    }

    // Test A3: vmsleu.vi with simm5 = -1 SEW=32
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 32, 4, EncodeVectorVI(0b011100, 3, 1, -1, 1));
        WriteElements<uint32_t>(processor.registers_, 1, {0u, 100u, 0x80000000u, 0xFFFFFFFFu});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x0F, 0x0F);
    }

    // Test A4: vmsleu.vi with simm5 = -1 SEW=64
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 64, 2, EncodeVectorVI(0b011100, 3, 1, -1, 1));
        WriteElements<uint64_t>(processor.registers_, 1, {0ULL, 0xFFFFFFFFFFFFFFFFULL});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x03, 0x03);
    }

    // Test B: vmsgtu.vi with simm5 = -1
    // a > UINT_MAX is FALSE for all unsigned elements.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVI(0b011110, 3, 1, -1, 1));
        WriteElements<uint8_t>(processor.registers_, 1, {0, 1, 50, 127, 128, 200, 254, 255});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
    }

    // Test C: vmsle.vi with simm5 = -1 (signed a <= -1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVI(0b011101, 3, 1, -1, 1));
        WriteElements<uint8_t>(processor.registers_, 1, {
            static_cast<uint8_t>(-10), static_cast<uint8_t>(-5),
            static_cast<uint8_t>(-2), static_cast<uint8_t>(-1),
            0, 1, 2, 10
        });
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x0F);
    }

    // Test D: vmsgt.vi with simm5 = -1 (signed a > -1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVI(0b011111, 3, 1, -1, 1));
        WriteElements<uint8_t>(processor.registers_, 1, {
            static_cast<uint8_t>(-10), static_cast<uint8_t>(-5),
            static_cast<uint8_t>(-2), static_cast<uint8_t>(-1),
            0, 1, 2, 10
        });
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xF0);
    }

    // Test E1: Boundary immediate simm5 = -16
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 4, EncodeVectorVI(0b011101, 3, 1, -16, 1)); // vmsle.vi
        WriteElements<uint8_t>(processor.registers_, 1, {
            static_cast<uint8_t>(-20), static_cast<uint8_t>(-16),
            static_cast<uint8_t>(-15), 0
        });
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x0F, 0x03);
    }

    // Test E2: Boundary immediate simm5 = 15
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 4, EncodeVectorVI(0b011101, 3, 1, 15, 1)); // vmsle.vi
        WriteElements<uint8_t>(processor.registers_, 1, {10, 15, 16, 20});
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x0F, 0x03);
    }
}

TEST(VectorComparisonTest, ScalarTruncationAndSignednessVX)
{
    // SEW = 8: scalar has nonzero upper bits 0xAAAAAAAAAAAA01FFULL.
    // Truncated 8-bit value is 0xFF (-1 signed, 255 unsigned).
    const uint64_t scalar8 = 0xAAAAAAAAAAAA01FFULL;
    const std::vector<uint8_t> vs2_vals8 = {
        0, 100, 254, 255,
        static_cast<uint8_t>(-2), static_cast<uint8_t>(-1), 50, 128
    };

    // vmseq.vx: matches 0xFF (255 and -1, indices 3 and 5) -> 0b00101000 = 0x28
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVX(0b011000, 3, 1, 2, 1, 0b100));
        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals8);
        processor.registers_.WriteGpr(2, scalar8);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x28);
    }

    // vmsltu.vx: unsigned < 255 -> indices 0, 1, 2, 4 (-2 is 254), 6, 7 -> 0b11010111 = 0xD7
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVX(0b011010, 3, 1, 2, 1, 0b100));
        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals8);
        processor.registers_.WriteGpr(2, scalar8);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xD7);
    }

    // vmslt.vx: signed < -1 -> -2 (i=2, 4) and -128 (i=7) -> 0b10010100 = 0x94
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVX(0b011011, 3, 1, 2, 1, 0b100));
        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals8);
        processor.registers_.WriteGpr(2, scalar8);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x94);
    }

    // vmsgtu.vx: unsigned > 255 -> none (0x00)
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVX(0b011110, 3, 1, 2, 1, 0b100));
        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals8);
        processor.registers_.WriteGpr(2, scalar8);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
    }

    // vmsgt.vx: signed > -1 -> elements >= 0: 0 (i=0), 100 (i=1), 50 (i=6) -> 0b01000011 = 0x43
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVX(0b011111, 3, 1, 2, 1, 0b100));
        WriteElements<uint8_t>(processor.registers_, 1, vs2_vals8);
        processor.registers_.WriteGpr(2, scalar8);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x43);
    }

    // SEW = 16: scalar 0xAAAAAAAAAAAA8000ULL. Low 16-bit is 0x8000 (-32768 signed, 32768 unsigned).
    const uint64_t scalar16 = 0xAAAAAAAAAAAA8000ULL;
    const std::vector<uint16_t> vs2_vals16 = {
        0, 100, 32767, 32768,
        static_cast<uint16_t>(-32768), static_cast<uint16_t>(-1), 5000, 10000
    };

    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 16, 8, EncodeVectorVX(0b011000, 3, 1, 2, 1, 0b100));
        WriteElements<uint16_t>(processor.registers_, 1, vs2_vals16);
        processor.registers_.WriteGpr(2, scalar16);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x18);
    }

    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 16, 8, EncodeVectorVX(0b011011, 3, 1, 2, 1, 0b100));
        WriteElements<uint16_t>(processor.registers_, 1, vs2_vals16);
        processor.registers_.WriteGpr(2, scalar16);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
    }

    // SEW = 32: scalar 0xAAAAAAAA80000000ULL. Low 32-bit is 0x80000000.
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        const uint64_t scalar32 = 0xAAAAAAAA80000000ULL;
        const std::vector<uint32_t> vs2_vals32 = {
            0u, 0x80000000u, 0x7FFFFFFFu, 0xFFFFFFFFu
        };

        LoadVectorProgram(processor, 32, 4, EncodeVectorVX(0b011000, 3, 1, 2, 1, 0b100));
        WriteElements<uint32_t>(processor.registers_, 1, vs2_vals32);
        processor.registers_.WriteGpr(2, scalar32);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & 0x0F, 0x02);
    }
}

TEST(VectorComparisonTest, MaskDestinationTailPolicyAlwaysTailAgnostic)
{
    // Test A: SEW=8, avl = 10 < VLMAX = 16.
    // Case 1: vta = false (tail undisturbed in vtype).
    // Tail bits in [vl, VLMAX) = [10, 16) MUST still be set to 1!
    // Bits beyond VLMAX (bytes 2..15) must remain undisturbed (0x00).
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000) // vmseq.vv v3, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 10);
        processor.Step();

        std::vector<uint8_t> vals(16, 42);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Active bits 0..9: 1
        // Tail bits 10..15: 1 (always tail-agnostic!)
        // Byte 0: 0xFF (bits 0..7)
        // Byte 1: 0xFF (bits 8..9 are 1, tail bits 10..15 are 1)
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);

        // Bytes 2..15 beyond VLMAX MUST remain undisturbed 0x00
        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00)
                << "Byte " << b << " beyond VLMAX was unexpectedly modified under vta=false";
        }
    }

    // Case 2: Active elements all UNEQUAL (bits 0..9 are 0), vta = false
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 10);
        processor.Step();

        std::vector<uint8_t> vals1(16, 42);
        std::vector<uint8_t> vals2(16, 99);
        WriteElements<uint8_t>(processor.registers_, 1, vals1);
        WriteElements<uint8_t>(processor.registers_, 2, vals2);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Active bits 0..9: 0
        // Tail bits 10..15: 1
        // Byte 0: 0x00
        // Byte 1: bits 8..9 are 0, bits 10..15 are 1 -> 0b11111100 = 0xFC
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFC);

        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);
        }
    }

    // Case 3: Same with vta = true
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 10);
        processor.Step();

        std::vector<uint8_t> vals1(16, 42);
        std::vector<uint8_t> vals2(16, 99);
        WriteElements<uint8_t>(processor.registers_, 1, vals1);
        WriteElements<uint8_t>(processor.registers_, 2, vals2);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFC);

        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);
        }
    }

    // Test B: SEW=16, avl = 5 < VLMAX = 8.
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(16, false, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 5);
        processor.Step();

        std::vector<uint16_t> vals1(8, 10);
        std::vector<uint16_t> vals2(8, 20);
        WriteElements<uint16_t>(processor.registers_, 1, vals1);
        WriteElements<uint16_t>(processor.registers_, 2, vals2);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xE0);
        for (unsigned int b = 1; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);
        }
    }
}

TEST(VectorComparisonTest, MaskingPoliciesUndisturbedAndAgnostic)
{
    // Part A1: vma = false (mask undisturbed), initial vd = 0x00.
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false), // tu, mu
            EncodeVectorVV(0b011000, 3, 1, 2, 0, 0b000) // vmseq.vv v3, v1, v2, v0.t (vm = 0)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 42);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);

        SetMask(processor.registers_, 0x55);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x55);
    }

    // Part A2: vma = false (mask undisturbed), initial vd = 0xAA (odd bits 1).
    // Active even bits become 1, inactive odd bits retain 1 -> 0xFF
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false), // tu, mu
            EncodeVectorVV(0b011000, 3, 1, 2, 0, 0b000) // vmseq.vv v3, v1, v2, v0.t (vm = 0)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 42);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);
        SetMask(processor.registers_, 0x55);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0xAA);

        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    }

    // Part B: vma = true (mask agnostic).
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, true), // tu, ma (mask agnostic)
            EncodeVectorVV(0b011000, 3, 1, 2, 0, 0b000) // vmseq.vv v3, v1, v2, v0.t
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        // Elements UNEQUAL: active even elements evaluate to 0
        std::vector<uint8_t> vals1(16, 10);
        std::vector<uint8_t> vals2(16, 20);
        WriteElements<uint8_t>(processor.registers_, 1, vals1);
        WriteElements<uint8_t>(processor.registers_, 2, vals2);
        SetMask(processor.registers_, 0x55);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Active even bits: 0 (unequal)
        // Inactive odd bits: 1 (mask-agnostic writes 1)
        // Result byte 0: 0b10101010 = 0xAA
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xAA);
    }
}

TEST(VectorComparisonTest, DestinationSourceRegisterOverlap)
{
    // Subtest A: vd = v0, unmasked (vm = 1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 0, 1, 2, 1, 0b000) // vmseq.vv v0, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals1 = {1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0};
        std::vector<uint8_t> vals2 = {1, 9, 3, 9, 5, 9, 7, 9, 0, 0, 0, 0, 0, 0, 0, 0};
        WriteElements<uint8_t>(processor.registers_, 1, vals1);
        WriteElements<uint8_t>(processor.registers_, 2, vals2);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(0, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 0), 0x55);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 1), 0xFF);
    }

    // Subtest B: vd = v0, masked (vm = 0), vma = false (mask undisturbed)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 0, 1, 2, 0, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 42);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(0, b, 0x00);
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0F);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 0), 0x0F);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 1), 0xFF);
    }

    // Subtest C: vd = v0, masked (vm = 0), vma = true (mask agnostic)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, true),
            EncodeVectorVV(0b011000, 0, 1, 2, 0, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals1(16, 1);
        std::vector<uint8_t> vals2(16, 2);
        WriteElements<uint8_t>(processor.registers_, 1, vals1);
        WriteElements<uint8_t>(processor.registers_, 2, vals2);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(0, b, 0x00);
        processor.registers_.WriteVregElement<uint8_t>(0, 0, 0x0F);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 0), 0xF0);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 1), 0xFF);
    }

    // Subtest D: vd = vs1
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 2, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 5);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(2, 0), 0xFF);
    }

    // Subtest E: vd = vs2
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 1, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 5);
        WriteElements<uint8_t>(processor.registers_, 1, vals);
        WriteElements<uint8_t>(processor.registers_, 2, vals);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0xFF);
    }

    // Subtest F: vd = vs1 = vs2
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011000, 1, 1, 1, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        std::vector<uint8_t> vals(16, 77);
        WriteElements<uint8_t>(processor.registers_, 1, vals);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0xFF);
    }
}

TEST(VectorComparisonTest, VstartBoundaries)
{
    // Subtest A: 0 < vstart < vl (vstart = 2, vl = 6, VLMAX = 8 at SEW=16)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(16, false, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 6);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        std::vector<uint16_t> vals1(8, 10);
        std::vector<uint16_t> vals2(8, 20);
        WriteElements<uint16_t>(processor.registers_, 1, vals1);
        WriteElements<uint16_t>(processor.registers_, 2, vals2);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);
        processor.registers_.WriteVregElement<uint8_t>(3, 0, 0x03);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xC3);
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Subtest B: vstart == vl
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false), // ta = true
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 4);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 4);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, static_cast<uint8_t>(b + 1));

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), static_cast<uint8_t>(b + 1));
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Subtest C: vstart > vl
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 4);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 6);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, static_cast<uint8_t>(b + 10));

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), static_cast<uint8_t>(b + 10));
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorComparisonTest, UndoRedoCorrectness)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(8, false, false),
        EncodeVectorVV(0b011000, 3, 1, 2, 1, 0b000)
    };
    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 8);

    for (unsigned int b = 0; b < 16; ++b)
        processor.registers_.WriteVregElement<uint8_t>(3, b, 0x33);

    std::vector<uint8_t> vals(16, 50);
    WriteElements<uint8_t>(processor.registers_, 1, vals);
    WriteElements<uint8_t>(processor.registers_, 2, vals);

    processor.Step(); // vsetvli
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VL), 8u);

    processor.Step(); // vmseq.vv
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);

    // Undo -> destination restored to initial 0x33
    processor.Undo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x33);

    // Redo -> destination re-applies 0xFF
    processor.Redo();
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

// ============================================================================
// RVV 1.0 Batch 2: Mask Logical and Prefix Instructions Tests
// ============================================================================

TEST(VectorArithmeticTest, MaskLogicalBasic)
{
    struct MaskLogicalCase
    {
        uint32_t funct6;
        const char *name;
        uint8_t expected_b0;
    };

    // vs2 byte 0 = 0b11001010 (0xCA)
    // vs1 byte 0 = 0b10100110 (0xA6)
    // vmand:  0xCA & 0xA6 = 0x82
    // vmandn: 0xCA & ~0xA6 = 0xCA & 0x59 = 0x48
    // vmor:   0xCA | 0xA6 = 0xEE
    // vmorn:  0xCA | ~0xA6 = 0xCA | 0x59 = 0xDB
    // vmxor:  0xCA ^ 0xA6 = 0x6C
    const MaskLogicalCase cases[] = {
        {0b011001, "vmand.mm", 0x82},
        {0b011000, "vmandn.mm", 0x48},
        {0b011010, "vmor.mm", 0xEE},
        {0b011100, "vmorn.mm", 0xDB},
        {0b011011, "vmxor.mm", 0x6C},
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // SEW = 8, avl = 8 (VL = 8, VLMAX = 16)
        LoadVectorProgram(processor, 8, 8, EncodeVectorVV(c.funct6, 3, 1, 2, 1, 0b010));

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xCA); // vs2
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xA6); // vs1

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Byte 0: active elements [0, 8)
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0)
            << "Mismatch for " << c.name;
        // Byte 1: tail elements [8, 16) are set to 1 (mask destination tail agnostic)
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF)
            << "Tail mismatch for " << c.name;
        // Bytes 2..15: beyond VLMAX must remain undisturbed (0x00)
        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00)
                << "Byte " << b << " beyond VLMAX was modified for " << c.name;
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, MaskPrefixBasic)
{
    // First set bit at index 3 in vs2 (0b00101000 = 0x28)
    // vmsbf.m (vs1 = 1): set before first -> bits 0..2 = 1, rest 0 -> 0x07
    // vmsof.m (vs1 = 2): set only first   -> bit 3 = 1, rest 0    -> 0x08
    // vmsif.m (vs1 = 3): set up to first  -> bits 0..3 = 1, rest 0 -> 0x0F
    struct PrefixCase
    {
        uint32_t vs1;
        const char *name;
        uint8_t expected_b0;
    };

    const PrefixCase cases[] = {
        {0b00001, "vmsbf.m", 0x07},
        {0b00010, "vmsof.m", 0x08},
        {0b00011, "vmsif.m", 0x0F},
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        // SEW = 8, avl = 8 (VL = 8, VLMAX = 16)
        LoadVectorProgram(processor, 8, 8, EncodeVectorVV(0b010100, 3, 1, c.vs1, 1, 0b010));

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x28); // vs2

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0)
            << "Mismatch for " << c.name;
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF)
            << "Tail mismatch for " << c.name;
        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00)
                << "Byte " << b << " beyond VLMAX was modified for " << c.name;
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // No set bit in vs2 (0x00):
    // vmsbf.m -> all 1 (0xFF)
    // vmsof.m -> all 0 (0x00)
    // vmsif.m -> all 1 (0xFF)
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVV(0b010100, 3, 1, 1, 1, 0b010)); // vmsbf.m
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x00);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    }
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVV(0b010100, 3, 1, 2, 1, 0b010)); // vmsof.m
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x00);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0xFF);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x00);
    }
    {
        Kites::setupVmStateDirectory();
        Processor processor;
        LoadVectorProgram(processor, 8, 8, EncodeVectorVV(0b010100, 3, 1, 3, 1, 0b010)); // vmsif.m
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x00);
        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
    }

    // Masked prefix operation (vm = 0, v0.t)
    // Mask v0 = 0x55 (even bits active, odd bits masked off)
    // vs2 = 0x10 (bit 4 set) -> first active set bit is bit 4
    // vma = false (undisturbed), initial vd = 0xAA (odd bits 1)
    // Active even bits: bit 0 -> 1, bit 2 -> 1, bit 4 -> 0, bit 6 -> 0
    // Inactive odd bits: retain initial vd odd bits (1) -> result byte 0 = 0xAF
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false), // tu, mu
            EncodeVectorVV(0b010100, 3, 1, 1, 0, 0b010) // vmsbf.m v3, v1, v0.t (vm = 0)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x10); // vs2
        SetMask(processor.registers_, 0x55); // v0

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0xAA);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xAF);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);
    }

    // Masked prefix with vma = true (mask agnostic) -> inactive bits written with 1
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, true), // tu, ma
            EncodeVectorVV(0b010100, 3, 1, 1, 0, 0b010) // vmsbf.m v3, v1, v0.t (vm = 0)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x10); // vs2
        SetMask(processor.registers_, 0x55); // v0

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Active even bits: bit 0 (1), bit 2 (1), bit 4 (0), bit 6 (0)
        // Inactive odd bits: written with 1 -> 0xAF
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xAF);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);
    }
}

TEST(VectorArithmeticTest, MaskedPrefixScanIgnoresInactiveSourceBits)
{
    // Regression test for masked prefix scan:
    // An early source bit is 1 but its corresponding v0 bit is 0 (inactive).
    // A later source bit is 1 and its corresponding v0 bit is 1 (active).
    // Verify that the later bit is treated as the first active set bit, and the early
    // inactive bit does NOT terminate the prefix scan.
    //
    // Setup (VL = 8):
    // vs2 = (1 << 1) | (1 << 3) = 0x0A (bit 1 is set, bit 3 is set)
    // v0  = ~static_cast<uint8_t>(1 << 1) = 0xFD (bit 1 is inactive; bits 0, 2..7 are active)
    // Initial vd = 0x00, vma = false (undisturbed), vta = false.
    //
    // Analysis:
    // i = 0: active, vs2[0]=0 -> vmsbf: 1, vmsof: 0, vmsif: 1
    // i = 1: inactive (v0[1]=0) -> ignored for prefix scan! vd[1] undisturbed (0)
    // i = 2: active, vs2[2]=0 -> vmsbf: 1, vmsof: 0, vmsif: 1
    // i = 3: active, vs2[3]=1 -> FIRST ACTIVE SET BIT!
    //        vmsbf: 0 (found_first = true)
    //        vmsof: 1 (found_first = true)
    //        vmsif: 1 (found_first = true)
    // i = 4..7: active, found_first is true -> all 0
    //
    // Expected vd byte 0:
    // vmsbf.m: bits 0, 2 set -> 0b00000101 = 0x05
    // vmsof.m: bit 3 set     -> 0b00001000 = 0x08
    // vmsif.m: bits 0, 2, 3 set -> 0b00001101 = 0x0D
    struct ScanCase
    {
        uint32_t vs1;
        const char *name;
        uint8_t expected_b0;
    };

    const ScanCase cases[] = {
        {0b00001, "vmsbf.m", 0x05},
        {0b00010, "vmsof.m", 0x08},
        {0b00011, "vmsif.m", 0x0D},
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false), // tu, mu
            EncodeVectorVV(0b010100, 3, 1, c.vs1, 0, 0b010) // vd=3, vs2=1, vs1, vm=0
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x0A); // vs2: bits 1 and 3 set
        SetMask(processor.registers_, 0xFD); // v0: bit 1 is 0 (inactive), all others active

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), c.expected_b0)
            << "Mismatch for " << c.name;
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF)
            << "Tail mismatch for " << c.name;
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, MaskLogicalVstartGTEVl)
{
    // When vstart >= vl, no destination bits are modified (neither body nor tail).
    // Instruction completes normally and resets vstart to 0.
    for (uint64_t vstart_val : {8ULL, 12ULL})
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010) // vmand.mm
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, vstart_val);

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xFF);
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xFF);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, static_cast<uint8_t>(b + 0x10));

        processor.Step();

        // No bits modified
        for (unsigned int b = 0; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), static_cast<uint8_t>(b + 0x10))
                << "Byte " << b << " modified when vstart=" << vstart_val << " >= vl=8";
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, MaskPrefixIllegalVstart)
{
    // Section 15.1: Nonzero vstart is illegal for prefix instructions, even when vl = 0.
    for (uint64_t avl : {8ULL, 0ULL})
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b010100, 3, 1, 1, 1, 0b010) // vmsbf.m
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, avl);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x55);

        processor.Step();

        // Destination must remain completely untouched
        for (unsigned int b = 0; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x55);
        }
        // Illegal instruction returns early without clearing vstart
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 2u);
    }
}

TEST(VectorArithmeticTest, MaskPrefixVlZero)
{
    // Section 15.1: If vstart == 0 and vl == 0, destination is completely unchanged (no tail writes applied).
    // Resets vstart to 0.
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(8, true, false), // ta = true
        EncodeVectorVV(0b010100, 3, 1, 1, 1, 0b010) // vmsbf.m
    };
    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 0); // avl = 0 -> vl = 0
    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VL), 0u);

    for (unsigned int b = 0; b < 16; ++b)
        processor.registers_.WriteVregElement<uint8_t>(3, b, 0x44);

    processor.Step();

    for (unsigned int b = 0; b < 16; ++b)
    {
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x44)
            << "Byte " << b << " was unexpectedly modified when vl=0";
    }
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

TEST(VectorArithmeticTest, MaskLogicalRejectsVmZero)
{
    // Mask logical instructions encoded with vm=0 are reserved encodings and rejected.
    Kites::setupVmStateDirectory();
    Processor processor;

    Kites::AssembledProgram program;
    program.text_buffer = {
        EncodeVsetvli(8, false, false),
        EncodeVectorVV(0b011001, 3, 1, 2, 0, 0b010) // vmand.mm with vm = 0 (reserved)
    };
    processor.LoadProgram(program);
    processor.registers_.WriteGpr(1, 8);
    processor.Step();

    processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xFF);
    processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xFF);

    for (unsigned int b = 0; b < 16; ++b)
        processor.registers_.WriteVregElement<uint8_t>(3, b, 0x77);

    processor.Step();

    for (unsigned int b = 0; b < 16; ++b)
    {
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x77);
    }
}

TEST(VectorArithmeticTest, MaskPrefixIllegalOverlap)
{
    // vd == vs2 is illegal for prefix instructions
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b010100, 1, 1, 1, 1, 0b010) // vmsbf.m v1, v1
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(1, b, 0x33);

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, b), 0x33);
    }

    // Masked prefix (vm = 0) with vd == v0 is illegal
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b010100, 0, 1, 1, 0, 0b010) // vmsbf.m v0, v1, v0.t
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(0, b, 0x55);

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, b), 0x55);
    }
}

TEST(VectorArithmeticTest, MaskMultiByteBoundaries)
{
    // VLEN = 128, LMUL = m1, SEW = 8 -> VLMAX = 16.
    // Test boundary lengths across multi-byte packed masks: VL in {1, 7, 8, 9, 15, 16}.
    for (unsigned int vl_target : {1u, 7u, 8u, 9u, 15u, 16u})
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false), // SEW=8, ta=true
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010) // vmand.mm v3, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, vl_target);
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VL), vl_target);

        // vs2 has all 1s across bytes 0 and 1
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xFF);
        processor.registers_.WriteVregElement<uint8_t>(1, 1, 0xFF);

        // vs1 has alternating pattern: byte 0 = 0xAA, byte 1 = 0x55
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xAA);
        processor.registers_.WriteVregElement<uint8_t>(2, 1, 0x55);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Verify active bits [0, vl_target)
        for (unsigned int i = 0; i < vl_target; ++i)
        {
            const unsigned int byte_idx = i / 8;
            const unsigned int bit_idx = i % 8;
            const uint8_t vs1_byte = (byte_idx == 0) ? 0xAA : 0x55;
            const bool expected_bit = ((vs1_byte >> bit_idx) & 1) != 0;
            const bool actual_bit = ((processor.registers_.ReadVregElement<uint8_t>(3, byte_idx) >> bit_idx) & 1) != 0;
            EXPECT_EQ(actual_bit, expected_bit)
                << "Active bit mismatch at index " << i << " for VL=" << vl_target;
        }

        // Verify tail bits [vl_target, 16) are set to 1 (tail agnostic)
        for (unsigned int i = vl_target; i < 16; ++i)
        {
            const unsigned int byte_idx = i / 8;
            const unsigned int bit_idx = i % 8;
            const bool actual_bit = ((processor.registers_.ReadVregElement<uint8_t>(3, byte_idx) >> bit_idx) & 1) != 0;
            EXPECT_TRUE(actual_bit)
                << "Tail bit at index " << i << " was not set to 1 for VL=" << vl_target;
        }

        // Verify bytes beyond VLMAX (bytes 2..15) remain undisturbed 0x00
        for (unsigned int b = 2; b < 16; ++b)
        {
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00)
                << "Byte " << b << " beyond VLMAX was modified for VL=" << vl_target;
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Prefix instruction cross-byte test with set bit at index 8 (byte 1, bit 0)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b010100, 3, 1, 1, 1, 0b010) // vmsbf.m v3, v1
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 9); // VL = 9
        processor.Step();

        // vs2: bit 8 is set (byte 1, bit 0)
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x00);
        processor.registers_.WriteVregElement<uint8_t>(1, 1, 0x01);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Byte 0: bits 0..7 are all before first set bit -> 0xFF
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
        // Byte 1: bit 8 is first set bit -> 0; tail bits 9..15 -> 1s => 0b11111110 = 0xFE
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFE);
        for (unsigned int b = 2; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);
    }
}

TEST(VectorArithmeticTest, MaskLogicalSEWIndependence)
{
    // Mask logical instructions operate bitwise on mask registers regardless of SEW.
    struct SEWCase
    {
        unsigned int sew_bits;
        unsigned int vlmax;
    };

    const SEWCase cases[] = {
        {8, 16},
        {16, 8},
        {32, 4},
        {64, 2},
    };

    for (const auto &c : cases)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(c.sew_bits, false, false),
            EncodeVectorVV(0b011011, 3, 1, 2, 1, 0b010) // vmxor.mm v3, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, c.vlmax); // VL = VLMAX
        processor.Step();
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VL), c.vlmax);

        // vs2 = 0xCA, vs1 = 0xA6 -> XOR = 0x6C
        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xCA);
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xA6);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // Active mask elements [0, vlmax) should match bits from 0x6C
        const uint8_t mask_bits = static_cast<uint8_t>((1u << c.vlmax) - 1u);
        const uint8_t expected_b0 = 0x6C & mask_bits;
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0) & mask_bits, expected_b0)
            << "Mismatch at SEW=" << c.sew_bits;

        // Bits beyond VLMAX must remain undisturbed 0x00
        for (unsigned int b = 1; b < 16; ++b)
        {
            if (c.vlmax <= 8 && b >= 1)
                EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);
        }
        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, MaskPrefixMaskingPoliciesComprehensive)
{
    // Test all prefix instructions under vma=false (undisturbed) and vma=true (agnostic)
    // with contiguous, alternating, empty, and full masks.
    Kites::setupVmStateDirectory();

    // 1. All elements inactive: v0 = 0x00, vs2 = 0x02 (bit 1 set)
    // vmsbf.m under vma=false: destination retains initial vd
    {
        Processor processor;
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false), // vma = false (undisturbed)
            EncodeVectorVV(0b010100, 3, 1, 1, 0, 0b010) // vmsbf.m v3, v1, v0.t
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x02);
        SetMask(processor.registers_, 0x00); // all inactive

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x37);

        processor.Step();

        // Active elements 0..7 were inactive, so retain 0x37
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x37);
        // Tail elements 8..15 set to 1 -> 0xFF
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);
    }

    // 2. All elements inactive under vma=true (agnostic): active elements written with 1
    {
        Processor processor;
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, true), // vma = true (agnostic)
            EncodeVectorVV(0b010100, 3, 1, 1, 0, 0b010) // vmsbf.m v3, v1, v0.t
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x02);
        SetMask(processor.registers_, 0x00); // all inactive

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFF);
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);
    }

    // 3. Full mask: v0 = 0xFF -> identical to unmasked
    {
        Processor processor;
        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b010100, 3, 1, 3, 0, 0b010) // vmsif.m v3, v1, v0.t
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x04); // bit 2 set
        SetMask(processor.registers_, 0xFF); // all active

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);

        processor.Step();

        // vmsif.m with first set bit at bit 2 -> bits 0, 1, 2 set -> 0x07
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x07);
    }
}

TEST(VectorArithmeticTest, MaskLogicalVstartBoundariesComprehensive)
{
    // Test 0 < vstart < vl: vstart = 3, vl = 8
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010) // vmand.mm v3, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xFF);
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0xFF);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x00);
        // Set initial vd byte 0: bits 0..2 are 0b010 = 0x02
        processor.registers_.WriteVregElement<uint8_t>(3, 0, 0x02);

        processor.Step();

        // Bits 0..2 undisturbed: 0x02
        // Bits 3..7 computed: 1s -> 0b11111000 = 0xF8
        // Combined byte 0 = 0xFA
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0xFA);
        // Tail bits [8, 16) are set to 1
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 1), 0xFF);
        // Bytes beyond VLMAX undisturbed
        for (unsigned int b = 2; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), 0x00);

        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test vstart == vl: destination completely unchanged
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 8);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, static_cast<uint8_t>(b + 0x30));

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), static_cast<uint8_t>(b + 0x30));

        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // Test vstart > vl: destination completely unchanged
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, true, false),
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010)
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 12);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, static_cast<uint8_t>(b + 0x50));

        processor.Step();

        for (unsigned int b = 0; b < 16; ++b)
            EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, b), static_cast<uint8_t>(b + 0x50));

        EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorArithmeticTest, MaskLogicalRegisterAliasing)
{
    // Test vd == vs1
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011001, 1, 2, 1, 1, 0b010) // vmand.mm v1, v2, v1
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x0F); // vs1 and vd
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0x33); // vs2

        processor.Step();

        // 0x33 & 0x0F = 0x03
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0x03);
    }

    // Test vd == vs2
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011010, 2, 2, 1, 1, 0b010) // vmor.mm v2, v2, v1
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x0F); // vs1
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0x30); // vs2 and vd

        processor.Step();

        // 0x30 | 0x0F = 0x3F
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(2, 0), 0x3F);
    }

    // Test vd == v0 (legal for mask logical with vm = 1)
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011011, 0, 1, 2, 1, 0b010) // vmxor.mm v0, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);
        processor.Step();

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xAA);
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0x55);

        processor.Step();

        // 0xAA ^ 0x55 = 0xFF
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(0, 0), 0xFF);
    }
}

TEST(VectorArithmeticTest, MaskLogicalAndPrefixUndoRedo)
{
    // Step, Undo, and Redo for mask logical
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b011001, 3, 1, 2, 1, 0b010) // vmand.mm v3, v1, v2
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x22);

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0xFF);
        processor.registers_.WriteVregElement<uint8_t>(2, 0, 0x55);

        processor.Step(); // vsetvli
        processor.Step(); // vmand.mm

        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x55);

        // Undo -> restores initial 0x22
        processor.Undo();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x22);

        // Redo -> re-applies 0x55
        processor.Redo();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x55);
    }

    // Step, Undo, and Redo for mask prefix
    {
        Kites::setupVmStateDirectory();
        Processor processor;

        Kites::AssembledProgram program;
        program.text_buffer = {
            EncodeVsetvli(8, false, false),
            EncodeVectorVV(0b010100, 3, 1, 1, 1, 0b010) // vmsbf.m v3, v1
        };
        processor.LoadProgram(program);
        processor.registers_.WriteGpr(1, 8);

        for (unsigned int b = 0; b < 16; ++b)
            processor.registers_.WriteVregElement<uint8_t>(3, b, 0x88);

        processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x08); // bit 3 set

        processor.Step(); // vsetvli
        processor.Step(); // vmsbf.m

        // Bits 0..2 set -> 0x07
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x07);

        // Undo -> restores initial 0x88
        processor.Undo();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x88);

        // Redo -> re-applies 0x07
        processor.Redo();
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(3, 0), 0x07);
    }
}
