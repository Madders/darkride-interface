//
//  HierarchyPanel.h
//  interface/src/editor/ui
//
//  The Hierarchy dock: a search field over the tree of the domain's entities, and under it how many
//  there are and how many have loaded, with Refresh for the entity server's list of them.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_HierarchyPanel_h
#define hifi_HierarchyPanel_h

#include <QtCore/QList>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtWidgets/QDockWidget>

class QItemSelection;
class QLabel;
class QLineEdit;
class QModelIndex;
class QSortFilterProxyModel;
class QToolButton;
class QTreeView;
class EntityTreeModel;

class HierarchyPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(QWidget* parent = nullptr);

    void setModel(EntityTreeModel* model);
    // shows these as selected without reporting it back as the user's doing
    void selectIds(const QList<QUuid>& ids);
    // opens the rows above this one and scrolls it to the middle of the panel
    void reveal(const QUuid& id);
    // opens the entity's row, so that what has just been put under it shows
    void expand(const QUuid& id);

    QTreeView* treeView() const { return _tree; }
    QLineEdit* searchField() const { return _search; }
    // the world cannot be edited: a note at the top, dimmed names, no renaming; selecting and
    // framing still work
    void setReadOnly(bool readOnly, const QString& reason);
    // a line under the counts: the list is being asked for, or the server did not send it
    void setListNote(const QString& note);

signals:
    void selectionChangedByUser(const QList<QUuid>& ids);
    void frameRequested(const QUuid& id);
    // a plain click on a row that is not loaded: go there
    void goToRequested(const QUuid& id);
    void refreshRequested();
    // a right click in the tree, after the row under it joined the selection
    void contextMenuRequested(const QPoint& globalPos);

private:
    QList<QUuid> selectedIds() const;
    QUuid idAt(const QModelIndex& proxyIndex) const;
    void updateCounts();

    QLabel* _banner { nullptr };
    QLineEdit* _search { nullptr };
    QTreeView* _tree { nullptr };
    QLabel* _counts { nullptr };
    QToolButton* _refresh { nullptr };
    QString _listNote;
    QTimer _countsTimer;    // the counts are redone once after a burst of rows
    QSortFilterProxyModel* _proxy { nullptr };
    EntityTreeModel* _model { nullptr };
    bool _settingSelection { false };
    QList<QUuid> _wantedIds;   // the last selection asked for; rows that arrive later are selected on arrival
};

#endif // hifi_HierarchyPanel_h
