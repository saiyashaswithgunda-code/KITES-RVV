
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
