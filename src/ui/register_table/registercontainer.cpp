#include "registercontainer.h"
#include "ui_registercontainer.h"
#include "processor/registers.h"
#include "ui/common/collapsible_section.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <array>

namespace Kites
{
namespace
{
// Filters a RegisterModel down to a contiguous row range, so General
// Purpose / Floating Point registers can be shown in separate collapsible
// sections while sharing the same underlying model.
class RegisterRangeProxyModel : public QSortFilterProxyModel
{
  public:
    RegisterRangeProxyModel(int firstRow, int lastRowExclusive, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent), m_firstRow(firstRow), m_lastRowExclusive(lastRowExclusive)
    {
    }

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent);
        return sourceRow >= m_firstRow && sourceRow < m_lastRowExclusive;
    }

  private:
    int m_firstRow;
    int m_lastRowExclusive;
};
} // namespace

RegisterContainer::RegisterContainer(QWidget *parent, RegisterFile *regfile)
    : QWidget(parent), ui(new Ui::RegisterContainer),
      m_registerModel(new RegisterModel(this, regfile)),
      m_vectorRegisterModel(new VectorRegisterModel(this, regfile)),
      m_vectorCsrModel(new VectorCsrModel(this, regfile))
{
    ui->setupUi(this);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(ui->sectionsContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->gridLayout->addWidget(scrollArea, 0, 0, 1, 2);

    setupRegisterTable();

    ui->displayType->setCurrentText("Hexadecimal");
    connect(ui->displayType, &QComboBox::currentTextChanged, this,
            [this](const QString &text)
            {
                if (text == "Hexadecimal")
                {
                    m_registerModel->setDisplayBase(Base::Hexadecimal);
                }
                else if (text == "Binary")
                {
                    m_registerModel->setDisplayBase(Base::Binary);
                }
                else if (text == "Decimal/Float")
                {
                    m_registerModel->setDisplayBase(Base::Decimal);
                }
            });
}

QSortFilterProxyModel *RegisterContainer::makeRangeProxy(int firstRow, int lastRowExclusive)
{
    auto *proxy = new RegisterRangeProxyModel(firstRow, lastRowExclusive, this);
    proxy->setSourceModel(m_registerModel);
    return proxy;
}

void RegisterContainer::setupRegisterTable()
{
    auto *sectionsLayout = new QVBoxLayout(ui->sectionsContainer);
    sectionsLayout->setContentsMargins(0, 0, 0, 0);
    sectionsLayout->setSpacing(4);

    struct SectionSpec
    {
        QString title;
        int firstRow;
        int lastRowExclusive;
    };
    const std::array<SectionSpec, 2> sections = {
        SectionSpec{"General Purpose", 0, 32},
        SectionSpec{"Floating Point", 32, 64},
    };

    for (const SectionSpec &spec : sections)
    {
        auto *section = new CollapsibleSection(spec.title, ui->sectionsContainer);

        auto *tableView = new QTableView(section);
        tableView->setModel(makeRangeProxy(spec.firstRow, spec.lastRowExclusive));
        tableView->verticalHeader()->setVisible(false);
        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        auto* header = tableView->horizontalHeader();
        header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(2, QHeaderView::Stretch);

        section->setContentWidget(tableView);
        sectionsLayout->addWidget(section);
    }

    setupVectorSections(sectionsLayout);
}

// Adds the "Vector Registers" and "Vector CSRs" sections below the GPR/FPR ones. They use their
// own models and are collapsed initially so the existing GPR/FPR layout is unchanged on start-up.
void RegisterContainer::setupVectorSections(QVBoxLayout *sectionsLayout)
{
    // --- Vector Registers: format selector + v0-v31 table ---------------------------------
    auto *vregSection = new CollapsibleSection("Vector Registers", ui->sectionsContainer);
    auto *vregBody = new QWidget(vregSection);
    auto *vregLayout = new QVBoxLayout(vregBody);
    vregLayout->setContentsMargins(0, 0, 0, 0);
    vregLayout->setSpacing(2);

    auto *formatRow = new QHBoxLayout();
    formatRow->addWidget(new QLabel("Element Format:", vregBody));
    auto *formatCombo = new QComboBox(vregBody);
    formatCombo->addItems({"Hexadecimal", "Unsigned Decimal", "Signed Decimal"});
    formatRow->addWidget(formatCombo, 1);
    vregLayout->addLayout(formatRow);

    auto *vregView = new QTableView(vregBody);
    vregView->setModel(m_vectorRegisterModel);
    vregView->verticalHeader()->setVisible(false);
    // The raw 128-bit value needs ~34 characters; size every column to its contents and let
    // the view scroll horizontally when the panel is narrower than the table.
    vregView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    vregView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    vregView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    vregLayout->addWidget(vregView);

    connect(formatCombo, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                switch (index)
                {
                case 1:
                    m_vectorRegisterModel->setDisplayFormat(VectorDisplayFormat::UnsignedDecimal);
                    break;
                case 2:
                    m_vectorRegisterModel->setDisplayFormat(VectorDisplayFormat::SignedDecimal);
                    break;
                default:
                    m_vectorRegisterModel->setDisplayFormat(VectorDisplayFormat::Hexadecimal);
                    break;
                }
            });

    vregSection->setContentWidget(vregBody);
    vregSection->setExpanded(false, /*animate=*/false);
    sectionsLayout->addWidget(vregSection);

    // --- Vector CSRs: read-only list -------------------------------------------------------
    auto *csrSection = new CollapsibleSection("Vector CSRs", ui->sectionsContainer);
    auto *csrView = new QTableView(csrSection);
    csrView->setModel(m_vectorCsrModel);
    csrView->verticalHeader()->setVisible(false);
    csrView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    csrView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    csrView->setMinimumHeight(csrView->horizontalHeader()->sizeHint().height() +
                              VectorCsrModel::RowCount * csrView->verticalHeader()->defaultSectionSize() +
                              2 * csrView->frameWidth());
    csrSection->setContentWidget(csrView);
    csrSection->setExpanded(false, /*animate=*/false);
    sectionsLayout->addWidget(csrSection);
}

void RegisterContainer::setRegisterFile(RegisterFile *regfile)
{
    m_registerModel->changeRegisterFile(regfile);
    m_vectorRegisterModel->changeRegisterFile(regfile);
    m_vectorCsrModel->changeRegisterFile(regfile);
}

void RegisterContainer::onStepFinished()
{
    m_registerModel->stepFinished();
    m_vectorRegisterModel->stepFinished();
}

RegisterContainer::~RegisterContainer()
{
    delete ui;
}

} // namespace Kites
