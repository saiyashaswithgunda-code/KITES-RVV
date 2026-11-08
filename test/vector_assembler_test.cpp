
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