
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

constexpr uint64_t BASE_ADDRESS = 0x100;

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

    const uint32_t vtype =
        (vsew << 3) |
        (static_cast<uint32_t>(tail_agnostic) << 6) |
        (static_cast<uint32_t>(mask_agnostic) << 7);

    return (vtype << 20) |
           (1u << 15) |
           (7u << 12) |
           0x57u;
}

// Encode unit-stride, unmasked vector load.
uint32_t EncodeVle(
    uint32_t funct3,
    uint32_t vd = 3,
    uint32_t rs1 = 10,
    uint32_t vm = 1)
{
    return (vm << 25) |
           (rs1 << 15) |
           (funct3 << 12) |
           (vd << 7) |
           0x07u;
}

// Encode unit-stride, unmasked vector store.
uint32_t EncodeVse(
    uint32_t funct3,
    uint32_t vs3 = 3,
    uint32_t rs1 = 10,
    uint32_t vm = 1)
{
    return (vm << 25) |
           (rs1 << 15) |
           (funct3 << 12) |
           (vs3 << 7) |
           0x27u;
}

void LoadVectorMemoryProgram(
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
    processor.registers_.WriteGpr(10, BASE_ADDRESS);

    // Execute vsetvli.
    processor.Step();
}

template <typename T>
void WriteVectorElements(
    RegisterFile &registers,
    unsigned int reg,
    const std::vector<T> &values)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        registers.WriteVregElement<T>(reg, i, values[i]);
    }
}

template <typename T>
void ExpectVectorElements(
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

// Write elements to memory in little-endian order.
template <typename T>
void WriteMemoryElements(
    Processor &processor,
    uint64_t address,
    const std::vector<T> &values)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        for (size_t b = 0; b < sizeof(T); ++b)
        {
            processor.memory_controller_.writeByte(
                address + i * sizeof(T) + b,
                static_cast<uint8_t>(
                    (values[i] >> (8 * b)) & 0xFF
                )
            );
        }
    }
}

template <typename T>
void ExpectMemoryElements(
    Processor &processor,
    uint64_t address,
    const std::vector<T> &expected)
{
    for (size_t i = 0; i < expected.size(); ++i)
    {
        uint64_t value = 0;

        for (size_t b = 0; b < sizeof(T); ++b)
        {
            value |= static_cast<uint64_t>(
                processor.memory_controller_.readByte(
                    address + i * sizeof(T) + b
                )
            ) << (8 * b);
        }

        EXPECT_EQ(static_cast<T>(value), expected[i])
            << "Mismatch at memory element " << i;
    }
}
template <typename T>
void TestVectorLoad(uint32_t funct3)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Generate exactly VLMAX elements for the configured SEW.
    const size_t element_count =
        RegisterFile::VLEN_BYTES / sizeof(T);

    std::vector<T> values;

    for (size_t i = 0; i < element_count; ++i)
    {
        values.push_back(static_cast<T>((i + 1) * 10));
    }

    LoadVectorMemoryProgram(
        processor,
        sizeof(T) * 8,
        values.size(),
        EncodeVle(funct3)
    );

    WriteMemoryElements(processor, BASE_ADDRESS, values);

    // Execute vector load.
    processor.Step();

    ExpectVectorElements<T>(
        processor.registers_,
        3,
        values
    );
}
template <typename T>
void TestVectorStore(uint32_t funct3)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Generate exactly VLMAX elements for the configured SEW.
    const size_t element_count =
        RegisterFile::VLEN_BYTES / sizeof(T);

    std::vector<T> values;

    for (size_t i = 0; i < element_count; ++i)
    {
        values.push_back(static_cast<T>((i + 1) * 10));
    }

    LoadVectorMemoryProgram(
        processor,
        sizeof(T) * 8,
        values.size(),
        EncodeVse(funct3)
    );

    WriteVectorElements<T>(
        processor.registers_,
        3,
        values
    );

    // Initialize memory with different values.
    WriteMemoryElements<T>(
        processor,
        BASE_ADDRESS,
        std::vector<T>(values.size(), static_cast<T>(99))
    );

    // Execute vector store.
    processor.Step();

    ExpectMemoryElements<T>(
        processor,
        BASE_ADDRESS,
        values
    );
}
} // namespace

// ---------------------------------------------------------
// 1–4. Vector loads for all supported element widths
// ---------------------------------------------------------

TEST(VectorMemoryTest, Vle8)
{
    TestVectorLoad<uint8_t>(0b000);
}

TEST(VectorMemoryTest, Vle16)
{
    TestVectorLoad<uint16_t>(0b101);
}

TEST(VectorMemoryTest, Vle32)
{
    TestVectorLoad<uint32_t>(0b110);
}

TEST(VectorMemoryTest, Vle64)
{
    TestVectorLoad<uint64_t>(0b111);
}

// ---------------------------------------------------------
// 5–8. Vector stores for all supported element widths
// ---------------------------------------------------------

TEST(VectorMemoryTest, Vse8)
{
    TestVectorStore<uint8_t>(0b000);
}

TEST(VectorMemoryTest, Vse16)
{
    TestVectorStore<uint16_t>(0b101);
}

TEST(VectorMemoryTest, Vse32)
{
    TestVectorStore<uint32_t>(0b110);
}

TEST(VectorMemoryTest, Vse64)
{
    TestVectorStore<uint64_t>(0b111);
}

// ---------------------------------------------------------
// 9. VL: only active elements should be loaded
// ---------------------------------------------------------

TEST(VectorMemoryTest, VleRespectsVectorLength)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 2, EncodeVle(0b110)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 99, 99}
    );
}

// ---------------------------------------------------------
// 10. VSTART: load starts at the specified element
// ---------------------------------------------------------

TEST(VectorMemoryTest, VleRespectsVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.registers_.WriteCsr(
        RegisterFile::CSR_VSTART, 2
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 30, 40}
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART),
        0
    );
}

// ---------------------------------------------------------
// 11. VSTART: store skips earlier elements
// ---------------------------------------------------------

TEST(VectorMemoryTest, VseRespectsVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVse(0b110)
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    processor.registers_.WriteCsr(
        RegisterFile::CSR_VSTART, 2
    );

    processor.Step();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 30, 40}
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART),
        0
    );
}

// ---------------------------------------------------------
// 12. Undo vector load
// ---------------------------------------------------------

TEST(VectorMemoryTest, UndoRestoresVectorDestination)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    processor.Undo();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );
}

// ---------------------------------------------------------
// 13. Redo vector load
// ---------------------------------------------------------

TEST(VectorMemoryTest, RedoReappliesVectorLoad)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    processor.Step();
    processor.Undo();
    processor.Redo();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );
}

// ---------------------------------------------------------
// 14. Undo vector store
// ---------------------------------------------------------

TEST(VectorMemoryTest, UndoRestoresMemory)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVse(0b110)
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    processor.Undo();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );
}

// ---------------------------------------------------------
// 15. Redo vector store
// ---------------------------------------------------------

TEST(VectorMemoryTest, RedoReappliesMemoryStore)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVse(0b110)
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    processor.Step();
    processor.Undo();
    processor.Redo();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );
}
TEST(VectorMemoryTest, MaskedLoad)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110, 3, 10, 0)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 99, 30, 99}
    );
}

TEST(VectorMemoryTest, MaskedStore)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVse(0b110, 3, 10, 0)
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 99, 30, 99}
    );
}
TEST(VectorMemoryTest, MaskAgnosticLoad)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVle(0b110, 3, 10, 0),
        false, true
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3,
        {10, UINT32_MAX, 30, UINT32_MAX}
    );
}
TEST(VectorMemoryTest, TailAgnosticLoad)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 2,
        EncodeVle(0b110),
        true, false
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3,
        {10, 20, UINT32_MAX, UINT32_MAX}
    );
}
TEST(VectorMemoryTest, MaskedLoadRespectsVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110, 3, 10, 0)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.registers_.WriteCsr(
        RegisterFile::CSR_VSTART, 1
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 30, 99}
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(RegisterFile::CSR_VSTART),
        0
    );
}
TEST(VectorMemoryTest, MaskedStoreUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVse(0b110, 3, 10, 0)
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 99, 30, 99}
    );

    processor.Undo();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    processor.Redo();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 99, 30, 99}
    );
}

TEST(VectorMemoryTest, MaskedLoadUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4, EncodeVle(0b110, 3, 10, 0)
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 20, 30, 40}
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 99, 30, 99}
    );

    processor.Undo();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {99, 99, 99, 99}
    );

    processor.Redo();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 99, 30, 99}
    );
}
TEST(VectorMemoryTest, MaskAgnosticStoreDoesNotModifyInactiveMemory)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVse(0b110, 3, 10, 0),
        false, true
    );

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {99, 99, 99, 99}
    );

    SetMask(processor.registers_, 0b00000101);

    processor.Step();

    ExpectMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {10, 99, 30, 99}
    );
}