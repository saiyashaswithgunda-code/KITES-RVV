#include "vectorregistermodel.h"
#include "processor/vector_config.h"
#include <QBrush>
#include <QColor>
#include <QFontDatabase>

namespace Kites
{
namespace
{
// Little-endian element `element` of width `sewBits` taken from the register's raw bytes.
uint64_t readElement(const RegisterFile::VectorRegister &bytes, unsigned sewBits, int element)
{
    const size_t width = sewBits / 8;
    const size_t offset = static_cast<size_t>(element) * width;
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    return value;
}

int64_t signExtend(uint64_t value, unsigned sewBits)
{
    if (sewBits >= 64)
        return static_cast<int64_t>(value);
    const uint64_t signBit = uint64_t{1} << (sewBits - 1);
    return static_cast<int64_t>((value ^ signBit) - signBit);
}

QString formatElement(uint64_t value, unsigned sewBits, VectorDisplayFormat format)
{
    switch (format)
    {
    case VectorDisplayFormat::UnsignedDecimal:
        return QString::number(value);
    case VectorDisplayFormat::SignedDecimal:
        return QString::number(signExtend(value, sewBits));
    case VectorDisplayFormat::Hexadecimal:
        break;
    }
    return QString("0x%1").arg(QString::number(value, 16).toUpper(), static_cast<int>(sewBits / 4),
                               QChar('0'));
}

// Full register, most significant byte first, e.g. 0x000102...0F.
QString formatRaw(const RegisterFile::VectorRegister &bytes)
{
    QString text = "0x";
    for (size_t i = bytes.size(); i-- > 0;)
        text += QString("%1").arg(bytes[i], 2, 16, QChar('0')).toUpper();
    return text;
}
} // namespace

VectorRegisterModel::VectorRegisterModel(QObject *parent, RegisterFile *regfile)
    : QAbstractTableModel(parent), m_registerFile(regfile)
{
    connectRegisterFile();
    m_sewBits = decodeSewBits();
}

int VectorRegisterModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(RegisterFile::NUM_VREG);
}

int VectorRegisterModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : FirstElementColumn + elementCount();
}

int VectorRegisterModel::elementCount() const
{
    return static_cast<int>(RegisterFile::VLEN_BITS / m_sewBits);
}

void VectorRegisterModel::connectRegisterFile()
{
    if (!m_registerFile)
        return;
    connect(m_registerFile, &RegisterFile::updateVRegister, this,
            &VectorRegisterModel::updateVRegisterValue, Qt::UniqueConnection);
    connect(m_registerFile, &RegisterFile::vectorCsrChanged, this,
            &VectorRegisterModel::vectorCsrChangedSlot, Qt::UniqueConnection);
    connect(m_registerFile, &RegisterFile::registerResetSignal, this,
            &VectorRegisterModel::registerResetSlot, Qt::UniqueConnection);
}

unsigned VectorRegisterModel::decodeSewBits() const
{
    if (!m_registerFile)
        return 8;
    const VectorConfig config =
        DecodeVectorConfig(m_registerFile->ReadCsr(RegisterFile::CSR_VTYPE));
    // An illegal vtype (vill) has no SEW; keep the registers readable as bytes.
    return config.vill ? 8 : config.sew_bits;
}

QVariant VectorRegisterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant{};

    if (section == NameColumn)
        return QString("Register");
    if (section == RawColumn)
        return QString("Raw (128-bit)");
    if (section >= FirstElementColumn && section < columnCount())
        return QString("e%1").arg(section - FirstElementColumn);
    return QVariant{};
}

QVariant VectorRegisterModel::data(const QModelIndex &index, int role) const
{
    if (!m_registerFile || !index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(RegisterFile::NUM_VREG) || index.column() < 0 ||
        index.column() >= columnCount())
        return QVariant{};

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;

    if (role == Qt::BackgroundRole)
    {
        if (index.row() == m_highlightedRegister)
            return QBrush(QColor("#b13232fd")); // same highlight as the GPR/FPR table
        return QVariant{};
    }

    if (role == Qt::FontRole)
    {
        // Fixed-width digits keep the 128-bit value and the element columns a stable width.
        if (index.column() >= RawColumn)
            return QFontDatabase::systemFont(QFontDatabase::FixedFont);
        return QVariant{};
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant{};

    if (index.column() == NameColumn)
        return QString("v%1").arg(index.row());

    const RegisterFile::VectorRegister bytes =
        m_registerFile->ReadVreg(static_cast<size_t>(index.row()));

    if (index.column() == RawColumn)
        return formatRaw(bytes);

    const int element = index.column() - FirstElementColumn;
    const uint64_t value = readElement(bytes, m_sewBits, element);

    if (role == Qt::ToolTipRole)
    {
        return QString("v%1[%2] (e%3)\nhex: %4\nunsigned: %5\nsigned: %6")
            .arg(index.row())
            .arg(element)
            .arg(m_sewBits)
            .arg(formatElement(value, m_sewBits, VectorDisplayFormat::Hexadecimal))
            .arg(formatElement(value, m_sewBits, VectorDisplayFormat::UnsignedDecimal))
            .arg(formatElement(value, m_sewBits, VectorDisplayFormat::SignedDecimal));
    }
    return formatElement(value, m_sewBits, m_displayFormat);
}

void VectorRegisterModel::setDisplayFormat(VectorDisplayFormat format)
{
    if (format == m_displayFormat)
        return;
    m_displayFormat = format;
    if (rowCount() > 0 && columnCount() > FirstElementColumn)
        emit dataChanged(index(0, FirstElementColumn),
                         index(rowCount() - 1, columnCount() - 1),
                         {Qt::DisplayRole});
}

void VectorRegisterModel::changeRegisterFile(RegisterFile *regfile)
{
    beginResetModel();
    // The previous RegisterFile may already be destroyed (its connections died with it),
    // so it must not be touched here.
    m_registerFile = regfile;
    connectRegisterFile();
    m_sewBits = decodeSewBits();
    m_highlightedRegister = -1;
    endResetModel();
}

void VectorRegisterModel::emitRowChanged(int row)
{
    if (row < 0 || row >= rowCount() || columnCount() == 0)
        return;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1),
                     {Qt::DisplayRole, Qt::ToolTipRole, Qt::BackgroundRole});
}

void VectorRegisterModel::updateVRegisterValue(size_t regIndex)
{
    if (regIndex >= RegisterFile::NUM_VREG)
        return;
    m_stepHadWrites = true;
    const int previous = m_highlightedRegister;
    m_highlightedRegister = static_cast<int>(regIndex);
    // Only the previously highlighted row and the written row need repainting.
    if (previous != m_highlightedRegister)
        emitRowChanged(previous);
    emitRowChanged(m_highlightedRegister);
}

void VectorRegisterModel::vectorCsrChangedSlot()
{
    const unsigned sew = decodeSewBits();
    if (sew == m_sewBits)
        return;
    // The element columns depend on SEW, so a SEW change is a layout change.
    beginResetModel();
    m_sewBits = sew;
    endResetModel();
}

void VectorRegisterModel::registerResetSlot()
{
    beginResetModel();
    m_highlightedRegister = -1;
    m_stepHadWrites = false;
    m_sewBits = decodeSewBits();
    endResetModel();
}

void VectorRegisterModel::stepFinished()
{
    if (!m_stepHadWrites)
    {
        if (m_highlightedRegister != -1)
        {
            const int previous = m_highlightedRegister;
            m_highlightedRegister = -1;
            emitRowChanged(previous);
        }
    }
    m_stepHadWrites = false;
}

} // namespace Kites
