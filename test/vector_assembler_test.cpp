
#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>

#include "assembler/assembler.h"
#include "utils/utils.h"

namespace
{

uint32_t assembleVsetvli(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
uint32_t assembleVectorVV(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
uint32_t assembleVectorMemory(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
uint32_t assembleVectorReduction(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
uint32_t assembleVectorVX(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
uint32_t assembleVectorVI(const std::string &source)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream(source);
    Kites::AssembledProgram program = Kites::assemble(stream);

    EXPECT_EQ(program.text_buffer.size(), 1u);

    if (program.text_buffer.empty())
    {
        return 0;
    }

    return program.text_buffer[0];
}
} // namespace

TEST(VectorAssemblerTest, EncodesVsetvliE32Correctly)
{
    // vsetvli x5, x6, e32, m1, ta, ma
    //
    // VTYPE = 0xD0
    // Expected encoding = 0x0D0372D7

    EXPECT_EQ(
        assembleVsetvli("vsetvli x5, x6, e32, m1, ta, ma\n"),
        0x0D0372D7u
    );
}

TEST(VectorAssemblerTest, EncodesAllSupportedSEWValues)
{
    struct TestCase
    {
        const char *sew;
        uint32_t expectedVtype;
    };

    const TestCase cases[] = {
        {"e8",  0xC0},
        {"e16", 0xC8},
        {"e32", 0xD0},
        {"e64", 0xD8}
    };

    for (const auto &testCase : cases)
    {
        SCOPED_TRACE(testCase.sew);

        std::string source =
            std::string("vsetvli x5, x6, ") +
            testCase.sew + ", m1, ta, ma\n";

        uint32_t expected =
            (testCase.expectedVtype << 20) |
            (6u << 15) |
            (7u << 12) |
            (5u << 7) |
            0x57u;

        EXPECT_EQ(assembleVsetvli(source), expected);
    }
}

TEST(VectorAssemblerTest, EncodesZeroRegistersCorrectly)
{
    // vsetvli x0, x0, e8, m1, tu, mu
    // VTYPE = 0x00

    EXPECT_EQ(
        assembleVsetvli("vsetvli x0, x0, e8, m1, tu, mu\n"),
        0x00007057u
    );
}
TEST(VectorAssemblerTest, EncodesVaddVVCorrectly)
{
    EXPECT_EQ(
        assembleVectorVV("vadd.vv v1, v2, v3\n"),
        0x022180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsubVVCorrectly)
{
    EXPECT_EQ(
        assembleVectorVV("vsub.vv v1, v2, v3\n"),
        0x0A2180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVandVVCorrectly)
{
    EXPECT_EQ(
        assembleVectorVV("vand.vv v1, v2, v3\n"),
        0x262180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVorVVCorrectly)
{
    EXPECT_EQ(
        assembleVectorVV("vor.vv v1, v2, v3\n"),
        0x2A2180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVxorVVCorrectly)
{
    EXPECT_EQ(
        assembleVectorVV("vxor.vv v1, v2, v3\n"),
        0x2E2180D7u
    );
}
TEST(VectorAssemblerTest, EncodesAllVectorMemoryInstructions)
{
    struct TestCase
    {
        const char *instruction;
        uint32_t expected;
    };

    const TestCase cases[] = {
        {"vle8.v v3, (x10)",  0x02050187u},
        {"vle16.v v3, (x10)", 0x02055187u},
        {"vle32.v v3, (x10)", 0x02056187u},
        {"vle64.v v3, (x10)", 0x02057187u},

        {"vse8.v v3, (x10)",  0x020501A7u},
        {"vse16.v v3, (x10)", 0x020551A7u},
        {"vse32.v v3, (x10)", 0x020561A7u},
        {"vse64.v v3, (x10)", 0x020571A7u}
    };

    for (const auto &testCase : cases)
    {
        SCOPED_TRACE(testCase.instruction);

        std::string source =
            std::string(testCase.instruction) + "\n";

        EXPECT_EQ(
            assembleVectorMemory(source),
            testCase.expected
        );
    }
}

TEST(VectorAssemblerTest, EncodesVectorMemoryRegistersCorrectly)
{
    // vle8.v v17, (x5)
    //
    // vm  = 1
    // rs1 = x5
    // vd  = v17
    // funct3 = 000
    // opcode = 0000111

    EXPECT_EQ(
        assembleVectorMemory("vle8.v v17, (x5)\n"),
        0x02028887u
    );

    EXPECT_EQ(
        assembleVectorMemory("vse8.v v17, (x5)\n"),
        0x020288A7u
    );
}

TEST(VectorAssemblerTest, EncodesVredsumVSCorrectly)
{
    // vredsum.vs v4, v2, v3
    //
    // funct6 = 000000
    // vm     = 1
    // vs2    = v2
    // vs1    = v3
    // funct3 = 010
    // vd     = v4
    // opcode = 1010111
    //
    // Expected encoding = 0x0221A257

    EXPECT_EQ(
        assembleVectorReduction("vredsum.vs v4, v2, v3\n"),
        0x0221A257u
    );
}

TEST(VectorAssemblerTest, EncodesVredsumVSZeroRegistersCorrectly)
{
    // vredsum.vs v0, v0, v0
    //
    // Expected encoding = 0x02002057

    EXPECT_EQ(
        assembleVectorReduction("vredsum.vs v0, v0, v0\n"),
        0x02002057u
    );
}

TEST(VectorAssemblerTest, EncodesVaddVXCorrectly)
{
    // vadd.vx v1, v2, x3
    // funct6 = 000000
    // vm     = 1
    // vs2    = 2
    // rs1    = 3
    // funct3 = 100
    // vd     = 1
    // opcode = 1010111
    // Expected = 0x0221C0D7
    EXPECT_EQ(
        assembleVectorVX("vadd.vx v1, v2, x3\n"),
        0x0221C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsubVXCorrectly)
{
    // vsub.vx v1, v2, x3
    // funct6 = 000010
    // vm     = 1
    // vs2    = 2
    // rs1    = 3
    // funct3 = 100
    // vd     = 1
    // opcode = 1010111
    // Expected = 0x0A21C0D7
    EXPECT_EQ(
        assembleVectorVX("vsub.vx v1, v2, x3\n"),
        0x0A21C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVaddVXWithAbiNames)
{
    // a0 is x10
    // rs1 = 10
    // Expected = 0x022540D7
    EXPECT_EQ(
        assembleVectorVX("vadd.vx v1, v2, a0\n"),
        0x022540D7u
    );
}

TEST(VectorAssemblerTest, EncodesVaddVXBoundaryRegisters)
{
    // vadd.vx v0, v0, x0 -> 0x02004057
    EXPECT_EQ(
        assembleVectorVX("vadd.vx v0, v0, x0\n"),
        0x02004057u
    );

    // vsub.vx v31, v31, x31 -> 0x0BFFCFD7
    EXPECT_EQ(
        assembleVectorVX("vsub.vx v31, v31, x31\n"),
        0x0BFFCFD7u
    );
}

TEST(VectorAssemblerTest, EncodesVaddVICorrectly)
{
    // vadd.vi v1, v2, 3 -> 0x0221B0D7
    EXPECT_EQ(
        assembleVectorVI("vadd.vi v1, v2, 3\n"),
        0x0221B0D7u
    );

    // vadd.vi v1, v2, -1 -> simm5=31 (0x1F) -> 0x022FB0D7
    EXPECT_EQ(
        assembleVectorVI("vadd.vi v1, v2, -1\n"),
        0x022FB0D7u
    );

    // vadd.vi v1, v2, -16 -> simm5=16 (0x10) -> 0x022830D7
    EXPECT_EQ(
        assembleVectorVI("vadd.vi v1, v2, -16\n"),
        0x022830D7u
    );

    // vadd.vi v1, v2, 15 -> simm5=15 (0x0F) -> 0x0227B0D7
    EXPECT_EQ(
        assembleVectorVI("vadd.vi v1, v2, 15\n"),
        0x0227B0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVrsubVXCorrectly)
{
    // vrsub.vx v1, v2, x3 -> funct6=000011, funct3=100 -> 0x0E21C0D7
    EXPECT_EQ(
        assembleVectorVX("vrsub.vx v1, v2, x3\n"),
        0x0E21C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVrsubVICorrectly)
{
    // vrsub.vi v1, v2, 3 -> funct6=000011, funct3=011 -> 0x0E21B0D7
    EXPECT_EQ(
        assembleVectorVI("vrsub.vi v1, v2, 3\n"),
        0x0E21B0D7u
    );

    // vrsub.vi v1, v2, -1 -> simm5=31 -> 0x0E2FB0D7
    EXPECT_EQ(
        assembleVectorVI("vrsub.vi v1, v2, -1\n"),
        0x0E2FB0D7u
    );
}

TEST(VectorAssemblerTest, EncodesMinMaxVVCorrectly)
{
    // vminu.vv v1, v2, v3 -> funct6=000100, funct3=000 -> 0x122180D7
    EXPECT_EQ(
        assembleVectorVV("vminu.vv v1, v2, v3\n"),
        0x122180D7u
    );

    // vmin.vv v1, v2, v3 -> funct6=000101, funct3=000 -> 0x162180D7
    EXPECT_EQ(
        assembleVectorVV("vmin.vv v1, v2, v3\n"),
        0x162180D7u
    );

    // vmaxu.vv v1, v2, v3 -> funct6=000110, funct3=000 -> 0x1A2180D7
    EXPECT_EQ(
        assembleVectorVV("vmaxu.vv v1, v2, v3\n"),
        0x1A2180D7u
    );

    // vmax.vv v1, v2, v3 -> funct6=000111, funct3=000 -> 0x1E2180D7
    EXPECT_EQ(
        assembleVectorVV("vmax.vv v1, v2, v3\n"),
        0x1E2180D7u
    );
}

TEST(VectorAssemblerTest, EncodesMinMaxVXCorrectly)
{
    // vminu.vx v1, v2, x3 -> funct6=000100, funct3=100 -> 0x1221C0D7
    EXPECT_EQ(
        assembleVectorVX("vminu.vx v1, v2, x3\n"),
        0x1221C0D7u
    );

    // vmin.vx v1, v2, x3 -> funct6=000101, funct3=100 -> 0x1621C0D7
    EXPECT_EQ(
        assembleVectorVX("vmin.vx v1, v2, x3\n"),
        0x1621C0D7u
    );

    // vmaxu.vx v1, v2, x3 -> funct6=000110, funct3=100 -> 0x1A21C0D7
    EXPECT_EQ(
        assembleVectorVX("vmaxu.vx v1, v2, x3\n"),
        0x1A21C0D7u
    );

    // vmax.vx v1, v2, x3 -> funct6=000111, funct3=100 -> 0x1E21C0D7
    EXPECT_EQ(
        assembleVectorVX("vmax.vx v1, v2, x3\n"),
        0x1E21C0D7u
    );
}

TEST(VectorAssemblerTest, RejectsOutOfRangeImmediate)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream_pos("vadd.vi v1, v2, 16\n");
    EXPECT_THROW(Kites::assemble(stream_pos), std::runtime_error);

    std::istringstream stream_neg("vadd.vi v1, v2, -17\n");
    EXPECT_THROW(Kites::assemble(stream_neg), std::runtime_error);
}

TEST(VectorAssemblerTest, EncodesVsllVVCorrectly)
{
    // vsll.vv v1, v2, v3 -> funct6=100101, funct3=000 -> 0x962180D7
    EXPECT_EQ(
        assembleVectorVV("vsll.vv v1, v2, v3\n"),
        0x962180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsllVXCorrectly)
{
    // vsll.vx v1, v2, x3 -> funct6=100101, funct3=100 -> 0x9621C0D7
    EXPECT_EQ(
        assembleVectorVX("vsll.vx v1, v2, x3\n"),
        0x9621C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsllVICorrectly)
{
    // vsll.vi v1, v2, 3 -> funct6=100101, funct3=011, uimm5=3 -> 0x9621B0D7
    EXPECT_EQ(
        assembleVectorVI("vsll.vi v1, v2, 3\n"),
        0x9621B0D7u
    );
    // vsll.vi v1, v2, 0 -> 0x962030D7
    EXPECT_EQ(
        assembleVectorVI("vsll.vi v1, v2, 0\n"),
        0x962030D7u
    );
    // vsll.vi v1, v2, 31 -> 0x962FB0D7
    EXPECT_EQ(
        assembleVectorVI("vsll.vi v1, v2, 31\n"),
        0x962FB0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsrlVVCorrectly)
{
    // vsrl.vv v1, v2, v3 -> funct6=101000, funct3=000 -> 0xA22180D7
    EXPECT_EQ(
        assembleVectorVV("vsrl.vv v1, v2, v3\n"),
        0xA22180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsrlVXCorrectly)
{
    // vsrl.vx v1, v2, x3 -> funct6=101000, funct3=100 -> 0xA221C0D7
    EXPECT_EQ(
        assembleVectorVX("vsrl.vx v1, v2, x3\n"),
        0xA221C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsrlVICorrectly)
{
    // vsrl.vi v1, v2, 3 -> funct6=101000, funct3=011, uimm5=3 -> 0xA221B0D7
    EXPECT_EQ(
        assembleVectorVI("vsrl.vi v1, v2, 3\n"),
        0xA221B0D7u
    );
    // vsrl.vi v1, v2, 0 -> 0xA22030D7
    EXPECT_EQ(
        assembleVectorVI("vsrl.vi v1, v2, 0\n"),
        0xA22030D7u
    );
    // vsrl.vi v1, v2, 31 -> 0xA22FB0D7
    EXPECT_EQ(
        assembleVectorVI("vsrl.vi v1, v2, 31\n"),
        0xA22FB0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsraVVCorrectly)
{
    // vsra.vv v1, v2, v3 -> funct6=101001, funct3=000 -> 0xA62180D7
    EXPECT_EQ(
        assembleVectorVV("vsra.vv v1, v2, v3\n"),
        0xA62180D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsraVXCorrectly)
{
    // vsra.vx v1, v2, x3 -> funct6=101001, funct3=100 -> 0xA621C0D7
    EXPECT_EQ(
        assembleVectorVX("vsra.vx v1, v2, x3\n"),
        0xA621C0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVsraVICorrectly)
{
    // vsra.vi v1, v2, 3 -> funct6=101001, funct3=011, uimm5=3 -> 0xA621B0D7
    EXPECT_EQ(
        assembleVectorVI("vsra.vi v1, v2, 3\n"),
        0xA621B0D7u
    );
    // vsra.vi v1, v2, 0 -> 0xA62030D7
    EXPECT_EQ(
        assembleVectorVI("vsra.vi v1, v2, 0\n"),
        0xA62030D7u
    );
    // vsra.vi v1, v2, 31 -> 0xA62FB0D7
    EXPECT_EQ(
        assembleVectorVI("vsra.vi v1, v2, 31\n"),
        0xA62FB0D7u
    );
}

TEST(VectorAssemblerTest, RejectsOutOfRangeShiftImmediate)
{
    Kites::setupVmStateDirectory();

    std::istringstream stream_sll_pos("vsll.vi v1, v2, 32\n");
    EXPECT_THROW(Kites::assemble(stream_sll_pos), std::runtime_error);

    std::istringstream stream_sll_neg("vsll.vi v1, v2, -1\n");
    EXPECT_THROW(Kites::assemble(stream_sll_neg), std::runtime_error);

    std::istringstream stream_srl_pos("vsrl.vi v1, v2, 32\n");
    EXPECT_THROW(Kites::assemble(stream_srl_pos), std::runtime_error);

    std::istringstream stream_srl_neg("vsrl.vi v1, v2, -1\n");
    EXPECT_THROW(Kites::assemble(stream_srl_neg), std::runtime_error);

    std::istringstream stream_sra_pos("vsra.vi v1, v2, 32\n");
    EXPECT_THROW(Kites::assemble(stream_sra_pos), std::runtime_error);

    std::istringstream stream_sra_neg("vsra.vi v1, v2, -1\n");
    EXPECT_THROW(Kites::assemble(stream_sra_neg), std::runtime_error);

    // Verify Batch 1 vadd.vi still rejects positive immediate > 15
    std::istringstream stream_vadd_31("vadd.vi v1, v2, 31\n");
    EXPECT_THROW(Kites::assemble(stream_vadd_31), std::runtime_error);
}

TEST(VectorAssemblerTest, EncodesVmulVVCorrectly)
{
    // vmul.vv v1, v2, v3 -> funct6=100101, funct3=010 -> 0x9621A0D7
    EXPECT_EQ(
        assembleVectorVV("vmul.vv v1, v2, v3\n"),
        0x9621A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulVXCorrectly)
{
    // vmul.vx v1, v2, x3 -> funct6=100101, funct3=110 -> 0x9621E0D7
    EXPECT_EQ(
        assembleVectorVX("vmul.vx v1, v2, x3\n"),
        0x9621E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhVVCorrectly)
{
    // vmulh.vv v1, v2, v3 -> funct6=100111, funct3=010 -> 0x9E21A0D7
    EXPECT_EQ(
        assembleVectorVV("vmulh.vv v1, v2, v3\n"),
        0x9E21A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhVXCorrectly)
{
    // vmulh.vx v1, v2, x3 -> funct6=100111, funct3=110 -> 0x9E21E0D7
    EXPECT_EQ(
        assembleVectorVX("vmulh.vx v1, v2, x3\n"),
        0x9E21E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhuVVCorrectly)
{
    // vmulhu.vv v1, v2, v3 -> funct6=100100, funct3=010 -> 0x9221A0D7
    EXPECT_EQ(
        assembleVectorVV("vmulhu.vv v1, v2, v3\n"),
        0x9221A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhuVXCorrectly)
{
    // vmulhu.vx v1, v2, x3 -> funct6=100100, funct3=110 -> 0x9221E0D7
    EXPECT_EQ(
        assembleVectorVX("vmulhu.vx v1, v2, x3\n"),
        0x9221E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhsuVVCorrectly)
{
    // vmulhsu.vv v1, v2, v3 -> funct6=100110, funct3=010 -> 0x9A21A0D7
    EXPECT_EQ(
        assembleVectorVV("vmulhsu.vv v1, v2, v3\n"),
        0x9A21A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVmulhsuVXCorrectly)
{
    // vmulhsu.vx v1, v2, x3 -> funct6=100110, funct3=110 -> 0x9A21E0D7
    EXPECT_EQ(
        assembleVectorVX("vmulhsu.vx v1, v2, x3\n"),
        0x9A21E0D7u
    );
}

TEST(VectorAssemblerTest, RejectsNonExistentMultiplyImmediate)
{
    Kites::setupVmStateDirectory();

    // vmul.vi does not exist in RVV 1.0
    std::istringstream stream_vmul_vi("vmul.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vmul_vi), std::runtime_error);

    std::istringstream stream_vmulh_vi("vmulh.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vmulh_vi), std::runtime_error);
}

TEST(VectorAssemblerTest, EncodesVdivuVVCorrectly)
{
    // vdivu.vv v1, v2, v3 -> funct6=100000, funct3=010 -> 0x8221A0D7
    EXPECT_EQ(
        assembleVectorVV("vdivu.vv v1, v2, v3\n"),
        0x8221A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVdivuVXCorrectly)
{
    // vdivu.vx v1, v2, x3 -> funct6=100000, funct3=110 -> 0x8221E0D7
    EXPECT_EQ(
        assembleVectorVX("vdivu.vx v1, v2, x3\n"),
        0x8221E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVdivVVCorrectly)
{
    // vdiv.vv v1, v2, v3 -> funct6=100001, funct3=010 -> 0x8621A0D7
    EXPECT_EQ(
        assembleVectorVV("vdiv.vv v1, v2, v3\n"),
        0x8621A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVdivVXCorrectly)
{
    // vdiv.vx v1, v2, x3 -> funct6=100001, funct3=110 -> 0x8621E0D7
    EXPECT_EQ(
        assembleVectorVX("vdiv.vx v1, v2, x3\n"),
        0x8621E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVremuVVCorrectly)
{
    // vremu.vv v1, v2, v3 -> funct6=100010, funct3=010 -> 0x8A21A0D7
    EXPECT_EQ(
        assembleVectorVV("vremu.vv v1, v2, v3\n"),
        0x8A21A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVremuVXCorrectly)
{
    // vremu.vx v1, v2, x3 -> funct6=100010, funct3=110 -> 0x8A21E0D7
    EXPECT_EQ(
        assembleVectorVX("vremu.vx v1, v2, x3\n"),
        0x8A21E0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVremVVCorrectly)
{
    // vrem.vv v1, v2, v3 -> funct6=100011, funct3=010 -> 0x8E21A0D7
    EXPECT_EQ(
        assembleVectorVV("vrem.vv v1, v2, v3\n"),
        0x8E21A0D7u
    );
}

TEST(VectorAssemblerTest, EncodesVremVXCorrectly)
{
    // vrem.vx v1, v2, x3 -> funct6=100011, funct3=110 -> 0x8E21E0D7
    EXPECT_EQ(
        assembleVectorVX("vrem.vx v1, v2, x3\n"),
        0x8E21E0D7u
    );
}

TEST(VectorAssemblerTest, RejectsNonExistentDivisionImmediate)
{
    Kites::setupVmStateDirectory();

    // vdiv.vi and vrem.vi do not exist in RVV 1.0
    std::istringstream stream_vdiv_vi("vdiv.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vdiv_vi), std::runtime_error);

    std::istringstream stream_vdivu_vi("vdivu.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vdivu_vi), std::runtime_error);

    std::istringstream stream_vrem_vi("vrem.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vrem_vi), std::runtime_error);

    std::istringstream stream_vremu_vi("vremu.vi v1, v2, 3\n");
    EXPECT_THROW(Kites::assemble(stream_vremu_vi), std::runtime_error);
}
