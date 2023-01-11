#include "editwidget.hpp"

#include <sstream>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QString>

#include <apps/opencs/model/filter/parser.hpp>
#include <apps/opencs/model/world/universalid.hpp>

#include <components/misc/helpviewer.hpp>

#include "../../model/prefs/shortcut.hpp"
#include "../../model/world/columns.hpp"
#include "../../model/world/data.hpp"
#include "../../model/world/idtablebase.hpp"

CSVFilter::EditWidget::EditWidget(CSMWorld::Data& data, QWidget* parent)
    : QLineEdit(parent)
    , mParser(data)
    , mIsEmpty(true)
{
    mPalette = palette();
    connect(this, &QLineEdit::textChanged, this, &EditWidget::textChanged);

    const CSMWorld::IdTableBase* model
        = static_cast<const CSMWorld::IdTableBase*>(data.getTableModel(CSMWorld::UniversalId::Type_Filters));

    connect(model, &CSMWorld::IdTableBase::dataChanged, this, &EditWidget::filterDataChanged, Qt::QueuedConnection);
    connect(model, &CSMWorld::IdTableBase::rowsRemoved, this, &EditWidget::filterRowsRemoved, Qt::QueuedConnection);
    connect(model, &CSMWorld::IdTableBase::rowsInserted, this, &EditWidget::filterRowsInserted, Qt::QueuedConnection);

    mStateColumnIndex = model->findColumnIndex(CSMWorld::Columns::ColumnId_Modification);
    mDescColumnIndex = model->findColumnIndex(CSMWorld::Columns::ColumnId_Description);

    mHelpAction = new QAction(tr("Help"), this);
    connect(mHelpAction, &QAction::triggered, this, &EditWidget::openHelp);
    mHelpAction->setIcon(QIcon(":/info.png"));
    addAction(mHelpAction);
    auto* openHelpShortcut = new CSMPrefs::Shortcut("help", this);
    openHelpShortcut->associateAction(mHelpAction);

    setText("!string(\"ID\", \".*\")");
}

void CSVFilter::EditWidget::textChanged(const QString& text)
{
    // no need to parse and apply filter if it was empty and now is empty too.
    // e.g. - we modifiing content of filter with already opened some other (big) tables.
    if (text.length() == 0)
    {
        if (mIsEmpty)
            return;
        else
            mIsEmpty = true;
    }
    else
        mIsEmpty = false;

    if (mParser.parse(text.toUtf8().constData()))
    {
        setPalette(mPalette);
        emit filterChanged(mParser.getFilter());
    }
    else
    {
        QPalette palette(mPalette);
        palette.setColor(QPalette::Text, Qt::red);
        setPalette(palette);

        /// \todo improve error reporting; mark only the faulty part
    }
}

void CSVFilter::EditWidget::filterDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    for (int i = topLeft.column(); i <= bottomRight.column(); ++i)
        if (i != mStateColumnIndex && i != mDescColumnIndex)
            textChanged(text());
}

void CSVFilter::EditWidget::filterRowsRemoved(const QModelIndex& parent, int start, int end)
{
    textChanged(text());
}

void CSVFilter::EditWidget::filterRowsInserted(const QModelIndex& parent, int start, int end)
{
    textChanged(text());
}

void CSVFilter::EditWidget::createFilterRequest(
    std::vector<std::pair<std::string, std::vector<std::string>>>& filterSource, Qt::DropAction action)
{
    const unsigned count = filterSource.size();
    bool multipleElements = false;

    switch (count) // setting multipleElements;
    {
        case 0: // empty
            return; // nothing to do here

        case 1: // only single
            multipleElements = false;
            break;

        default:
            multipleElements = true;
            break;
    }

    Qt::KeyboardModifiers key = QApplication::keyboardModifiers();
    QString oldContent(text());

    bool replaceMode = false;
    std::string orAnd;

    switch (key) // setting replaceMode and string used to glue expressions
    {
        case Qt::ShiftModifier:
            orAnd = "!or(";
            replaceMode = false;
            break;

        case Qt::ControlModifier:
            orAnd = "!and(";
            replaceMode = false;
            break;

        default:
            replaceMode = true;
            break;
    }

    if (oldContent.isEmpty()
        || !oldContent.contains(QRegularExpression("^!.*$",
            QRegularExpression::CaseInsensitiveOption))) // if line edit is empty or it does not contain one shot filter
                                                         // go into replace mode
    {
        replaceMode = true;
    }

    if (!replaceMode)
    {
        oldContent.remove('!');
    }

    std::stringstream ss;

    if (multipleElements)
    {
        if (replaceMode)
        {
            ss << "!or(";
        }
        else
        {
            ss << orAnd << oldContent.toUtf8().constData() << ',';
        }

        for (unsigned i = 0; i < count; ++i)
        {
            ss << generateFilter(filterSource[i]);

            if (i + 1 != count)
            {
                ss << ", ";
            }
        }

        ss << ')';
    }
    else
    {
        if (!replaceMode)
        {
            ss << orAnd << oldContent.toUtf8().constData() << ',';
        }
        else
        {
            ss << '!';
        }

        ss << generateFilter(filterSource[0]);

        if (!replaceMode)
        {
            ss << ')';
        }
    }

    if (ss.str().length() > 4)
    {
        clear();
        insert(QString::fromUtf8(ss.str().c_str()));
    }
}

std::string CSVFilter::EditWidget::generateFilter(std::pair<std::string, std::vector<std::string>>& seekedString) const
{
    const unsigned columns = seekedString.second.size();

    bool multipleColumns = false;
    switch (columns)
    {
        case 0: // empty
            return ""; // no column to filter

        case 1: // one column to look for
            multipleColumns = false;
            break;

        default:
            multipleColumns = true;
            break;
    }

    std::stringstream ss;
    if (multipleColumns)
    {
        ss << "or(";
        for (unsigned i = 0; i < columns; ++i)
        {
            ss << "string(" << '"' << seekedString.second[i] << '"' << ',' << '"' << seekedString.first << '"' << ')';
            if (i + 1 != columns)
                ss << ',';
        }
        ss << ')';
    }
    else
    {
        ss << "string" << '(' << '"' << seekedString.second[0] << "\"," << '"' << seekedString.first << "\")";
    }

    return ss.str();
}

void CSVFilter::EditWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addAction(mHelpAction);
    menu->exec(event->globalPos());
    delete menu;
}

void CSVFilter::EditWidget::openHelp()
{
    Misc::HelpViewer::openHelp("manuals/openmw-cs/record-filters.html");
}
