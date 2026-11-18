#pragma once
#include "processor/registers.h"
#include <QAbstractTableModel>

namespace Kites
{

/**
 * @brief Read-only model listing the vector CSRs and the fields decoded from vtype.
 *
 * Rows: vl, vstart, vtype (raw), vlenb, SEW, LMUL, VLMAX, VTA, VMA. Everything is read from the
 * RegisterFile on demand (vtype is decoded with DecodeVectorConfig), so the model has no state
 * of its own.
 */
class VectorCsrModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    enum Row
    {
        Vl,
        Vstart,
        Vtype,
        Vlenb,
        Sew,
        Lmul,
        Vlmax,
        Vta,
        Vma,
        RowCount
    };

    explicit VectorCsrModel(QObject *parent = nullptr, RegisterFile *regfile = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void changeRegisterFile(RegisterFile *regfile);

  public slots:
    void refresh();

  private:
    void connectRegisterFile();

    RegisterFile *m_registerFile = nullptr;
};

} // namespace Kites
