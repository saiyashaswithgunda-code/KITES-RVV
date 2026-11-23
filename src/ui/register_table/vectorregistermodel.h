#pragma once
#include "processor/registers.h"
#include <QAbstractTableModel>

namespace Kites
{

/**
 * @brief How the individual vector elements are rendered by VectorRegisterModel.
 */
enum class VectorDisplayFormat
{
    Hexadecimal,
    UnsignedDecimal,
    SignedDecimal
};

/**
 * @brief Read-only model showing the 32 vector registers (v0-v31).
 *
 * Columns: register name, the raw 128-bit value (always hexadecimal) and then one column per
 * element. The element count is VLEN / SEW, where SEW is decoded from the current vtype, so the
 * column count changes whenever vsetvli changes the SEW. The model keeps no architectural state:
 * every value is read from the RegisterFile on demand.
 */
class VectorRegisterModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    static constexpr int NameColumn = 0;
    static constexpr int RawColumn = 1;
    static constexpr int FirstElementColumn = 2;

    explicit VectorRegisterModel(QObject *parent = nullptr, RegisterFile *regfile = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setDisplayFormat(VectorDisplayFormat format);
    VectorDisplayFormat displayFormat() const { return m_displayFormat; }
    void changeRegisterFile(RegisterFile *regfile);

    /// Element width in bits currently used to split the registers (8, 16, 32 or 64).
    unsigned sewBits() const { return m_sewBits; }
    /// Number of elements per register for the current SEW (VLEN / SEW).
    int elementCount() const;
    /// Row of the most recently written register, or -1 if none.
    int highlightedRegister() const { return m_highlightedRegister; }

  public slots:
    void updateVRegisterValue(size_t regIndex);
    void vectorCsrChangedSlot();
    void registerResetSlot();
    void stepFinished();

  private:
    void connectRegisterFile();
    unsigned decodeSewBits() const;
    void emitRowChanged(int row);

    RegisterFile *m_registerFile = nullptr;
    VectorDisplayFormat m_displayFormat = VectorDisplayFormat::Hexadecimal;
    unsigned m_sewBits = 8;
    int m_highlightedRegister = -1;
    bool m_stepHadWrites = false;
};

} // namespace Kites
