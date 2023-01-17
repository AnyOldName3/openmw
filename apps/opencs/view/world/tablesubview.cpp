#include "tablesubview.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#include <apps/opencs/view/doc/subview.hpp>

#include <components/debug/debuglog.hpp>

#include "../../model/doc/document.hpp"
#include "../../model/world/tablemimedata.hpp"

#include "../doc/sizehint.hpp"
#include "../filter/filterbox.hpp"
#include "table.hpp"
#include "tablebottombox.hpp"

CSVWorld::TableSubView::TableSubView(
    const CSMWorld::UniversalId& id, CSMDoc::Document& document, const CreatorFactoryBase& creatorFactory, bool sorting)
    : SubView(id)
    , mShowOptions(false)
    , mOptions(0)
{
    QVBoxLayout* layout = new QVBoxLayout;

    layout->addWidget(mBottom = new TableBottomBox(creatorFactory, document, id, this), 0);

    layout->insertWidget(0, mTable = new Table(id, mBottom->canCreateAndDelete(), sorting, document), 2);

    mFilterBox = new CSVFilter::FilterBox(document.getData(), this);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->insertWidget(0, mFilterBox);

    mOptions = new QWidget;

    QHBoxLayout* optHLayout = new QHBoxLayout;
    QCheckBox* autoJump = new QCheckBox("Auto Jump");
    autoJump->setToolTip(
        "Whether to jump to the modified record."
        "\nCan be useful in finding the moved or modified"
        "\nobject instance while 3D editing.");
    autoJump->setCheckState(Qt::Unchecked);
    connect(autoJump, &QCheckBox::stateChanged, mTable, &Table::jumpAfterModChanged);
    optHLayout->insertWidget(0, autoJump);
    optHLayout->setContentsMargins(QMargins(0, 3, 0, 0));
    mOptions->setLayout(optHLayout);
    mOptions->resize(mOptions->width(), mFilterBox->height());
    mOptions->hide();

    QPushButton* opt = new QPushButton();
    opt->setIcon(QIcon(":startup/configure"));
    opt->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    opt->setToolTip("Open additional options for this subview.");
    connect(opt, &QPushButton::clicked, this, &TableSubView::toggleOptions);

    QVBoxLayout* buttonLayout = new QVBoxLayout; // work around margin issues
    buttonLayout->setContentsMargins(QMargins(0 /*left*/, 3 /*top*/, 3 /*right*/, 0 /*bottom*/));
    buttonLayout->insertWidget(0, opt, 0, Qt::AlignVCenter | Qt::AlignRight);
    hLayout->insertWidget(1, mOptions);
    hLayout->insertLayout(2, buttonLayout);

    layout->insertLayout(0, hLayout);

    CSVDoc::SizeHintWidget* widget = new CSVDoc::SizeHintWidget;

    widget->setLayout(layout);

    setWidget(widget);
    // prefer height of the screen and full width of the table
    const QRect rect = QApplication::screenAt(pos())->geometry();
    int frameHeight = 40; // set a reasonable default
    QWidget* topLevel = QApplication::topLevelAt(pos());
    if (topLevel)
        frameHeight = topLevel->frameGeometry().height() - topLevel->height();
    widget->setSizeHint(QSize(mTable->horizontalHeader()->length(), rect.height() - frameHeight));

    connect(mTable, &Table::editRequest, this, &TableSubView::editRequest);

    connect(mTable, &Table::selectionSizeChanged, mBottom, &TableBottomBox::selectionSizeChanged);
    connect(mTable, &Table::tableSizeChanged, mBottom, &TableBottomBox::tableSizeChanged);

    mTable->tableSizeUpdate();
    mTable->selectionSizeUpdate();
    mTable->viewport()->installEventFilter(this);
    mBottom->installEventFilter(this);
    mFilterBox->installEventFilter(this);

    if (mBottom->canCreateAndDelete())
    {
        connect(mTable, &Table::createRequest, mBottom, &TableBottomBox::createRequest);

        connect(
            mTable, &Table::cloneRequest, this, qOverload<const CSMWorld::UniversalId&>(&TableSubView::cloneRequest));

        connect(this, qOverload<const std::string&, const CSMWorld::UniversalId::Type>(&TableSubView::cloneRequest),
            mBottom, &TableBottomBox::cloneRequest);

        connect(mTable, &Table::createRecordsDirectlyRequest, mBottom, &TableBottomBox::createRecordsDirectlyRequest);

        connect(mTable, &Table::touchRequest, mBottom, &TableBottomBox::touchRequest);

        connect(mTable, &Table::extendedDeleteConfigRequest, mBottom, &TableBottomBox::extendedDeleteConfigRequest);
        connect(mTable, &Table::extendedRevertConfigRequest, mBottom, &TableBottomBox::extendedRevertConfigRequest);
    }
    connect(mBottom, &TableBottomBox::requestFocus, mTable, &Table::requestFocus);

    connect(mFilterBox, &CSVFilter::FilterBox::recordFilterChanged, mTable, &Table::recordFilterChanged);

    connect(mFilterBox, &CSVFilter::FilterBox::recordDropped, this, &TableSubView::createFilterRequest);

    connect(mTable, &Table::closeRequest, this, qOverload<>(&TableSubView::closeRequest));
}

void CSVWorld::TableSubView::setEditLock(bool locked)
{
    mTable->setEditLock(locked);
    mBottom->setEditLock(locked);
}

void CSVWorld::TableSubView::editRequest(const CSMWorld::UniversalId& id, const std::string& hint)
{
    focusId(id, hint);
}

void CSVWorld::TableSubView::setStatusBar(bool show)
{
    mBottom->setStatusBar(show);
}

void CSVWorld::TableSubView::useHint(const std::string& hint)
{
    if (hint.empty())
        return;

    if (hint[0] == 'f' && hint.size() >= 2)
        mFilterBox->setRecordFilter(hint.substr(2));
}

void CSVWorld::TableSubView::cloneRequest(const CSMWorld::UniversalId& toClone)
{
    emit cloneRequest(toClone.getId(), toClone.getType());
}

void CSVWorld::TableSubView::createFilterRequest(std::vector<CSMWorld::UniversalId>& types, Qt::DropAction action,
    const std::string& searchString, const std::string& searchColumn, bool isValue)
{
    std::vector<std::pair<std::string, std::vector<std::string>>> filterSource;

    std::vector<std::string> refIdColumns = mTable->getColumnsWithDisplay(
        CSMWorld::TableMimeData::convertEnums(CSMWorld::UniversalId::Type_Referenceable));
    bool hasRefIdDisplay = !refIdColumns.empty();

    for (std::vector<CSMWorld::UniversalId>::iterator it(types.begin()); it != types.end(); ++it)
    {
        CSMWorld::UniversalId::Type type = it->getType();
        std::vector<std::string> col = mTable->getColumnsWithDisplay(CSMWorld::TableMimeData::convertEnums(type));
        if (!col.empty())
        {
            filterSource.emplace_back(it->getId(), col);
        }

        if (hasRefIdDisplay && CSMWorld::TableMimeData::isReferencable(type))
        {
            filterSource.emplace_back(it->getId(), refIdColumns);
        }
    }

    if (!filterSource.empty())
        mFilterBox->createFilterRequest(filterSource, action);
    else if (isValue)
    {
        try 
        {
            std::vector<std::pair<int, std::vector<std::string>>> valueFilterSource;
            std::vector<std::string> searchColumns;
            searchColumns.emplace_back(searchColumn);
            int searchValue = std::stoi(searchString);
            Log(Debug::Warning) << "Debug: " << searchValue;
            valueFilterSource.emplace_back(searchValue, searchColumns);
            mFilterBox->createFilterRequest(valueFilterSource, action);
        }
        catch (...)
        {
            Log(Debug::Warning) << "Error in converting the filter request value to integer.";
        }
    }
    else if (searchString != "")
    {
        std::vector<std::string> searchColumns;
        searchColumns.emplace_back(searchColumn);
        filterSource.emplace_back(searchString, searchColumns);
        mFilterBox->createFilterRequest(filterSource, action);
    }
   
}

bool CSVWorld::TableSubView::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Drop)
    {
        if (QDropEvent* drop = dynamic_cast<QDropEvent*>(event))
        {
            const CSMWorld::TableMimeData* tableMimeData
                = dynamic_cast<const CSMWorld::TableMimeData*>(drop->mimeData());
            if (!tableMimeData) // May happen when non-records (e.g. plain text) are dragged and dropped
                return false;

            bool handled = tableMimeData->holdsType(CSMWorld::UniversalId::Type_Filter);
            if (handled)
            {
                mFilterBox->setRecordFilter(tableMimeData->returnMatching(CSMWorld::UniversalId::Type_Filter).getId());
            }
            return handled;
        }
    }
    return false;
}

void CSVWorld::TableSubView::toggleOptions()
{
    if (mShowOptions)
    {
        mShowOptions = false;
        mOptions->hide();
    }
    else
    {
        mShowOptions = true;
        mOptions->show();
    }
}

void CSVWorld::TableSubView::requestFocus(const std::string& id)
{
    mTable->requestFocus(id);
}
