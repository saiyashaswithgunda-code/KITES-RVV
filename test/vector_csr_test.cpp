
#include <gtest/gtest.h>
#include "processor/registers.h"

namespace Kites
{

TEST(VectorCsrTest, VstartInitiallyZero)
{
    RegisterFile registers;
    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VSTART), 0);
}

TEST(VectorCsrTest, VlInitiallyZero)
{
    RegisterFile registers;
    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VL), 0);
}

TEST(VectorCsrTest, VtypeInitiallyZero)
{
    RegisterFile registers;
    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VTYPE), 0);
}

TEST(VectorCsrTest, VlenbEqualsVlenBytes)
{
    RegisterFile registers;
    EXPECT_EQ(
        registers.ReadCsr(RegisterFile::CSR_VLENB),
        RegisterFile::VLEN_BYTES
    );
}

TEST(VectorCsrTest, ResetRestoresVectorCsrDefaults)
{
    RegisterFile registers;

    registers.WriteCsr(RegisterFile::CSR_VSTART, 5);
    registers.WriteCsr(RegisterFile::CSR_VL, 8);
    registers.WriteCsr(RegisterFile::CSR_VTYPE, 0x18);
    registers.WriteCsr(RegisterFile::CSR_VLENB, 32);

    registers.Reset();

    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VSTART), 0);
    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VL), 0);
    EXPECT_EQ(registers.ReadCsr(RegisterFile::CSR_VTYPE), 0);
    EXPECT_EQ(
        registers.ReadCsr(RegisterFile::CSR_VLENB),
        RegisterFile::VLEN_BYTES
    );
}

} // namespace Kites
