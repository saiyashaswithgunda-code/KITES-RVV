#include "vectorcsrmodel.h"
#include "processor/vector_config.h"

namespace Kites
{

VectorCsrModel::VectorCsrModel(QObject *parent, RegisterFile *regfile)
    : QAbstractTableModel(parent), m_registerFile(regfile)
{
    connectRegisterFile();
}

int VectorCsrModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : RowCount;
}

int VectorCsrModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2; // name, value
}

void VectorCsrModel::connectRegisterFile()
{
    if (!m_registerFile)
        return;
    connect(m_registerFile, &RegisterFile::vectorCsrChanged, this, &VectorCsrModel::refresh, Qt::UniqueConnection);
    connect(m_registerFile, &RegisterFile::registerResetSignal, this, &VectorCsrModel::refresh, Qt::UniqueConnection);
}

QVariant VectorCsrModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant{};
    if (section == 0)
        return QString("CSR");
    if (section == 1)
        return QString("Value");
    return QVariant{};
}

QVariant VectorCsrModel::data(const QModelIndex &index, int role) const
{
    if (!m_registerFile || !index.isValid() || index.row() < 0 || index.row() >= RowCount ||
        index.column() < 0 || index.column() > 1)
        return QVariant{};

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant{};

    static const char *const names[RowCount] = {"vl",   "vstart", "vtype", "vlenb", "SEW",
                                                 "LMUL", "VLMAX",  "VTA",   "VMA"};
    if (index.column() == 0)
        return QString(names[index.row()]);

    const uint64_t vtypeRaw = m_registerFile->ReadCsr(RegisterFile::CSR_VTYPE);
    const VectorConfig config = DecodeVectorConfig(vtypeRaw);
    const QString illegal("-"); // vtype does not decode to a supported configuration (vill)

    switch (index.row())
    {
    case Vl:
        return QString::number(m_registerFile->ReadCsr(RegisterFile::CSR_VL));
    case Vstart:
        return QString::number(m_registerFile->ReadCsr(RegisterFile::CSR_VSTART));
    case Vtype:
        return QString("0x%1").arg(QString::number(vtypeRaw, 16).toUpper());
    case Vlenb:
        return QString::number(m_registerFile->ReadCsr(RegisterFile::CSR_VLENB));
    case Sew:
        return config.vill ? illegal : QString("e%1").arg(config.sew_bits);
    case Lmul:
        return config.vill ? illegal : QString("m%1").arg(config.lmul);
    case Vlmax:
        return config.vill ? illegal : QString::number(config.vlmax);
    case Vta:
        return config.vill ? illegal : QString(config.tail_agnostic ? "agnostic" : "undisturbed");
    case Vma:
        return config.vill ? illegal : QString(config.mask_agnostic ? "agnostic" : "undisturbed");
    }
    return QVariant{};
}

void VectorCsrModel::changeRegisterFile(RegisterFile *regfile)
{
    beginResetModel();
    // The previous RegisterFile may already be destroyed (its connections died with it),
    // so it must not be touched here.
    m_registerFile = regfile;
    connectRegisterFile();
    endResetModel();
}

void VectorCsrModel::refresh()
{
    // Nine short rows: one dataChanged over the value column is cheaper than tracking which
    // of them (vtype also changes the decoded fields) actually moved.
    emit dataChanged(index(0, 1), index(RowCount - 1, 1), {Qt::DisplayRole, Qt::ToolTipRole});
}

} // namespace Kites
