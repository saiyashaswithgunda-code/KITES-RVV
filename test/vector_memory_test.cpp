
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

// Encode strided vector load.
uint32_t EncodeVlse(
    uint32_t funct3,
    uint32_t vd = 3,
    uint32_t rs1 = 10,
    uint32_t rs2 = 11,
    uint32_t vm = 1)
{
    constexpr uint32_t mop = 0b10;
    return (mop << 26) |
           (vm << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (vd << 7) |
           0x07u;
}

// Encode strided vector store.
uint32_t EncodeVsse(
    uint32_t funct3,
    uint32_t vs3 = 3,
    uint32_t rs1 = 10,
    uint32_t rs2 = 11,
    uint32_t vm = 1)
{
    constexpr uint32_t mop = 0b10;
    return (mop << 26) |
           (vm << 25) |
           (rs2 << 20) |
           (rs1 << 15) |
           (funct3 << 12) |
           (vs3 << 7) |
           0x27u;
}

// Encode mask vector load.
uint32_t EncodeVlm(
    uint32_t vd = 1,
    uint32_t rs1 = 10,
    uint32_t vm = 1)
{
    constexpr uint32_t lumop = 0b01011;
    return (vm << 25) |
           (lumop << 20) |
           (rs1 << 15) |
           (vd << 7) |
           0x07u;
}

// Encode mask vector store.
uint32_t EncodeVsm(
    uint32_t vs3 = 1,
    uint32_t rs1 = 10,
    uint32_t vm = 1)
{
    constexpr uint32_t sumop = 0b01011;
    return (vm << 25) |
           (sumop << 20) |
           (rs1 << 15) |
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

TEST(VectorMemoryTest, FocusedStridedLoadPositiveStride)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x11
    // Stride is 8 bytes (skipping 4 bytes between elements)
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 8); // stride = 8

    // Memory layout:
    // offset 0: 111, offset 4: 999, offset 8: 222, offset 12: 999,
    // offset 16: 333, offset 20: 999, offset 24: 444
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 0, {111});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 4, {999});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 8, {222});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 12, {999});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 16, {333});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 20, {999});
    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + 24, {444});

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {111, 222, 333, 444}
    );
}

TEST(VectorMemoryTest, FocusedStridedStorePositiveStride)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsse32.v v3, (x10), x11
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVsse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 8); // stride = 8

    // Initialize memory to sentinels
    for (uint64_t offset = 0; offset <= 28; offset += 4)
    {
        WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS + offset, {0xDEADBEEF});
    }

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {555, 666, 777, 888}
    );

    processor.Step();

    // Stored elements at offsets 0, 8, 16, 24
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 0), 555u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 4), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 8), 666u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 12), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 16), 777u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 20), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 24), 888u);
}

TEST(VectorMemoryTest, FocusedMaskMemoryLoad)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Test vlm.v vd, (rs1)
    // vl = 8 -> evl = 1 byte
    LoadVectorMemoryProgram(
        processor, 8, 8,
        EncodeVlm(1, 10)
    );

    // Initial memory byte at BASE_ADDRESS
    processor.memory_controller_.writeByte(BASE_ADDRESS, 0xA5);

    processor.Step();

    // Byte 0 should be 0xA5
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0xA5u);
    // Tail bytes [1, 16) must be 0xFF (always tail-agnostic)
    for (size_t b = 1; b < RegisterFile::VLEN_BYTES; ++b)
    {
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, b), 0xFFu);
    }
}

TEST(VectorMemoryTest, FocusedMaskMemoryStore)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Test vsm.v vs3, (rs1)
    // vl = 16 -> evl = 2 bytes
    LoadVectorMemoryProgram(
        processor, 8, 16,
        EncodeVsm(1, 10)
    );

    processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x3C);
    processor.registers_.WriteVregElement<uint8_t>(1, 1, 0x7E);

    processor.Step();

    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 0), 0x3Cu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 1), 0x7Eu);
}

// ============================================================================
// Phase 2 Comprehensive Tests
// ============================================================================

TEST(VectorMemoryTest, StridedStorePartialOverlapWithStride1AndUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsse32.v v3, (x10), x11 with stride = 1 byte
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVsse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 1); // stride = 1

    // Initialize 16 bytes of memory to 0
    for (size_t i = 0; i < 16; ++i)
    {
        processor.memory_controller_.writeByte(BASE_ADDRESS + i, 0x00);
    }

    // Little-endian layout:
    // element[0] = 0x01020304 -> bytes: 0x04, 0x03, 0x02, 0x01 at offsets 0..3
    // element[1] = 0x05060708 -> bytes: 0x08, 0x07, 0x06, 0x05 at offsets 1..4
    // element[2] = 0x090A0B0C -> bytes: 0x0C, 0x0B, 0x0A, 0x09 at offsets 2..5
    // element[3] = 0x0D0E0F10 -> bytes: 0x10, 0x0F, 0x0E, 0x0D at offsets 3..6
    WriteVectorElements<uint32_t>(
        processor.registers_, 3,
        {0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10}
    );

    processor.Step();

    // Verify sequential overwrites (highest element writes last)
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 0), 0x04u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 1), 0x08u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 2), 0x0Cu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 3), 0x10u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 4), 0x0Fu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 5), 0x0Eu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 6), 0x0Du);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 7), 0x00u);

    // Verify Undo restores exact pre-instruction bytes
    processor.Undo();
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + i), 0x00u);
    }

    // Verify Redo reproduces final state
    processor.Redo();
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 0), 0x04u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 1), 0x08u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 2), 0x0Cu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 3), 0x10u);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 4), 0x0Fu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 5), 0x0Eu);
    EXPECT_EQ(processor.memory_controller_.readByte(BASE_ADDRESS + 6), 0x0Du);
}

TEST(VectorMemoryTest, StridedLoadZeroStrideBroadcast)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x11 with stride in x11 = 0
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 0); // stride = 0

    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS, {0xCAFEBABE});

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3,
        {0xCAFEBABE, 0xCAFEBABE, 0xCAFEBABE, 0xCAFEBABE}
    );
}

TEST(VectorMemoryTest, StridedLoadZeroStrideWithX0)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x0 (hardwired zero register)
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 0)
    );

    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS, {0x12345678});

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3,
        {0x12345678, 0x12345678, 0x12345678, 0x12345678}
    );
}

TEST(VectorMemoryTest, StridedStoreZeroStrideOrderingAndUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsse32.v v3, (x10), x11 with stride in x11 = 0
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVsse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 0); // stride = 0

    WriteMemoryElements<uint32_t>(processor, BASE_ADDRESS, {0x11111111});

    // Elements have different values; element 3 is highest active and writes last
    WriteVectorElements<uint32_t>(
        processor.registers_, 3,
        {0xAAAA0001, 0xAAAA0002, 0xAAAA0003, 0xAAAA0004}
    );

    processor.Step();

    // Verify highest active index persisted
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS), 0xAAAA0004u);

    // Verify Undo restores initial value
    processor.Undo();
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS), 0x11111111u);

    // Verify Redo reproduces element 3
    processor.Redo();
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS), 0xAAAA0004u);
}

TEST(VectorMemoryTest, StridedLoadNegativeStride)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x11 with negative stride = -4 bytes
    const uint64_t base = BASE_ADDRESS + 0x40; // 0x140
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(10, base);
    processor.registers_.WriteGpr(11, static_cast<uint64_t>(-4LL));

    // Element 0: base (0x140) -> 10
    // Element 1: base - 4 (0x13C) -> 20
    // Element 2: base - 8 (0x138) -> 30
    // Element 3: base - 12 (0x134) -> 40
    processor.memory_controller_.writeWord(base - 0, 10);
    processor.memory_controller_.writeWord(base - 4, 20);
    processor.memory_controller_.writeWord(base - 8, 30);
    processor.memory_controller_.writeWord(base - 12, 40);

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {10, 20, 30, 40}
    );
}

TEST(VectorMemoryTest, StridedStoreNegativeStrideAndUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsse32.v v3, (x10), x11 with stride = -8 bytes
    const uint64_t base = BASE_ADDRESS + 0x40;
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVsse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(10, base);
    processor.registers_.WriteGpr(11, static_cast<uint64_t>(-8LL));

    // Clear memory around base
    for (int offset = -24; offset <= 8; offset += 4)
    {
        processor.memory_controller_.writeWord(base + offset, 0xDEADBEEF);
    }

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {100, 200, 300, 400}
    );

    processor.Step();

    EXPECT_EQ(processor.memory_controller_.readWord(base - 0), 100u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 4), 0xDEADBEEFu); // untouched
    EXPECT_EQ(processor.memory_controller_.readWord(base - 8), 200u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 12), 0xDEADBEEFu); // untouched
    EXPECT_EQ(processor.memory_controller_.readWord(base - 16), 300u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 20), 0xDEADBEEFu); // untouched
    EXPECT_EQ(processor.memory_controller_.readWord(base - 24), 400u);

    // Undo restores all to 0xDEADBEEF
    processor.Undo();
    EXPECT_EQ(processor.memory_controller_.readWord(base - 0), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 8), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 16), 0xDEADBEEFu);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 24), 0xDEADBEEFu);

    // Redo restores values
    processor.Redo();
    EXPECT_EQ(processor.memory_controller_.readWord(base - 0), 100u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 8), 200u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 16), 300u);
    EXPECT_EQ(processor.memory_controller_.readWord(base - 24), 400u);
}

TEST(VectorMemoryTest, StridedOperationsSupportAllWidths)
{
    Kites::setupVmStateDirectory();

    // 1. 8-bit strided load & store
    {
        Processor p8;
        LoadVectorMemoryProgram(p8, 8, 4, EncodeVlse(0b000, 3, 10, 11));
        p8.registers_.WriteGpr(11, 2); // stride = 2 bytes
        p8.memory_controller_.writeByte(BASE_ADDRESS + 0, 11);
        p8.memory_controller_.writeByte(BASE_ADDRESS + 2, 22);
        p8.memory_controller_.writeByte(BASE_ADDRESS + 4, 33);
        p8.memory_controller_.writeByte(BASE_ADDRESS + 6, 44);
        p8.Step();
        ExpectVectorElements<uint8_t>(p8.registers_, 3, {11, 22, 33, 44});
    }

    // 2. 16-bit strided load & store
    {
        Processor p16;
        LoadVectorMemoryProgram(p16, 16, 4, EncodeVlse(0b101, 3, 10, 11));
        p16.registers_.WriteGpr(11, 4); // stride = 4 bytes
        p16.memory_controller_.writeHalfWord(BASE_ADDRESS + 0, 1001);
        p16.memory_controller_.writeHalfWord(BASE_ADDRESS + 4, 2002);
        p16.memory_controller_.writeHalfWord(BASE_ADDRESS + 8, 3003);
        p16.memory_controller_.writeHalfWord(BASE_ADDRESS + 12, 4004);
        p16.Step();
        ExpectVectorElements<uint16_t>(p16.registers_, 3, {1001, 2002, 3003, 4004});
    }

    // 3. 64-bit strided load & store
    {
        Processor p64;
        LoadVectorMemoryProgram(p64, 64, 2, EncodeVlse(0b111, 3, 10, 11));
        p64.registers_.WriteGpr(11, 16); // stride = 16 bytes
        p64.memory_controller_.writeDoubleWord(BASE_ADDRESS + 0, 0x1122334455667788ULL);
        p64.memory_controller_.writeDoubleWord(BASE_ADDRESS + 16, 0x99AABBCCDDEEFF00ULL);
        p64.Step();
        ExpectVectorElements<uint64_t>(p64.registers_, 3, {0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL});
    }
}

TEST(VectorMemoryTest, StridedLoadMaskedWithInactiveMemoryUnread)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x11, v0.t (masked, vm = 0)
    // tail undisturbed (vta = 0), mask agnostic (vma = 1)
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11, /*vm=*/0),
        /*tail_agnostic=*/false, /*mask_agnostic=*/true
    );
    processor.registers_.WriteGpr(11, 8); // stride = 8

    // Pre-populate destination register v3 with sentinel values
    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {0x1234, 0x5678, 0x9ABC, 0xDEF0}
    );

    // Active elements: 0 and 2. Inactive: 1 and 3.
    SetMask(processor.registers_, 0b00000101);

    // Memory at element 0 (0) and element 2 (16)
    processor.memory_controller_.writeWord(BASE_ADDRESS + 0, 0x1111);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 8, 0xDEAD); // Inactive address
    processor.memory_controller_.writeWord(BASE_ADDRESS + 16, 0x2222);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 24, 0xDEAD); // Inactive address

    processor.Step();

    // Element 0: 0x1111 (loaded)
    // Element 1: 0xFFFFFFFF (inactive, mask-agnostic set to all 1s)
    // Element 2: 0x2222 (loaded)
    // Element 3: 0xFFFFFFFF (inactive, mask-agnostic set to all 1s)
    ExpectVectorElements<uint32_t>(
        processor.registers_, 3,
        {0x1111, 0xFFFFFFFF, 0x2222, 0xFFFFFFFF}
    );
}

TEST(VectorMemoryTest, StridedStoreMaskedDoesNotModifyInactiveMemory)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vsse32.v v3, (x10), x11, v0.t (masked, vm = 0)
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVsse(0b110, 3, 10, 11, /*vm=*/0)
    );
    processor.registers_.WriteGpr(11, 8); // stride = 8

    // Active elements: 0 and 2. Inactive: 1 and 3.
    SetMask(processor.registers_, 0b00000101);

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {0x1111, 0x2222, 0x3333, 0x4444}
    );

    // Initialize all destination locations to 0x9999
    processor.memory_controller_.writeWord(BASE_ADDRESS + 0, 0x9999);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 8, 0x9999);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 16, 0x9999);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 24, 0x9999);

    processor.Step();

    // Active elements 0 and 2 were stored; inactive 1 and 3 were NOT modified
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 0), 0x1111u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 8), 0x9999u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 16), 0x3333u);
    EXPECT_EQ(processor.memory_controller_.readWord(BASE_ADDRESS + 24), 0x9999u);
}

TEST(VectorMemoryTest, MaskMemoryUnderDifferentSEWConfigurations)
{
    Kites::setupVmStateDirectory();

    // Mask memory operates on bytes (EEW=8, EMUL=1) independently of configured SEW.
    // 1. Under SEW=64: VLMAX is 2, so vl=2, evl = ceil(2/8) = 1 byte.
    {
        Processor p64;
        LoadVectorMemoryProgram(p64, 64, 2, EncodeVlm(1, 10)); // SEW=64, vl=2 -> evl=1
        p64.memory_controller_.writeByte(BASE_ADDRESS + 0, 0x12);
        p64.memory_controller_.writeByte(BASE_ADDRESS + 1, 0x34);
        p64.Step();
        EXPECT_EQ(p64.registers_.ReadVregElement<uint8_t>(1, 0), 0x12u);
        EXPECT_EQ(p64.registers_.ReadVregElement<uint8_t>(1, 1), 0xFFu); // tail byte is agnostic
    }

    // 2. Under SEW=16: VLMAX is 8, so vl=8, evl = ceil(8/8) = 1 byte.
    {
        Processor p16;
        LoadVectorMemoryProgram(p16, 16, 8, EncodeVsm(1, 10)); // SEW=16, vl=8 -> evl=1
        p16.registers_.WriteVregElement<uint8_t>(1, 0, 0xAA);
        p16.registers_.WriteVregElement<uint8_t>(1, 1, 0x55);
        p16.Step();
        EXPECT_EQ(p16.memory_controller_.readByte(BASE_ADDRESS + 0), 0xAAu);
        EXPECT_EQ(p16.memory_controller_.readByte(BASE_ADDRESS + 1), 0x00u); // beyond evl not stored
    }

    // 3. Under SEW=8: VLMAX is 16, so vl=16 -> evl = 2 bytes.
    {
        Processor p8;
        LoadVectorMemoryProgram(p8, 8, 16, EncodeVlm(1, 10)); // SEW=8, vl=16 -> evl=2
        p8.memory_controller_.writeByte(BASE_ADDRESS + 0, 0x12);
        p8.memory_controller_.writeByte(BASE_ADDRESS + 1, 0x34);
        p8.Step();
        EXPECT_EQ(p8.registers_.ReadVregElement<uint8_t>(1, 0), 0x12u);
        EXPECT_EQ(p8.registers_.ReadVregElement<uint8_t>(1, 1), 0x34u);
        EXPECT_EQ(p8.registers_.ReadVregElement<uint8_t>(1, 2), 0xFFu); // tail
    }
}

TEST(VectorMemoryTest, MaskMemoryAlwaysTailAgnosticEvenWhenVtaIsZero)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlm.v with tail-undisturbed configured (vta = false)
    LoadVectorMemoryProgram(
        processor, 8, 8,
        EncodeVlm(1, 10),
        /*tail_agnostic=*/false
    );

    // Pre-initialize destination register to all zeros
    for (size_t b = 0; b < RegisterFile::VLEN_BYTES; ++b)
    {
        processor.registers_.WriteVregElement<uint8_t>(1, b, 0x00);
    }

    processor.memory_controller_.writeByte(BASE_ADDRESS, 0x77);

    processor.Step();

    // Byte 0 is loaded
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0x77u);
    // Tail bytes [1, 16) must still be 0xFF (always tail-agnostic!)
    for (size_t b = 1; b < RegisterFile::VLEN_BYTES; ++b)
    {
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, b), 0xFFu);
    }
}

TEST(VectorMemoryTest, MaskMemoryPartialFinalByteTransferredInFull)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vl = 3 -> evl = 1 byte
    LoadVectorMemoryProgram(
        processor, 8, 3,
        EncodeVlm(1, 10)
    );

    // Prepopulate byte with upper bits set
    processor.memory_controller_.writeByte(BASE_ADDRESS, 0b11010101);

    processor.Step();

    // Top bits must NOT be masked out
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0b11010101u);
}

TEST(VectorMemoryTest, MaskMemoryZeroVLNoBody)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vl = 0 -> evl = 0 bytes
    LoadVectorMemoryProgram(
        processor, 8, 0,
        EncodeVlm(1, 10)
    );

    // Pre-fill destination register with non-0xFF values
    for (size_t b = 0; b < RegisterFile::VLEN_BYTES; ++b)
    {
        processor.registers_.WriteVregElement<uint8_t>(1, b, 0x33);
    }

    processor.Step();

    // Destination register must remain completely untouched (no 0xFF tail fill!)
    for (size_t b = 0; b < RegisterFile::VLEN_BYTES; ++b)
    {
        EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, b), 0x33u);
    }
}

TEST(VectorMemoryTest, StridedLoadNonzeroVstart)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 8); // stride = 8

    // Pre-fill destination with sentinels
    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {0x11, 0x22, 0x33, 0x44}
    );

    // Pre-populate memory
    processor.memory_controller_.writeWord(BASE_ADDRESS + 0, 0xAAAA);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 8, 0xBBBB);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 16, 0xCCCC);
    processor.memory_controller_.writeWord(BASE_ADDRESS + 24, 0xDDDD);

    // vstart = 2 (elements 0 and 1 skipped)
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2);

    processor.Step();

    // Elements 0 and 1 untouched; elements 2 and 3 loaded
    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {0x11, 0x22, 0xCCCC, 0xDDDD}
    );

    // vstart must be reset to 0
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

TEST(VectorMemoryTest, StridedNoBodyVstartGeVL)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    LoadVectorMemoryProgram(
        processor, 32, 2, // vl = 2
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 8);

    // vstart = 3 >= vl
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {0x55, 0x66, 0x77, 0x88}
    );

    processor.Step();

    // Destination register must be completely unchanged (no body or tail write)
    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {0x55, 0x66, 0x77, 0x88}
    );

    // vstart reset to 0
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
}

TEST(VectorMemoryTest, MaskMemoryVstartBoundaryAndNoBody)
{
    Kites::setupVmStateDirectory();

    // 1. Partial vstart (0 < vstart < evl)
    {
        Processor p;
        LoadVectorMemoryProgram(p, 8, 16, EncodeVlm(1, 10)); // vl = 16 -> evl = 2 bytes
        p.registers_.WriteVregElement<uint8_t>(1, 0, 0x11);
        p.registers_.WriteVregElement<uint8_t>(1, 1, 0x22);
        p.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1); // skip byte 0

        p.memory_controller_.writeByte(BASE_ADDRESS + 0, 0xAA);
        p.memory_controller_.writeByte(BASE_ADDRESS + 1, 0xBB);

        p.Step();

        // Byte 0 untouched (0x11); byte 1 loaded (0xBB); tail [2, 16) filled with 0xFF
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 0), 0x11u);
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 1), 0xBBu);
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 2), 0xFFu);
        EXPECT_EQ(p.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }

    // 2. No-body vstart (vstart >= evl)
    {
        Processor p;
        LoadVectorMemoryProgram(p, 8, 8, EncodeVlm(1, 10)); // vl = 8 -> evl = 1 byte
        p.registers_.WriteCsr(RegisterFile::CSR_VSTART, 2); // vstart = 2 >= evl
        for (size_t b = 0; b < RegisterFile::VLEN_BYTES; ++b)
        {
            p.registers_.WriteVregElement<uint8_t>(1, b, 0x44);
        }

        p.Step();

        // Destination unchanged, no tail fill
        for (size_t b = 0; b < RegisterFile::VLEN_BYTES; ++b)
        {
            EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, b), 0x44u);
        }
        EXPECT_EQ(p.registers_.ReadCsr(RegisterFile::CSR_VSTART), 0u);
    }
}

TEST(VectorMemoryTest, MaskMemoryReservedVm0Rejected)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlm.v encoded with vm = 0 (reserved encoding!)
    LoadVectorMemoryProgram(
        processor, 8, 8,
        EncodeVlm(1, 10, /*vm=*/0)
    );
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 3);
    processor.registers_.WriteVregElement<uint8_t>(1, 0, 0x99);

    processor.memory_controller_.writeByte(BASE_ADDRESS, 0x55);

    processor.Step();

    // Instruction must be rejected: destination unchanged, vstart NOT reset
    EXPECT_EQ(processor.registers_.ReadVregElement<uint8_t>(1, 0), 0x99u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 3u);
}

TEST(VectorMemoryTest, StridedMemorySEWMismatchRejected)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // Configure SEW = 16, but execute vlse32.v (EEW = 32)
    LoadVectorMemoryProgram(
        processor, 16, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 4);
    processor.registers_.WriteCsr(RegisterFile::CSR_VSTART, 1);
    processor.registers_.WriteVregElement<uint32_t>(3, 0, 0x12345678);

    processor.Step();

    // Rejected: destination untouched, vstart NOT reset
    EXPECT_EQ(processor.registers_.ReadVregElement<uint32_t>(3, 0), 0x12345678u);
    EXPECT_EQ(processor.registers_.ReadCsr(RegisterFile::CSR_VSTART), 1u);
}

TEST(VectorMemoryTest, StridedLoadUndoRedo)
{
    Kites::setupVmStateDirectory();
    Processor processor;

    // vlse32.v v3, (x10), x11
    LoadVectorMemoryProgram(
        processor, 32, 4,
        EncodeVlse(0b110, 3, 10, 11)
    );
    processor.registers_.WriteGpr(11, 4); // stride = 4

    WriteVectorElements<uint32_t>(
        processor.registers_, 3, {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD}
    );

    WriteMemoryElements<uint32_t>(
        processor, BASE_ADDRESS, {11, 22, 33, 44}
    );

    processor.Step();

    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {11, 22, 33, 44}
    );

    processor.Undo();
    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD}
    );

    processor.Redo();
    ExpectVectorElements<uint32_t>(
        processor.registers_, 3, {11, 22, 33, 44}
    );
}

TEST(VectorMemoryTest, MaskMemoryLoadAndStoreUndoRedo)
{
    Kites::setupVmStateDirectory();

    // Test vlm.v Undo/Redo
    {
        Processor p;
        LoadVectorMemoryProgram(p, 8, 8, EncodeVlm(1, 10)); // evl = 1
        p.registers_.WriteVregElement<uint8_t>(1, 0, 0x11);
        p.memory_controller_.writeByte(BASE_ADDRESS, 0xEE);

        p.Step();
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 0), 0xEEu);

        p.Undo();
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 0), 0x11u);

        p.Redo();
        EXPECT_EQ(p.registers_.ReadVregElement<uint8_t>(1, 0), 0xEEu);
    }

    // Test vsm.v Undo/Redo
    {
        Processor p;
        LoadVectorMemoryProgram(p, 8, 8, EncodeVsm(1, 10)); // evl = 1
        p.registers_.WriteVregElement<uint8_t>(1, 0, 0x77);
        p.memory_controller_.writeByte(BASE_ADDRESS, 0x22);

        p.Step();
        EXPECT_EQ(p.memory_controller_.readByte(BASE_ADDRESS), 0x77u);

        p.Undo();
        EXPECT_EQ(p.memory_controller_.readByte(BASE_ADDRESS), 0x22u);

        p.Redo();
        EXPECT_EQ(p.memory_controller_.readByte(BASE_ADDRESS), 0x77u);
    }
}