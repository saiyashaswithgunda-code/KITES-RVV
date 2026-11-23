#ifndef REGISTERCONTAINER_H
#define REGISTERCONTAINER_H

#include "registermodel.h"
#include "vectorcsrmodel.h"
#include "vectorregistermodel.h"
#include <QWidget>
#include <memory>

class QSortFilterProxyModel;
class QVBoxLayout;

namespace Kites
{
namespace Ui
{
class RegisterContainer;
}
class CollapsibleSection;

class RegisterContainer : public QWidget
{
    Q_OBJECT

  public:
    explicit RegisterContainer(QWidget *parent = nullptr, RegisterFile *regfile = nullptr);
    ~RegisterContainer();
    void setRegisterFile(RegisterFile *regfile);

  public slots:
    void onStepFinished();

  private:
    void setupRegisterTable();
    void setupVectorSections(QVBoxLayout *sectionsLayout);
    QSortFilterProxyModel *makeRangeProxy(int firstRow, int lastRowExclusive);

    Ui::RegisterContainer *ui;
    RegisterModel *m_registerModel;
    VectorRegisterModel *m_vectorRegisterModel;
    VectorCsrModel *m_vectorCsrModel;
};
} // namespace Kites
#endif // REGISTERCONTAINER_H
