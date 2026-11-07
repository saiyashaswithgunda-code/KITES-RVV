#include <gtest/gtest.h>

#include "common/assembled_program.h"
#include "common/globals.h"
#include "processor/rvss/rvss_processor.h"
#include "processor/registers.h"
#include "utils/utils.h"

TEST(VectorInstructionTest, VsetvliConfiguresVectorLength)
{
    Kites::setupVmStateDirectory();

    Kites::RVSSProcessor processor;

    // vsetvli x5, x6, e32, m1, ta, ma
    //
    // vtype immediate:
    // vsew = 010 -> SEW = 32
    // vta  = 1   -> tail agnostic
    // vma  = 1   -> mask agnostic
    // vtype = 0xD0

    uint32_t instruction =
        (0xD0u << 20) |
        (6u << 15) |
        (0b111u << 12) |
        (5u << 7) |
        0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);

    processor.LoadProgram(program);

    // AVL = 10, while VLMAX = 128 / 32 = 4.
    processor.registers_.WriteGpr(6, 10);

    processor.Step();

    EXPECT_EQ(
        processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL),
        4
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VTYPE),
        0xD0
    );

    EXPECT_EQ(
        processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VSTART),
        0
    );

    EXPECT_EQ(processor.registers_.ReadGpr(5), 4);
}

TEST(VectorInstructionTest, VsetvliUsesAVLWhenBelowVLMAX)
{
    Kites::setupVmStateDirectory();

    Kites::RVSSProcessor processor;

    // vsetvli x5, x6, e32, m1, ta, ma
    // VLMAX = 128 / 32 = 4.
    uint32_t instruction =
        (0xD0u << 20) |
        (6u << 15) |
        (0b111u << 12) |
        (5u << 7) |
        0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);

    processor.LoadProgram(program);

    // AVL = 2, which is below VLMAX = 4.
    processor.registers_.WriteGpr(6, 2);

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 2);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 2);
}
TEST(VectorInstructionTest, VsetvliUsesVLMAXWhenRs1IsZeroAndRdIsNonzero)
{
    Kites::setupVmStateDirectory();

    Kites::RVSSProcessor processor;

    // vsetvli x5, x0, e32, m1, ta, ma
    uint32_t instruction =
        (0xD0u << 20) |
        (0u << 15) |
        (0b111u << 12) |
        (5u << 7) |
        0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);

    processor.LoadProgram(program);
    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 4);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 4);
}

// 1. rs1=x0, rd=x0: use current vl as AVL.
TEST(VectorInstructionTest, VsetvliPreservesCurrentVLWhenRs1AndRdAreZero)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    // First: vsetvli x5, x6, e32, m1, ta, ma
    uint32_t first =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    // Second: vsetvli x0, x0, e16, m1, ta, ma
    // e16 encoding = 0xC8.
    uint32_t second =
        (0xC8u << 20) | (7u << 12) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer = {first, second};
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 3);

    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 3);

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 3);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VTYPE), 0xC8);
}

// 2. Unsupported vtype: set vill and make vl zero.
TEST(VectorInstructionTest, VsetvliSetsVillForInvalidVtype)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    // LMUL=2 (vlmul=001), which our implementation does not support.
    uint32_t instruction =
        (0xD1u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 10);
    processor.Step();

    uint64_t vtype =
        processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VTYPE);

    EXPECT_NE(vtype & (uint64_t{1} << 63), 0);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 0);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 0);
}

// 3. vsetvli resets vstart to zero.
TEST(VectorInstructionTest, VsetvliResetsVstart)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    uint32_t instruction =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 3);
    processor.registers_.WriteCsr(Kites::RegisterFile::CSR_VSTART, 7);

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VSTART), 0);
}

// 4. rd=x0: do not write the resulting vl into x0.
TEST(VectorInstructionTest, VsetvliDoesNotModifyX0WhenRdIsZero)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    // vsetvli x0, x6, e32, m1, ta, ma
    uint32_t instruction =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 2);
    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 2);
    EXPECT_EQ(processor.registers_.ReadGpr(0), 0);
}

// 5. Undo restores the previous vector configuration and GPR.
TEST(VectorInstructionTest, UndoRestoresPreviousVectorState)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    uint32_t instruction =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 3);
    processor.registers_.WriteGpr(5, 99);
    processor.registers_.WriteCsr(Kites::RegisterFile::CSR_VSTART, 7);

    processor.Step();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 3);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 3);

    processor.Undo();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 0);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VTYPE), 0);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VSTART), 7);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 99);
}

// 6. Redo reapplies the vector configuration.
TEST(VectorInstructionTest, RedoReappliesVectorConfiguration)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    uint32_t instruction =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer.push_back(instruction);
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 3);
    processor.Step();
    processor.Undo();
    processor.Redo();

    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 3);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VTYPE), 0xD0);
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VSTART), 0);
    EXPECT_EQ(processor.registers_.ReadGpr(5), 3);
}

// 7. Changing SEW recalculates VLMAX and vl.
TEST(VectorInstructionTest, VsetvliRecalculatesVLWhenSEWChanges)
{
    Kites::setupVmStateDirectory();
    Kites::RVSSProcessor processor;

    // e8:  VLMAX=16
    uint32_t e8 =
        (0xC0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    // e16: VLMAX=8
    uint32_t e16 =
        (0xC8u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    // e32: VLMAX=4
    uint32_t e32 =
        (0xD0u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    // e64: VLMAX=2
    uint32_t e64 =
        (0xD8u << 20) | (6u << 15) | (7u << 12) | (5u << 7) | 0x57u;

    Kites::AssembledProgram program;
    program.text_buffer = {e8, e16, e32, e64};
    processor.LoadProgram(program);

    processor.registers_.WriteGpr(6, 10);

    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 10);

    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 8);

    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 4);

    processor.Step();
    EXPECT_EQ(processor.registers_.ReadCsr(Kites::RegisterFile::CSR_VL), 2);
}
