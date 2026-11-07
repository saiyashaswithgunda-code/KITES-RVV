#include <gtest/gtest.h>

#include "processor/vector_config.h"

namespace Kites
{

TEST(VectorConfigTest, SupportsSEW8)
{
    auto config = DecodeVectorConfig(0b000);

    EXPECT_FALSE(config.vill);
    EXPECT_EQ(config.sew_bits, 8);
    EXPECT_EQ(config.lmul, 1);
    EXPECT_EQ(config.vlmax, 16);
}

TEST(VectorConfigTest, SupportsSEW16)
{
    auto config = DecodeVectorConfig(0b001 << 3);

    EXPECT_FALSE(config.vill);
    EXPECT_EQ(config.sew_bits, 16);
    EXPECT_EQ(config.vlmax, 8);
}

TEST(VectorConfigTest, SupportsSEW32)
{
    auto config = DecodeVectorConfig(0b010 << 3);

    EXPECT_FALSE(config.vill);
    EXPECT_EQ(config.sew_bits, 32);
    EXPECT_EQ(config.vlmax, 4);
}

TEST(VectorConfigTest, SupportsSEW64)
{
    auto config = DecodeVectorConfig(0b011 << 3);

    EXPECT_FALSE(config.vill);
    EXPECT_EQ(config.sew_bits, 64);
    EXPECT_EQ(config.vlmax, 2);
}

TEST(VectorConfigTest, SupportsTailAndMaskAgnostic)
{
    uint64_t vtype = (1ULL << 6) | (1ULL << 7);

    auto config = DecodeVectorConfig(vtype);

    EXPECT_FALSE(config.vill);
    EXPECT_TRUE(config.tail_agnostic);
    EXPECT_TRUE(config.mask_agnostic);
}

TEST(VectorConfigTest, RejectsUnsupportedLMUL)
{
    auto config = DecodeVectorConfig(0b001);

    EXPECT_TRUE(config.vill);
    EXPECT_EQ(config.vlmax, 0);
}

TEST(VectorConfigTest, RejectsUnsupportedSEW)
{
    auto config = DecodeVectorConfig(0b100 << 3);

    EXPECT_TRUE(config.vill);
    EXPECT_EQ(config.vlmax, 0);
}

TEST(VectorConfigTest, RejectsReservedBits)
{
    auto config = DecodeVectorConfig(1ULL << 8);

    EXPECT_TRUE(config.vill);
}

TEST(VectorConfigTest, RejectsVillBit)
{
    auto config = DecodeVectorConfig(1ULL << 63);

    EXPECT_TRUE(config.vill);
}
TEST(VectorConfigTest, VectorLengthEqualsAVLWhenBelowVLMAX)
{
    EXPECT_EQ(CalculateVectorLength(5, 16), 5);
}

TEST(VectorConfigTest, VectorLengthEqualsVLMAXWhenAVLIsLarger)
{
    EXPECT_EQ(CalculateVectorLength(20, 16), 16);
}

TEST(VectorConfigTest, VectorLengthEqualsVLMAXWhenAVLIsEqual)
{
    EXPECT_EQ(CalculateVectorLength(16, 16), 16);
}

TEST(VectorConfigTest, VectorLengthIsZeroWhenAVLIsZero)
{
    EXPECT_EQ(CalculateVectorLength(0, 16), 0);
}

} // namespace Kites