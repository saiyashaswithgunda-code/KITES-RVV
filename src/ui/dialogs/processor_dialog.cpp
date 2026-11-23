#include "processor_dialog.h"
#include "ui_processor_dialog.h"

namespace Kites
{
ProcessorDialog::ProcessorDialog(QWidget *parent, const ProcessorType &currentVMType)
    : QDialog(parent), ui(new Ui::ProcessorDialog)
{
    ui->setupUi(this);
    ui->treeWidget->setHeaderHidden(true);
    ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(static_cast<int>(currentVMType)));
}

ProcessorDialog::~ProcessorDialog()
{
    delete ui;
}

void ProcessorDialog::on_buttonBox_accepted()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->currentItem();
    if (selectedItem)
    {
        m_currentSelectedItem = selectedItem;
        int index = ui->treeWidget->indexOfTopLevelItem(selectedItem);
        if (index >= 0 && index < static_cast<int>(ProcessorType::ProcessorTypeCount))
        {
            emit vmSelected(static_cast<ProcessorType>(index));
            return;
        }

        QString processorType = selectedItem->text(0);
        if (processorType.startsWith("Single cycle", Qt::CaseInsensitive))
        {
            emit vmSelected(ProcessorType::RVSS);
        }
        else if (processorType.contains("w/o forwarding", Qt::CaseInsensitive))
        {
            if (processorType.contains("w/o hazard", Qt::CaseInsensitive))
                emit vmSelected(ProcessorType::RV5Stage_NH_NF);
            else
                emit vmSelected(ProcessorType::RV5Stage_H_NF);
        }
        else if (processorType.contains("w/ forwarding", Qt::CaseInsensitive))
        {
            if (processorType.contains("w/o hazard", Qt::CaseInsensitive))
                emit vmSelected(ProcessorType::RV5Stage_NH_F);
            else
                emit vmSelected(ProcessorType::RV5Stage_H_F);
        }
    }
}

} // namespace Kites
