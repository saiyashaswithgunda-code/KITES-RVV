#include <gtest/gtest.h>
#include "processor/registers.h"

#include <array>
#include <cstdint>
#include <stdexcept>

using namespace Kites;

TEST(VectorRegisterTest, InitiallyAllZero)
{
    RegisterFile registers;

    for (size_t i = 0; i < RegisterFile::NUM_VREG; ++i)
    {
        auto value = registers.ReadVreg(i);

        EXPECT_EQ(value, RegisterFile::VectorRegister{});
    }
}

TEST(VectorRegisterTest, WriteAndRead)
{
    RegisterFile registers;

    RegisterFile::VectorRegister value = {
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16
    };

    registers.WriteVreg(5, value);

    EXPECT_EQ(registers.ReadVreg(5), value);
}

TEST(VectorRegisterTest, AllRegistersAreIndependent)
{
    RegisterFile registers;

    for (size_t i = 0; i < RegisterFile::NUM_VREG; ++i)
    {
        RegisterFile::VectorRegister value{};
        value.fill(static_cast<uint8_t>(i));

        registers.WriteVreg(i, value);
    }

    for (size_t i = 0; i < RegisterFile::NUM_VREG; ++i)
    {
        RegisterFile::VectorRegister expected{};
        expected.fill(static_cast<uint8_t>(i));

        EXPECT_EQ(registers.ReadVreg(i), expected);
    }
}

TEST(VectorRegisterTest, InvalidReadThrows)
{
    RegisterFile registers;

    EXPECT_THROW(registers.ReadVreg(32), std::out_of_range);
}

TEST(VectorRegisterTest, InvalidWriteThrows)
{
    RegisterFile registers;

    RegisterFile::VectorRegister value{};

    EXPECT_THROW(registers.WriteVreg(32, value), std::out_of_range);
}

TEST(VectorRegisterTest, ResetClearsVectorRegisters)
{
    RegisterFile registers;

    RegisterFile::VectorRegister value{};
    value.fill(255);

    for (size_t i = 0; i < RegisterFile::NUM_VREG; ++i)
    {
        registers.WriteVreg(i, value);
    }

    registers.Reset();

    for (size_t i = 0; i < RegisterFile::NUM_VREG; ++i)
    {
        EXPECT_EQ(registers.ReadVreg(i), RegisterFile::VectorRegister{});
    }
}

TEST(VectorRegisterTest, ReadWrite8BitElements)
{
    RegisterFile registers;

    registers.WriteVregElement<uint8_t>(0, 0, 255);
    registers.WriteVregElement<uint8_t>(0, 15, 128);

    EXPECT_EQ(registers.ReadVregElement<uint8_t>(0, 0), 255);
    EXPECT_EQ(registers.ReadVregElement<uint8_t>(0, 15), 128);
}

TEST(VectorRegisterTest, ReadWrite16BitElements)
{
    RegisterFile registers;

    registers.WriteVregElement<uint16_t>(1, 0, 1000);
    registers.WriteVregElement<uint16_t>(1, 7, 65535);

    EXPECT_EQ(registers.ReadVregElement<uint16_t>(1, 0), 1000);
    EXPECT_EQ(registers.ReadVregElement<uint16_t>(1, 7), 65535);
}

TEST(VectorRegisterTest, ReadWrite32BitElements)
{
    RegisterFile registers;

    registers.WriteVregElement<uint32_t>(2, 0, 123456789);
    registers.WriteVregElement<uint32_t>(2, 3, 0xFFFFFFFF);

    EXPECT_EQ(registers.ReadVregElement<uint32_t>(2, 0), 123456789);
    EXPECT_EQ(registers.ReadVregElement<uint32_t>(2, 3), 0xFFFFFFFF);
}

TEST(VectorRegisterTest, ReadWrite64BitElements)
{
    RegisterFile registers;

    registers.WriteVregElement<uint64_t>(3, 0, 0x123456789ABCDEF0ULL);
    registers.WriteVregElement<uint64_t>(3, 1, 0xFFFFFFFFFFFFFFFFULL);

    EXPECT_EQ(registers.ReadVregElement<uint64_t>(3, 0), 0x123456789ABCDEF0ULL);
    EXPECT_EQ(registers.ReadVregElement<uint64_t>(3, 1), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(VectorRegisterTest, InvalidElementReadThrows)
{
    RegisterFile registers;

    EXPECT_THROW(
        registers.ReadVregElement<uint32_t>(0, 4),
        std::out_of_range
    );
}

TEST(VectorRegisterTest, InvalidElementWriteThrows)
{
    RegisterFile registers;

    EXPECT_THROW(
        registers.WriteVregElement<uint16_t>(0, 8, 100),
        std::out_of_range
    );
}