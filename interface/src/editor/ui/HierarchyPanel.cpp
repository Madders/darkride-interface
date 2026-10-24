//
//  HierarchyPanel.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "HierarchyPanel.h"

#include <QtCore/QItemSelectionModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "../EntityTreeModel.h"

HierarchyPanel::HierarchyPanel(QWidget* parent) : QDockWidget("Hierarchy", parent) {
    setObjectName("EditorHierarchy");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // fixed in place: docking a floating panel back reparents it across native windows, and the GL
    // view (a native window itself) does not survive that; it can be closed and reopened
    setFeatures(QDockWidget::DockWidgetClosable);
    setMinimumWidth(220);

    auto body = new QWidget(this);
    auto layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    _search = new QLineEdit(body);
    _search->setPlaceholderText("Search");
    _search->setClearButtonEnabled(true);

    _tree = new QTreeView(body);
    _tree->setHeaderHidden(true);
    _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _tree->setUniformRowHeights(true);
    _tree->setIndentation(14);
    _tree->setFrameShape(QFrame::NoFrame);
    _tree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    _tree->setExpandsOnDoubleClick(false);
    // rows dragged onto another become its children, onto the empty space below the rows top-level entities
    _tree->setDragEnabled(true);
    _tree->setAcceptDrops(true);
    _tree->setDropIndicatorShown(true);
    _tree->setDragDropMode(QAbstractItemView::InternalMove);

    // the search filters the tree and keeps a match's ancestors; names sort case-blind
    _proxy = new QSortFilterProxyModel(this);
    _proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    _proxy->setFilterRole(Qt::DisplayRole);
    _proxy->setRecursiveFilteringEnabled(true);
    _proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    _proxy->setDynamicSortFilter(true);
    connect(_search, &QLineEdit::textChanged, _proxy, &QSortFilterProxyModel::setFilterFixedString);

    // a row that is not loaded can't be selected for editing; a plain click takes the camera to it, and it is
    // selected when it arrives
    connect(_tree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        auto modifiers = QGuiApplication::keyboardModifiers();
        if (index.data(EntityTreeModel::LoadedRole).toBool()
            || (modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier))) {
            return;
        }
        QUuid id = idAt(index);
        if (!id.isNull()) {
            emit goToRequested(id);
        }
    });
    connect(_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        QUuid id = idAt(index);
        if (!id.isNull()) {
            emit frameRequested(id);
        }
    });
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_tree, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        // a right click on a row outside the selection selects that row first, as Unity does
        QModelIndex index = _tree->indexAt(pos);
        if (index.isValid() && !_tree->selectionModel()->isSelected(index)) {
            _tree->setCurrentIndex(index);
        }
        emit contextMenuRequested(_tree->viewport()->mapToGlobal(pos));
    });

    _banner = new QLabel(body);
    _banner->setObjectName("EditorReadOnlyBanner");
    _banner->setWordWrap(true);
    _banner->hide();

    _counts = new QLabel(body);
    _counts->setObjectName("EditorHierarchyCounts");
    _counts->setWordWrap(true);
    _refresh = new QToolButton(body);
    _refresh->setObjectName("EditorHierarchyRefresh");
    _refresh->setText("Refresh");
    _refresh->setToolTip("Ask the server again for everything in the world");
    connect(_refresh, &QToolButton::clicked, this, &HierarchyPanel::refreshRequested);
    auto countsRow = new QHBoxLayout();
    countsRow->setContentsMargins(0, 0, 0, 0);
    countsRow->addWidget(_counts, 1);
    countsRow->addWidget(_refresh, 0, Qt::AlignTop);

    _countsTimer.setSingleShot(true);
    _countsTimer.setInterval(150);
    connect(&_countsTimer, &QTimer::timeout, this, &HierarchyPanel::updateCounts);

    layout->addWidget(_banner);
    layout->addWidget(_search);
    layout->addWidget(_tree, 1);
    layout->addLayout(countsRow);
    setWidget(body);
}

void HierarchyPanel::setListNote(const QString& note) {
    _listNote = note;
    updateCounts();
}

void HierarchyPanel::updateCounts() {
    if (!_model) {
        return;
    }
    int loaded = _model->loadedCount();
    QString text = _model->hasList() ? QString("%1 entities, %2 loaded").arg(_model->count()).arg(loaded)
                                     : QString("%1 loaded").arg(loaded);
    if (!_listNote.isEmpty()) {
        text += "\n" + _listNote;
    }
    _counts->setText(text);
}

void HierarchyPanel::setReadOnly(bool readOnly, const QString& reason) {
    _banner->setText(reason);
    _banner->setVisible(readOnly);
    _tree->setEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                                    : QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    _tree->setDragEnabled(!readOnly);
    // the style sheet dims the names through this property
    _tree->setProperty("readOnly", readOnly);
    _tree->style()->unpolish(_tree);
    _tree->style()->polish(_tree);
    _tree->viewport()->update();
}

void HierarchyPanel::setModel(EntityTreeModel* model) {
    _model = model;
    _proxy->setSourceModel(model);
    _tree->setModel(_proxy);
    _proxy->setSortRole(EntityTreeModel::CreatedRole);   // siblings in the order they were made, as in Unity
    _proxy->sort(0, Qt::AscendingOrder);
    auto recount = [this] {
        if (!_countsTimer.isActive()) {
            _countsTimer.start();
        }
    };
    connect(model, &QAbstractItemModel::rowsInserted, this, recount);
    connect(model, &QAbstractItemModel::rowsRemoved, this, recount);
    connect(model, &QAbstractItemModel::modelReset, this, recount);
    connect(model, &QAbstractItemModel::dataChanged, this, recount);
    updateCounts();
    connect(_tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (!_settingSelection) {
            _wantedIds = selectedIds();
            emit selectionChangedByUser(_wantedIds);
        }
    });
    // a new entity is selected before its row exists (the model adds rows a moment later):
    // when rows arrive, a wanted one that is not yet selected gets selected
    connect(model, &QAbstractItemModel::rowsInserted, this, [this] {
        if (_wantedIds.isEmpty()) {
            return;
        }
        auto selectionModel = _tree->selectionModel();
        for (const auto& id : _wantedIds) {
            QModelIndex proxyIndex = _proxy->mapFromSource(_model->indexForId(id));
            if (proxyIndex.isValid() && !selectionModel->isSelected(proxyIndex)) {
                selectIds(_wantedIds);
                return;
            }
        }
    });
}

QUuid HierarchyPanel::idAt(const QModelIndex& proxyIndex) const {
    return proxyIndex.data(EntityTreeModel::IdRole).toUuid();
}

QList<QUuid> HierarchyPanel::selectedIds() const {
    QList<QUuid> ids;
    if (!_tree->selectionModel()) {
        return ids;
    }
    for (const auto& index : _tree->selectionModel()->selectedRows()) {
        QUuid id = idAt(index);
        if (!id.isNull()) {
            ids << id;
        }
    }
    return ids;
}

void HierarchyPanel::selectIds(const QList<QUuid>& ids) {
    auto selectionModel = _tree->selectionModel();
    if (!_model || !selectionModel) {
        return;
    }
    _wantedIds = ids;
    _settingSelection = true;
    QItemSelection selection;
    QModelIndex first;
    for (const auto& id : ids) {
        QModelIndex proxyIndex = _proxy->mapFromSource(_model->indexForId(id));
        if (!proxyIndex.isValid()) {
            continue;
        }
        selection.select(proxyIndex, proxyIndex);
        if (!first.isValid()) {
            first = proxyIndex;
        }
    }
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (first.isValid()) {
        selectionModel->setCurrentIndex(first, QItemSelectionModel::NoUpdate);
        _tree->scrollTo(first);
    }
    _settingSelection = false;
}

void HierarchyPanel::expand(const QUuid& id) {
    if (!_model) {
        return;
    }
    QModelIndex proxyIndex = _proxy->mapFromSource(_model->indexForId(id));
    if (proxyIndex.isValid()) {
        _tree->expand(proxyIndex);
    }
}

void HierarchyPanel::reveal(const QUuid& id) {
    if (!_model) {
        return;
    }
    QModelIndex proxyIndex = _proxy->mapFromSource(_model->indexForId(id));
    if (!proxyIndex.isValid()) {
        return;   // not in the tree yet, or hidden by the search
    }
    for (QModelIndex parent = proxyIndex.parent(); parent.isValid(); parent = parent.parent()) {
        _tree->expand(parent);
    }
    _tree->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
}
