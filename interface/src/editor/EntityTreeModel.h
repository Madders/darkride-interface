//
//  EntityTreeModel.h
//  interface/src/editor
//
//  The Hierarchy's model: the domain's entities as a parent/child tree, kept live from the
//  entity tree's signals, with a slow rescan behind them for what the signals miss. Entities the
//  entity server listed but has not streamed to this client are rows too, not loaded (grey), until
//  they arrive (NATIVE-EDITOR.md 8).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityTreeModel_h
#define hifi_EntityTreeModel_h

#include <memory>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtGui/QColor>

#include <AACube.h>
#include <EntityItemID.h>
#include <EntityListMessage.h>
#include <EntityTree.h>
#include <EntityTypes.h>

class EntityTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        TypeNameRole,
        LockedRole,
        VisibleRole,
        CreatedRole,    // when the entity was made (usecs): the Hierarchy lists siblings in that order, as Unity does
        LoadedRole      // false for an entity the server listed that this client does not have
    };

    explicit EntityTreeModel(QObject* parent = nullptr);
    ~EntityTreeModel();

    // follow the client's entity tree (call once it exists)
    void attach();

    QModelIndex indexForId(const QUuid& id) const;
    QUuid idForIndex(const QModelIndex& index) const;
    QList<QUuid> allIds() const;

    // the entity server's list: what this client lacks becomes rows that are not loaded; rows that were not
    // loaded and are no longer listed go
    void setListed(const QVector<EntityListMessage::Record>& records);
    bool hasList() const { return _hasList; }
    // a row the client has the entity for (false for one it does not know at all)
    bool isLoaded(const QUuid& id) const;
    // a row that is not loaded: where the server keeps it, and its name as the row shows it
    bool listedPlace(const QUuid& id, AACube& cube, QString& name) const;
    int count() const { return _nodes.size(); }
    int loadedCount() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;
    // rows dragged onto another become its children, onto the empty space top-level entities; the editor makes
    // the move (reparentRequested), and the tree follows the entities
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                         const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    // the entity is the other, or below it
    bool isDescendantOf(const QUuid& id, const QUuid& ancestor) const;
    // what the entity could not load (its model, image or textures, with the reasons; EntityRenderer::loadFailures),
    // empty when nothing: its row's name is drawn red and the reasons are in its tooltip. The editor sets it
    void setLoadProblem(const QUuid& id, const QString& problem);
    QString loadProblem(const QUuid& id) const;
    // the colours of those rows (red: couldn't load) and of the rows listed but not loaded yet (grey); the editor sets
    // them from the skin (Editor::applyStyle), Darkride's own without one
    void setRowColors(const QColor& failed, const QColor& unloaded);
    // the colour the entity's row is drawn in, invalid when it is the panel's own (for scripted tests)
    QColor rowColor(const QUuid& id) const;

signals:
    // the user typed a new name in the Hierarchy; the editor applies it (through undo)
    void renameRequested(const QUuid& id, const QString& name);
    // the user dragged entities onto another (or onto the empty space: a null parent); the editor moves them
    void reparentRequested(const QList<QUuid>& ids, const QUuid& parentId);
    // a row that was not loaded has its entity now
    void entityLoaded(const QUuid& id);
    // the client's tree was emptied (a domain change, a reload): the server's list went with it
    void listCleared();

private slots:
    void onEntityAdded(const EntityItemID& id);
    void onEntityDeleted(const EntityItemID& id);
    void flushEdits();
    void rescan();
    void rebuild();
    void clearTree();

private:
    struct Node {
        QUuid id;
        QString name;
        EntityTypes::EntityType type { EntityTypes::Unknown };
        bool locked { false };
        bool visible { true };
        quint64 created { 0 };
        QString loadProblem;    // setLoadProblem
        QUuid parentID;
        bool loaded { true };
        AACube cube;        // the server's query cube, for a row that is not loaded
        Node* parent { nullptr };
        QVector<Node*> children;

        ~Node() { qDeleteAll(children); }
        int row() const { return parent ? parent->children.indexOf(const_cast<Node*>(this)) : 0; }
    };

    Node* nodeFor(const QModelIndex& index) const;
    QModelIndex indexFor(Node* node) const;
    Node* parentNodeFor(const QUuid& parentID) const;
    bool isDescendant(Node* node, Node* ancestor) const;

    // reads name, type, locked, visible and parent under the tree's read lock; false if gone
    bool readEntity(const QUuid& id, Node& node) const;
    static void fillNode(Node& node, const EntityItemPointer& entity);
    static void fillNode(Node& node, const EntityListMessage::Record& record);
    // takes the entity's fresh fields; a row that was not loaded is now
    void refreshNode(Node* node, const Node& fresh);
    void insertNode(Node* node, Node* parent);
    void removeNode(Node* node);
    void moveNode(Node* node, Node* newParent);
    void adoptOrphans(Node* parent);
    void updateNode(Node* node);

    EntityTreePointer _tree;
    std::unique_ptr<Node> _root;
    QHash<QUuid, Node*> _nodes;
    QColor _failedColor { 255, 107, 107 };    // setRowColors
    QColor _unloadedColor { 0x7a, 0x7a, 0x7a };
    QHash<QUuid, EntityListMessage::Record> _listed;
    bool _hasList { false };
    QSet<QUuid> _pendingEdits;
    QTimer _editFlush;
    QTimer _rescan;
};

#endif // hifi_EntityTreeModel_h
