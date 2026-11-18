#include "memorymodel.h"
#include "config/config.h"
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <algorithm>
#include <cstdint>
namespace Kites
{
namespace
{
// Highest 8-byte-aligned row that still fits entirely inside memory.
uint64_t maxCentralAddress()
{
    const uint64_t memorySize = vm_config::config.getMemorySize();
    if (memorySize < 8)
    {
        return 0;
    }
    return ((memorySize - 8) / 8) * 8;
}

uint64_t clampCentralAddress(int64_t address)
{
    if (address < 0)
    {
        return 0;
    }
    return std::min(static_cast<uint64_t>(address), maxCentralAddress());
}
} // namespace

MemoryModel::MemoryModel(QObject *parent, MemoryController *memoryController)
    : QAbstractTableModel(parent)
{
    m_memoryController = memoryController;
    connect(m_memoryController, &MemoryController::memoryUpdated, this, &MemoryModel::updateMemory);
    connect(m_memoryController, &MemoryController::memoryResetSignal, this,
            &MemoryModel::memoryResetSlot);
}

int MemoryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent); // to remove the unused parameter warning
    return m_rowsVisible;
}

int MemoryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 10; // address + 8 bytes + full double word
}

void MemoryModel::setRowsVisible(int rows)
{
    beginResetModel();
    m_rowsVisible = rows;
    endResetModel();
}

void MemoryModel::setDisplayBase(Base base)
{
    beginResetModel();
    m_displayBase = base;
    endResetModel();
}
void MemoryModel::changeMemoryController(MemoryController *memoryController)
{
    beginResetModel();
    m_modifiedBytes.clear();
    m_stepHadWrites = false;
    m_memoryController = memoryController;
    // the previous connection will be invalid now
    // and since previous vm as destroyed the connection auto disconnects
    connect(m_memoryController, &MemoryController::memoryUpdated, this, &MemoryModel::updateMemory);
    connect(m_memoryController, &MemoryController::memoryResetSignal, this,
            &MemoryModel::memoryResetSlot);
    endResetModel();
}
// bool MemoryModel::isValidAddress(const uint64_t& address, int offset) const
// {
//     if(offset < 0)
//     {
//         return (vm_config::config.getMemorySize() + offset*8 < )
//     }
// }

bool MemoryModel::canOffset(int offset)
{
    return ((offset < 0 && m_currentCentralAddress != 0) ||
            (offset > 0 && m_currentCentralAddress != maxCentralAddress()));
}

void MemoryModel::offsetCentralAddress(int offset)
{
    if (!canOffset(offset))
        return;
    beginResetModel();
    // Signed arithmetic, then clamp: scrolling past either end would otherwise wrap the
    // unsigned address around to a wildly out-of-range row.
    m_currentCentralAddress = clampCentralAddress(static_cast<int64_t>(m_currentCentralAddress) +
                                                  static_cast<int64_t>(offset) * 8);
    endResetModel();
}

void MemoryModel::setCentralAddress(const uint64_t &address)
{
    beginResetModel();
    // multiples of 8 so that a row never straddles a double-word boundary
    m_currentCentralAddress = clampCentralAddress(static_cast<int64_t>((address / 8) * 8));
    endResetModel();
}

QVariant MemoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant{};

    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case 0:
            return QString("Address");
        case 1:
            return QString("Double Word");
        case 2:
            return QString("Byte 0");
        case 3:
            return QString("Byte 1");
        case 4:
            return QString("Byte 2");
        case 5:
            return QString("Byte 3");
        case 6:
            return QString("Byte 4");
        case 7:
            return QString("Byte 5");
        case 8:
            return QString("Byte 6");
        case 9:
            return QString("Byte 7");
        default:
            return QVariant{};
        }
    }
    return QVariant{};
}

QVariant MemoryModel::data(const QModelIndex &index, int role) const
{
    if (!m_memoryController)
    {
        if (role == Qt::DisplayRole)
            return QString("yo");
        else
            return QVariant();
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    if (role == Qt::BackgroundRole)
    {
        // Highlight bytes modified by the latest step; the double-word cell is highlighted
        // when any byte of its row changed.
        uint64_t rowAddress = 0;
        if (m_modifiedBytes.empty() || index.column() < 1 || !rowBaseAddress(index.row(), rowAddress))
            return QVariant();
        if (index.column() == 1)
        {
            for (uint64_t offset = 0; offset < 8; ++offset)
                if (m_modifiedBytes.count(rowAddress + offset))
                    return QBrush(QColor("#b13232fd"));
            return QVariant();
        }
        if (m_modifiedBytes.count(rowAddress + static_cast<uint64_t>(index.column() - 2)))
            return QBrush(QColor("#b13232fd"));
        return QVariant();
    }

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
    {
        int offsetAddress = ((((m_rowsVisible * 8) / 2) / 8) * 8) - (index.row() * 8);
        // protecting against overflows
        if (offsetAddress < 0 && static_cast<uint64_t>(abs(offsetAddress)) > m_currentCentralAddress)
        {
            return QString("-");
        }
        const uint64_t alignedAddress =
            static_cast<uint64_t>(m_currentCentralAddress) + offsetAddress;

        // Every column of a row reads somewhere in the 8 bytes at alignedAddress, so the whole
        // row must fit in memory. A partially out-of-range row would make the reads below throw
        // std::out_of_range, and that would unwind through Qt's view code rather than be caught.
        const uint64_t memorySize = vm_config::config.getMemorySize();
        if (memorySize < 8 || alignedAddress > memorySize - 8)
        {
            return QString("-");
        }

        // size_t row = static_cast<size_t>(index.row());
        //  if(!isValidAddress(alignedAddress))
        //  {
        //      return QString("-");
        //  }
        // TODO
        //  refactor this swithch to dynamically give the byte based on column number
        //  so that we have have an arbitrary byte sized block
        //  for supporting 32 bit arch in future
        QString prefix;
        switch (m_displayBase)
        {
        case Base::Hexadecimal:
            prefix = "0x";
            break;
        case Base::Decimal:
            prefix = "";
            break;
        case Base::Binary:
            prefix = "0b";
            break;
        }
        switch (index.column())
        {
        case 0: // Address
        {
            return QString("0x%1").arg(QString::number(alignedAddress, 16).toUpper());
        }
        case 1: // Double Word
        {
            uint64_t doubleWord = m_memoryController->readDoubleWord_d(alignedAddress);
            return QString("%1%2").arg(prefix).arg(
                QString::number(doubleWord, static_cast<int>(m_displayBase)).toUpper());
        }
        default:
        {
            uint8_t byte = m_memoryController->readByte_d(alignedAddress + (index.column() - 2));
            return QString("%1%2").arg(prefix).arg(
                QString::number(byte, static_cast<int>(m_displayBase)).toUpper());
        }
        }
    }

    return QVariant();
}

bool MemoryModel::rowBaseAddress(int row, uint64_t &address) const
{
    // Same mapping as data(): the central address sits at the middle row and rows above it
    // (smaller row numbers) are higher addresses.
    const int offsetAddress = ((((m_rowsVisible * 8) / 2) / 8) * 8) - (row * 8);
    if (offsetAddress < 0 && static_cast<uint64_t>(-offsetAddress) > m_currentCentralAddress)
        return false;
    address = m_currentCentralAddress + offsetAddress;
    const uint64_t memorySize = vm_config::config.getMemorySize();
    return memorySize >= 8 && address <= memorySize - 8;
}

int MemoryModel::visibleRowForAddress(uint64_t address) const
{
    const int64_t topAddr =
        static_cast<int64_t>(m_currentCentralAddress) + static_cast<int64_t>(m_rowsVisible / 2) * 8;
    // Rows are 8-byte aligned, so any byte of a row maps to that row's base address.
    const int64_t bytesFromTop = topAddr - static_cast<int64_t>(address & ~uint64_t{7});
    if (bytesFromTop < 0)
        return -1;
    const int row = static_cast<int>(bytesFromTop / 8);
    return row < m_rowsVisible ? row : -1;
}

void MemoryModel::emitRowChanged(int row)
{
    if (row < 0 || row >= m_rowsVisible)
        return;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1),
                     {Qt::DisplayRole, Qt::ToolTipRole, Qt::BackgroundRole});
}

void MemoryModel::clearModifiedBytes()
{
    if (m_modifiedBytes.empty())
        return;
    const std::unordered_set<uint64_t> old = std::move(m_modifiedBytes);
    m_modifiedBytes.clear();
    for (const uint64_t address : old)
        emitRowChanged(visibleRowForAddress(address));
}

void MemoryModel::memoryResetSlot()
{
    beginResetModel();
    m_modifiedBytes.clear();
    m_stepHadWrites = false;
    endResetModel();
}

void MemoryModel::updateMemory(uint64_t address)
{
    // The first write of a step replaces the highlight left by the previous step.
    if (!m_stepHadWrites)
    {
        clearModifiedBytes();
        m_stepHadWrites = true;
    }
    m_modifiedBytes.insert(address);

    // Only the row containing the written byte needs refreshing (if it is on screen).
    emitRowChanged(visibleRowForAddress(address));
}

void MemoryModel::stepFinished()
{
    if (!m_stepHadWrites)
        clearModifiedBytes();
    m_stepHadWrites = false;
}

void MemoryModel::clearHighlight()
{
    clearModifiedBytes();
    m_stepHadWrites = false;
}
} // namespace Kites
