// Tests for the RVV GUI models (VectorRegisterModel, VectorCsrModel, MemoryModel highlighting)
// and for the vector_*.s example programs. The models are driven by a real RVSSProcessor, the
// same way MainWindow wires them, but no widgets are created.

#include <gtest/gtest.h>

#include <QObject>
#include <QString>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextDocument>
#include <cstdint>
#include <string>
#include <vector>

#include "assembler/assembler.h"
#include "common/assembled_program.h"
#include "common/globals.h"
#include "common/instructions.h"
#include "language_service/completion_provider.h"
#include "profiler/profiler.h"
#include "processor/registers.h"
#include "processor/rvss/rvss_processor.h"
#include "ui/common/syntax_highlighter.h"
#include "ui/memory_tab/memorymodel.h"
#include "ui/register_table/vectorcsrmodel.h"
#include "ui/register_table/vectorregistermodel.h"
#include "ui/theme/theme_manager.h"
#include "utils/utils.h"

using namespace Kites;

namespace
{
constexpr int kBackground = Qt::BackgroundRole;

// vtype for e<sew>, m1, ta/ma as given.
uint64_t Vtype(unsigned sew, bool ta = false, bool ma = false)
{
    uint64_t vsew = sew == 8 ? 0 : sew == 16 ? 1 : sew == 32 ? 2 : 3;
    return (vsew << 3) | (uint64_t{ta} << 6) | (uint64_t{ma} << 7);
}

RegisterFile::VectorRegister Bytes(std::initializer_list<uint8_t> low)
{
    RegisterFile::VectorRegister r{};
    size_t i = 0;
    for (uint8_t b : low)
        r[i++] = b;
    return r;
}

QString Text(const QAbstractItemModel &m, int row, int col, int role = Qt::DisplayRole)
{
    return m.data(m.index(row, col), role).toString();
}

// Counts dataChanged / reset notifications and remembers the rows touched by dataChanged.
struct ModelSpy
{
    int resets = 0;
    int dataChangedCount = 0;
    std::vector<int> rows;

    explicit ModelSpy(QAbstractItemModel &model)
    {
        QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [this] { ++resets; });
        QObject::connect(&model, &QAbstractItemModel::dataChanged, &model,
                         [this](const QModelIndex &tl, const QModelIndex &br)
                         {
                             ++dataChangedCount;
                             for (int r = tl.row(); r <= br.row(); ++r)
                                 rows.push_back(r);
                         });
    }
};

// RVSS processor with the three models attached the way MainWindow does it.
struct StateDirectory
{
    StateDirectory() { setupVmStateDirectory(); } // must exist before the processor dumps state
};

struct Gui
{
    StateDirectory stateDirectory;
    RVSSProcessor vm;
    VectorRegisterModel vregs{nullptr, &vm.registers_};
    VectorCsrModel csrs{nullptr, &vm.registers_};
    MemoryModel mem{nullptr, &vm.memory_controller_};

    explicit Gui(const std::string &example)
    {
        // MainWindow connects the manager's "state changed" signal to MemoryTab::onStepFinished.
        QObject::connect(&vm, &ProcessorBase::processorClockedSignal, &mem,
                         [this](const ProcessorState &) { mem.stepFinished(); });
        vm.LoadProgram(assemble("../examples/" + example));
        mem.clearHighlight(); // MainWindow does this right after loading a program
        mem.setRowsVisible(10);
    }

    void step(int n = 1)
    {
        for (int i = 0; i < n; ++i)
            vm.StepAndNotify();
    }
    void runAll()
    {
        while (vm.StepAndNotify())
        {
        }
    }

    // Row of the 8-byte memory row starting at `address` when the central address is `address`.
    int rowFor(uint64_t address)
    {
        mem.setCentralAddress(address);
        return 5; // rowsVisible = 10 -> the central address is displayed in row 5
    }
};

uint32_t Word(Gui &g, uint64_t address) { return g.vm.memory_controller_.readWord_d(address); }
} // namespace

// ---------------------------------------------------------------------------------------------
// VectorRegisterModel
// ---------------------------------------------------------------------------------------------
TEST(VectorRegisterModelTest, ShapeAndRawValue)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);

    EXPECT_EQ(model.rowCount(), 32);
    EXPECT_EQ(model.sewBits(), 8u); // vtype = 0 -> e8
    EXPECT_EQ(model.columnCount(), 2 + 16);
    EXPECT_EQ(Text(model, 0, 0), "v0");
    EXPECT_EQ(Text(model, 31, 0), "v31");

    rf.WriteVreg(3, Bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                           0x0D, 0x0E, 0x0F, 0xFF}));
    // 128 bits = 32 hex digits, most significant byte first.
    EXPECT_EQ(Text(model, 3, VectorRegisterModel::RawColumn), "0xFF0F0E0D0C0B0A090807060504030201");
    EXPECT_EQ(Text(model, 4, VectorRegisterModel::RawColumn), "0x00000000000000000000000000000000");
}

TEST(VectorRegisterModelTest, ElementCountFollowsSew)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);
    const std::pair<unsigned, int> cases[] = {{8, 16}, {16, 8}, {32, 4}, {64, 2}};
    for (auto [sew, count] : cases)
    {
        rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(sew));
        EXPECT_EQ(model.sewBits(), sew);
        EXPECT_EQ(model.elementCount(), count);
        EXPECT_EQ(model.columnCount(), 2 + count);
        EXPECT_EQ(model.headerData(2 + count - 1, Qt::Horizontal).toString(),
                  QString("e%1").arg(count - 1));
    }
}

TEST(VectorRegisterModelTest, ElementsForEverySewAndFormat)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);
    // bytes 0xFE 0xFF 0x01 0x80 0x00 0x00 0x00 0x80 | 0x7F 0 ... : mixes signs for each width
    rf.WriteVreg(7, Bytes({0xFE, 0xFF, 0x01, 0x80, 0x00, 0x00, 0x00, 0x80, 0x7F}));
    const int e0 = VectorRegisterModel::FirstElementColumn;

    // e8
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(8));
    EXPECT_EQ(Text(model, 7, e0 + 0), "0xFE");
    model.setDisplayFormat(VectorDisplayFormat::UnsignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "254");
    model.setDisplayFormat(VectorDisplayFormat::SignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "-2");
    EXPECT_EQ(Text(model, 7, e0 + 3), "-128");
    EXPECT_EQ(Text(model, 7, e0 + 8), "127");

    // e16: e0 = 0xFFFE, e1 = 0x8001
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(16));
    model.setDisplayFormat(VectorDisplayFormat::Hexadecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "0xFFFE");
    EXPECT_EQ(Text(model, 7, e0 + 1), "0x8001");
    model.setDisplayFormat(VectorDisplayFormat::UnsignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "65534");
    model.setDisplayFormat(VectorDisplayFormat::SignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "-2");
    EXPECT_EQ(Text(model, 7, e0 + 1), "-32767");

    // e32: e0 = 0x8001FFFE, e1 = 0x80000000? (bytes 4..7 = 00 00 00 80)
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(32));
    model.setDisplayFormat(VectorDisplayFormat::Hexadecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "0x8001FFFE");
    EXPECT_EQ(Text(model, 7, e0 + 1), "0x80000000");
    model.setDisplayFormat(VectorDisplayFormat::UnsignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 1), "2147483648");
    model.setDisplayFormat(VectorDisplayFormat::SignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 1), "-2147483648");

    // e64: e0 = 0x800000008001FFFE, e1 = 0x7F
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(64));
    model.setDisplayFormat(VectorDisplayFormat::Hexadecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "0x800000008001FFFE");
    EXPECT_EQ(Text(model, 7, e0 + 1), "0x000000000000007F");
    model.setDisplayFormat(VectorDisplayFormat::UnsignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "9223372039002390526");
    model.setDisplayFormat(VectorDisplayFormat::SignedDecimal);
    EXPECT_EQ(Text(model, 7, e0 + 0), "-9223372034707161090");
}

TEST(VectorRegisterModelTest, IllegalVtypeFallsBackToBytes)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(32));
    EXPECT_EQ(model.sewBits(), 32u);
    rf.WriteCsr(RegisterFile::CSR_VTYPE, uint64_t{1} << 63); // vill
    EXPECT_EQ(model.sewBits(), 8u);
}

TEST(VectorRegisterModelTest, SingleRegisterWriteRefreshesOnlyAffectedRows)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);
    ModelSpy spy(model);

    rf.WriteVreg(5, Bytes({1}));
    EXPECT_EQ(spy.resets, 0); // no full-table reset
    EXPECT_EQ(model.highlightedRegister(), 5);
    ASSERT_FALSE(spy.rows.empty());
    for (int r : spy.rows)
        EXPECT_EQ(r, 5);
    EXPECT_TRUE(model.data(model.index(5, 1), kBackground).isValid());
    EXPECT_FALSE(model.data(model.index(4, 1), kBackground).isValid());

    spy.rows.clear();
    rf.WriteVreg(9, Bytes({2})); // moves the highlight: only rows 5 (clear) and 9 change
    EXPECT_EQ(spy.resets, 0);
    for (int r : spy.rows)
        EXPECT_TRUE(r == 5 || r == 9);
    EXPECT_FALSE(model.data(model.index(5, 1), kBackground).isValid());
    EXPECT_TRUE(model.data(model.index(9, 1), kBackground).isValid());
}

TEST(VectorRegisterModelTest, SewChangeAndResetAndFormatChange)
{
    RegisterFile rf;
    VectorRegisterModel model(nullptr, &rf);
    ModelSpy spy(model);

    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(32)); // layout change -> reset
    EXPECT_EQ(spy.resets, 1);
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(32, true)); // same SEW, only ta changed
    EXPECT_EQ(spy.resets, 1);

    spy.dataChangedCount = 0;
    model.setDisplayFormat(VectorDisplayFormat::SignedDecimal); // no reset needed
    EXPECT_EQ(spy.resets, 1);
    EXPECT_EQ(spy.dataChangedCount, 1);

    rf.WriteVreg(2, Bytes({9}));
    EXPECT_EQ(model.highlightedRegister(), 2);
    rf.Reset();
    EXPECT_EQ(model.highlightedRegister(), -1);
    EXPECT_EQ(model.sewBits(), 8u);
    EXPECT_EQ(Text(model, 2, VectorRegisterModel::RawColumn), "0x00000000000000000000000000000000");
}

TEST(VectorRegisterModelTest, ChangeRegisterFileRebindsWithoutTouchingOldOne)
{
    auto first = std::make_unique<RegisterFile>();
    VectorRegisterModel model(nullptr, first.get());
    first.reset(); // the processor (and its register file) can be destroyed on a processor change

    RegisterFile second;
    second.WriteVreg(1, Bytes({0xAB}));
    model.changeRegisterFile(&second);
    EXPECT_EQ(Text(model, 1, VectorRegisterModel::RawColumn), "0x000000000000000000000000000000AB");
    second.WriteVreg(2, Bytes({1}));
    EXPECT_EQ(model.highlightedRegister(), 2);
}

// ---------------------------------------------------------------------------------------------
// VectorCsrModel
// ---------------------------------------------------------------------------------------------
TEST(VectorCsrModelTest, ReadsRegisterFileAndDecodesVtype)
{
    RegisterFile rf;
    VectorCsrModel model(nullptr, &rf);
    EXPECT_EQ(model.rowCount(), 9);

    // Initial state: vl=0, vstart=0, vtype=0 (e8,m1,tu,mu), vlenb=16
    EXPECT_EQ(Text(model, VectorCsrModel::Vl, 1), "0");
    EXPECT_EQ(Text(model, VectorCsrModel::Vtype, 1), "0x0");
    EXPECT_EQ(Text(model, VectorCsrModel::Vlenb, 1), "16");
    EXPECT_EQ(Text(model, VectorCsrModel::Sew, 1), "e8");
    EXPECT_EQ(Text(model, VectorCsrModel::Vlmax, 1), "16");

    int changes = 0;
    QObject::connect(&model, &QAbstractItemModel::dataChanged, &model, [&] { ++changes; });
    rf.WriteCsr(RegisterFile::CSR_VL, 3);
    rf.WriteCsr(RegisterFile::CSR_VSTART, 1);
    rf.WriteCsr(RegisterFile::CSR_VTYPE, Vtype(32, true, true)); // e32, m1, ta, ma
    EXPECT_EQ(changes, 3);
    EXPECT_EQ(Text(model, VectorCsrModel::Vl, 1), "3");
    EXPECT_EQ(Text(model, VectorCsrModel::Vstart, 1), "1");
    EXPECT_EQ(Text(model, VectorCsrModel::Vtype, 1), "0xD0");
    EXPECT_EQ(Text(model, VectorCsrModel::Sew, 1), "e32");
    EXPECT_EQ(Text(model, VectorCsrModel::Lmul, 1), "m1");
    EXPECT_EQ(Text(model, VectorCsrModel::Vlmax, 1), "4");
    EXPECT_EQ(Text(model, VectorCsrModel::Vta, 1), "agnostic");
    EXPECT_EQ(Text(model, VectorCsrModel::Vma, 1), "agnostic");

    // Writes to unrelated CSRs do not notify.
    changes = 0;
    rf.WriteCsr(0x003, 1);
    EXPECT_EQ(changes, 0);

    rf.WriteCsr(RegisterFile::CSR_VTYPE, uint64_t{1} << 63); // vill
    EXPECT_EQ(Text(model, VectorCsrModel::Sew, 1), "-");
    EXPECT_EQ(Text(model, VectorCsrModel::Vtype, 1), "0x8000000000000000");
}

// ---------------------------------------------------------------------------------------------
// MemoryModel highlighting
// ---------------------------------------------------------------------------------------------
TEST(MemoryModelHighlightTest, UnalignedByteRefreshesItsOwnRowOnly)
{
    MemoryController mc;
    MemoryModel model(nullptr, &mc);
    model.setRowsVisible(10);
    model.setCentralAddress(0x200);
    ModelSpy spy(model);

    // 0x203 lives in the row starting at 0x200 (row 5); 0x20B in the row starting at 0x208 (row 4).
    mc.writeByte(0x203, 0xAA);
    ASSERT_EQ(spy.rows.size(), 1u);
    EXPECT_EQ(spy.rows[0], 5);
    spy.rows.clear();
    spy.dataChangedCount = 0;
    mc.writeByte(0x20B, 0xBB);
    EXPECT_EQ(spy.resets, 0);
    EXPECT_EQ(spy.dataChangedCount, 1);
    ASSERT_EQ(spy.rows.size(), 1u);
    EXPECT_EQ(spy.rows[0], 4);

    // Off-screen address: nothing to refresh.
    spy.dataChangedCount = 0;
    mc.writeByte(0x8000, 1);
    EXPECT_EQ(spy.dataChangedCount, 0);
}

TEST(MemoryModelHighlightTest, HighlightsOnlyModifiedBytesAndClearsNextStep)
{
    MemoryController mc;
    MemoryModel model(nullptr, &mc);
    model.setRowsVisible(10);
    model.setCentralAddress(0x200);

    mc.writeByte(0x202, 0x11); // byte column = 2 + 2
    EXPECT_TRUE(model.data(model.index(5, 4), kBackground).isValid());
    EXPECT_FALSE(model.data(model.index(5, 3), kBackground).isValid());
    EXPECT_FALSE(model.data(model.index(5, 5), kBackground).isValid());
    EXPECT_TRUE(model.data(model.index(5, 1), kBackground).isValid());  // double word cell
    EXPECT_FALSE(model.data(model.index(5, 0), kBackground).isValid()); // address cell

    model.stepFinished();                                       // end of the step that wrote it
    EXPECT_TRUE(model.data(model.index(5, 4), kBackground).isValid()); // still shown
    mc.writeByte(0x205, 0x22);                                  // next step writes elsewhere
    EXPECT_FALSE(model.data(model.index(5, 4), kBackground).isValid());
    EXPECT_TRUE(model.data(model.index(5, 7), kBackground).isValid());
    model.stepFinished();
    model.stepFinished(); // a step that wrote nothing clears the highlight
    EXPECT_FALSE(model.data(model.index(5, 7), kBackground).isValid());
}

TEST(MemoryModelHighlightTest, DisplayFormatsAndSearchUnaffected)
{
    MemoryController mc;
    MemoryModel model(nullptr, &mc);
    model.setRowsVisible(10);
    mc.writeByte_d(0x200, 0x2A);
    model.setCentralAddress(0x200);
    EXPECT_EQ(Text(model, 5, 0), "0x200");
    EXPECT_EQ(Text(model, 5, 2), "0x2A");
    model.setDisplayBase(Base::Decimal);
    EXPECT_EQ(Text(model, 5, 2), "42");
    model.setDisplayBase(Base::Binary);
    EXPECT_EQ(Text(model, 5, 2), "0b101010");
}

// ---------------------------------------------------------------------------------------------
// Example programs, driven with the models attached (execution, undo, redo, reset)
// ---------------------------------------------------------------------------------------------
TEST(VectorExamplesTest, Basic)
{
    Gui g("vector_basic.s");
    g.step(14); // up to and including the first vadd.vi (instruction 14 = vadd.vi v3)
    g.runAll();

    EXPECT_EQ(g.vm.registers_.ReadGpr(5), 16u);
    EXPECT_EQ(g.vm.registers_.ReadCsr(RegisterFile::CSR_VL), 16u);
    EXPECT_EQ(g.vm.registers_.ReadCsr(RegisterFile::CSR_VTYPE), 0xC0u);

    // Now e8: v3 = 0C 00 00 00 0E 00 00 00 10 00 00 00 12 00 00 00
    EXPECT_EQ(g.vregs.sewBits(), 8u);
    EXPECT_EQ(Text(g.vregs, 3, VectorRegisterModel::RawColumn), "0x00000012000000100000000E0000000C");
    EXPECT_EQ(Text(g.vregs, 3, VectorRegisterModel::FirstElementColumn + 0), "0x0C");
    EXPECT_EQ(Text(g.vregs, 3, VectorRegisterModel::FirstElementColumn + 4), "0x0E");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vl, 1), "16");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vtype, 1), "0xC0");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vlmax, 1), "16");

    // v1, v2 as e32 in the register file
    EXPECT_EQ(g.vm.registers_.ReadVregElement<uint32_t>(1, 3), 4u);
    EXPECT_EQ(g.vm.registers_.ReadVregElement<uint32_t>(2, 3), 8u);
    EXPECT_EQ(g.vm.registers_.ReadVregElement<uint32_t>(3, 3), 18u);
}

TEST(VectorExamplesTest, BasicUndoRedoResetKeepModelsInSync)
{
    Gui g("vector_basic.s");
    // 8 sw/addi pairs (8 instr + 1 addi x10) ... run until the e32 vsetvli and the vle32 are done
    g.step(11);
    EXPECT_EQ(g.vregs.sewBits(), 32u);
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Sew, 1), "e32");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vl, 1), "4");

    g.step(2); // vle32.v v1, vadd.vv v2
    const QString v2Raw = Text(g.vregs, 2, VectorRegisterModel::RawColumn);
    EXPECT_EQ(v2Raw, "0x00000008000000060000000400000002");
    EXPECT_EQ(g.vregs.highlightedRegister(), 2);

    g.vm.Undo(); // undo vadd.vv v2
    EXPECT_EQ(Text(g.vregs, 2, VectorRegisterModel::RawColumn), "0x00000000000000000000000000000000");
    g.vm.Redo();
    EXPECT_EQ(Text(g.vregs, 2, VectorRegisterModel::RawColumn), v2Raw);

    // undo across the vsetvli: SEW goes back to e8 and CSR rows follow
    for (int i = 0; i < 12; ++i)
        g.vm.Undo();
    EXPECT_EQ(g.vregs.sewBits(), 8u);
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vl, 1), "0");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vtype, 1), "0x0");
    for (int i = 0; i < 12; ++i)
        g.vm.Redo();
    EXPECT_EQ(g.vregs.sewBits(), 32u);
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vl, 1), "4");

    g.vm.registers_.Reset();
    EXPECT_EQ(Text(g.vregs, 2, VectorRegisterModel::RawColumn), "0x00000000000000000000000000000000");
    EXPECT_EQ(Text(g.csrs, VectorCsrModel::Vl, 1), "0");
    EXPECT_EQ(g.vregs.highlightedRegister(), -1);
}

TEST(VectorExamplesTest, MemoryStoreUpdatesAndHighlightsMemoryUndoRedo)
{
    Gui g("vector_memory.s");
    const int storeIndex = 14; // 2 addi, 4x(addi+sw), addi x6, vsetvli, vle32, vadd.vi; next is vse32
    g.step(storeIndex);
    g.rowFor(0x200);
    EXPECT_EQ(Word(g, 0x200), 0u); // not stored yet
    EXPECT_FALSE(g.mem.data(g.mem.index(5, 2), kBackground).isValid());

    g.step(1); // vse32.v v1, (x11)
    EXPECT_EQ(Word(g, 0x200), 15u);
    EXPECT_EQ(Word(g, 0x204), 25u);
    EXPECT_EQ(Word(g, 0x208), 35u);
    EXPECT_EQ(Word(g, 0x20C), 45u);
    EXPECT_EQ(g.vm.memory_controller_.readDoubleWord_d(0x200), 0x000000190000000FULL);

    // Highlight: every byte written by the store has a non-zero value here; 0x200 row and 0x208 row.
    // Only bytes that were actually written are highlighted (all 16 bytes of the store, even
    // zero-valued ones, are written byte by byte).
    for (int col = 2; col < 10; ++col)
        EXPECT_TRUE(g.mem.data(g.mem.index(5, col), kBackground).isValid()) << col;
    for (int col = 2; col < 10; ++col)
        EXPECT_TRUE(g.mem.data(g.mem.index(4, col), kBackground).isValid()) << col;
    EXPECT_FALSE(g.mem.data(g.mem.index(3, 2), kBackground).isValid()); // 0x210 untouched
    EXPECT_EQ(Text(g.mem, 5, 1), "0x190000000F");

    g.vm.Undo(); // restores old bytes (all zero) and re-highlights the restored bytes
    EXPECT_EQ(Word(g, 0x200), 0u);
    EXPECT_EQ(Word(g, 0x20C), 0u);
    EXPECT_EQ(Text(g.mem, 5, 1), "0x0");
    g.vm.Redo();
    EXPECT_EQ(Word(g, 0x200), 15u);
    EXPECT_EQ(Text(g.mem, 5, 1), "0x190000000F");
    EXPECT_EQ(Text(g.mem, 4, 1), "0x2D00000023");

    g.step(1); // vle32.v v2 reads the stored data back; no memory writes: highlight is dropped
    EXPECT_FALSE(g.mem.data(g.mem.index(5, 2), kBackground).isValid());
    EXPECT_EQ(g.vm.registers_.ReadVregElement<uint32_t>(2, 0), 15u);
    EXPECT_EQ(g.vm.registers_.ReadVregElement<uint32_t>(2, 3), 45u);
    EXPECT_EQ(g.vm.registers_.ReadGpr(5), 4u);
}

TEST(VectorExamplesTest, Mask)
{
    Gui g("vector_mask.s");
    g.runAll();
    const auto &rf = g.vm.registers_;
    EXPECT_EQ(rf.ReadVregElement<uint8_t>(0, 0) & 0x0F, 0x05);
    EXPECT_EQ(rf.ReadVregElement<uint8_t>(4, 0) & 0x0F, 0x0F);
    EXPECT_EQ(rf.ReadVregElement<uint8_t>(5, 0) & 0x0F, 0x05);
    EXPECT_EQ(rf.ReadVregElement<uint8_t>(6, 0) & 0x0F, 0x0A);
    EXPECT_EQ(rf.ReadVregElement<uint8_t>(7, 0) & 0x0F, 0x01);
    EXPECT_EQ(rf.ReadGpr(5), 4u);
}

TEST(VectorExamplesTest, Strided)
{
    Gui g("vector_strided.s");
    g.runAll();
    const auto &rf = g.vm.registers_;
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(rf.ReadVregElement<uint32_t>(1, i), i + 1) << i;
    EXPECT_EQ(g.vm.memory_controller_.readDoubleWord_d(0x200), 0x0000000200000001ULL);
    EXPECT_EQ(g.vm.memory_controller_.readDoubleWord_d(0x208), 0x0000000400000003ULL);
    EXPECT_EQ(rf.ReadGpr(5), 4u);
}

TEST(VectorExamplesTest, Reduction)
{
    Gui g("vector_reduction.s");
    g.runAll();
    const auto &rf = g.vm.registers_;
    EXPECT_EQ(rf.ReadVregElement<uint32_t>(4, 0), 32u);
    EXPECT_EQ(rf.ReadVregElement<uint32_t>(5, 0), 10u);
    EXPECT_EQ(rf.ReadVregElement<uint32_t>(6, 0), 3u);
    EXPECT_EQ(rf.ReadGpr(5), 4u);
}

// ---------------------------------------------------------------------------------------------
// Profiler: vector instructions used to be classified UNKNOWN, which indexed one past the end of
// the instruction-type count array (heap corruption -> the GUI closed on the first vadd).
// ---------------------------------------------------------------------------------------------
TEST(VectorProfilerTest, VectorInstructionsHaveATypeAndDoNotOverflowCounts)
{
    using instruction_set::InstructionType;
    EXPECT_EQ(instruction_set::getInstructionType("vadd.vv"), InstructionType::V_TYPE);
    EXPECT_EQ(instruction_set::getInstructionType("vle32.v"), InstructionType::V_TYPE);
    EXPECT_EQ(instruction_set::getInstructionType("vredsum.vs"), InstructionType::V_TYPE);
    EXPECT_EQ(instruction_set::getInstructionType("add"), InstructionType::R_TYPE);
    EXPECT_EQ(instruction_set::getInstructionType("bogus"), InstructionType::UNKNOWN);

    Profiler profiler;
    profiler.setLineNumberToInstructionTypeMapping(assemble("../examples/vector_mask.s"));
    const auto &counts = profiler.getInstructionTypeCounts();
    EXPECT_GT(counts[toIndex(InstructionType::V_TYPE)], 0u);
    EXPECT_EQ(instruction_set::instructionTypeNames[toIndex(InstructionType::V_TYPE)], "V-Type");
}

// ---------------------------------------------------------------------------------------------
// Syntax Highlighting & Instructions Database for RVV
// ---------------------------------------------------------------------------------------------
TEST(VectorSyntaxHighlighterTest, InstructionsJsonContainsAllSupportedRvvInstructions)
{
    QFile file(":/instructions/instructions.json");
    ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << file.errorString().toStdString();
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    ASSERT_FALSE(doc.isNull());
    const QJsonObject root = doc.object();
    ASSERT_TRUE(root.contains("VInstruction"));
    const QJsonArray vInsts = root.value("VInstruction").toArray();
    EXPECT_EQ(vInsts.size(), 100);

    for (const auto &val : vInsts)
    {
        const std::string mnemonic = val.toString().toStdString();
        EXPECT_TRUE(instruction_set::isValidInstruction(mnemonic)) << "Invalid instruction: " << mnemonic;
        EXPECT_TRUE(instruction_set::getInstructionType(mnemonic) == instruction_set::InstructionType::V_TYPE || mnemonic == "vsetvli")
            << "Not V_TYPE or vsetvli: " << mnemonic;
    }
}

TEST(VectorSyntaxHighlighterTest, ThemeManagerHasVInstructionFormat)
{
    const auto &tm = ThemeManager::getInstance();
    const QTextCharFormat vFormat = tm.getSyntaxFormat(SyntaxStyle::VInstruction);
    EXPECT_TRUE(vFormat.foreground().color().isValid());
    EXPECT_EQ(vFormat.fontWeight(), QFont::Bold);

    const QTextCharFormat r32Format = tm.getSyntaxFormat(SyntaxStyle::R32Instruction);
    EXPECT_TRUE(r32Format.foreground().color().isValid());
    EXPECT_NE(vFormat.foreground().color(), QColor(Qt::black));
}

TEST(VectorSyntaxHighlighterTest, CompletionProviderIncludesRvvMnemonics)
{
    const auto &completions = CompletionProvider::getInstance().staticSymbols();
    EXPECT_TRUE(completions.contains("vsetvli"));
    EXPECT_TRUE(completions.contains("vadd.vv"));
    EXPECT_TRUE(completions.contains("vle32.v"));
    EXPECT_TRUE(completions.contains("vse32.v"));
    EXPECT_TRUE(completions.contains("vredsum.vs"));
}

TEST(VectorSyntaxHighlighterTest, SyntaxHighlighterAppliesVectorStyles)
{
    QTextDocument doc;
    SyntaxHighlighter highlighter(&doc);
    doc.setPlainText("vsetvli t0, a0, e32, m1, ta, ma\n"
                     "vadd.vv v1, v2, v3\n"
                     "# comment with vadd.vv\n"
                     "add x1, x2, x3");

    // Force highlighting of the document
    highlighter.rehighlight();
    ASSERT_EQ(doc.blockCount(), 4);

    const auto &tm = ThemeManager::getInstance();
    const QColor vColor = tm.getSyntaxFormat(SyntaxStyle::VInstruction).foreground().color();
    const QColor r32Color = tm.getSyntaxFormat(SyntaxStyle::R32Instruction).foreground().color();
    const QColor regColor = tm.getSyntaxFormat(SyntaxStyle::Register).foreground().color();
    const QColor commentColor = tm.getSyntaxFormat(SyntaxStyle::Comment).foreground().color();

    // Line 0: vsetvli t0, a0, e32, m1, ta, ma
    QTextBlock block0 = doc.findBlockByLineNumber(0);
    const auto formats0 = block0.layout()->formats();
    ASSERT_FALSE(formats0.isEmpty());
    EXPECT_EQ(formats0[0].start, 0);
    EXPECT_EQ(formats0[0].length, 7); // "vsetvli"
    EXPECT_EQ(formats0[0].format.foreground().color(), vColor);

    // Line 1: vadd.vv v1, v2, v3
    QTextBlock block1 = doc.findBlockByLineNumber(1);
    const auto formats1 = block1.layout()->formats();
    ASSERT_GE(formats1.size(), 1);
    EXPECT_EQ(formats1[0].start, 0);
    EXPECT_EQ(formats1[0].length, 7); // "vadd.vv"
    EXPECT_EQ(formats1[0].format.foreground().color(), vColor);

    // Verify v1, v2, v3 receive register formatting
    bool foundV1 = false;
    for (const auto &range : formats1)
    {
        if (range.start == 8 && range.length == 2) // "v1"
        {
            EXPECT_EQ(range.format.foreground().color(), regColor);
            foundV1 = true;
        }
    }
    EXPECT_TRUE(foundV1);

    // Line 2: # comment with vadd.vv
    QTextBlock block2 = doc.findBlockByLineNumber(2);
    const auto formats2 = block2.layout()->formats();
    ASSERT_FALSE(formats2.isEmpty());
    EXPECT_EQ(formats2.last().format.foreground().color(), commentColor);

    // Line 3: add x1, x2, x3
    QTextBlock block3 = doc.findBlockByLineNumber(3);
    const auto formats3 = block3.layout()->formats();
    ASSERT_FALSE(formats3.isEmpty());
    EXPECT_EQ(formats3[0].start, 0);
    EXPECT_EQ(formats3[0].length, 3); // "add"
    EXPECT_EQ(formats3[0].format.foreground().color(), r32Color);
}
