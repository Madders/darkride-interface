//
//  EntityTreeModel.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityTreeModel.h"

#include <QtCore/QMimeData>
#include <QtGui/QBrush>
#include <QtGui/QColor>

#include <EntityItem.h>
#include <EntityTreeElement.h>
#include <EntityTreeRenderer.h>

#include "Application.h"

namespace {
    const int EDIT_FLUSH_MS = 100;
    const int RESCAN_MS = 3000;
    const QString ID_MIME_TYPE = QStringLiteral("application/x-darkride-entity-ids");   // a drag's ids, one a line

    QList<QUuid> draggedIds(const QMimeData* data) {
        QList<QUuid> ids;
        for (const auto& text : QString::fromUtf8(data->data(ID_MIME_TYPE)).split('\n', Qt::SkipEmptyParts)) {
            QUuid id(text);
            if (!id.isNull()) {
                ids << id;
            }
        }
        return ids;
    }
}

EntityTreeModel::EntityTreeModel(QObject* parent) : QAbstractItemModel(parent), _root(new Node()) {
    _editFlush.setSingleShot(true);
    _editFlush.setInterval(EDIT_FLUSH_MS);
    connect(&_editFlush, &QTimer::timeout, this, &EntityTreeModel::flushEdits);
    _rescan.setInterval(RESCAN_MS);
    connect(&_rescan, &QTimer::timeout, this, &EntityTreeModel::rescan);
}

EntityTreeModel::~EntityTreeModel() {
}

void EntityTreeModel::attach() {
    auto renderer = qApp->getEntities();
    if (!renderer) {
        return;
    }
    auto tree = renderer->getTree();
    if (!tree || tree == _tree) {
        return;
    }
    if (_tree) {
        disconnect(_tree.get(), nullptr, this, nullptr);
    }
    _tree = tree;

    // the tree's signals come from other threads; everything here runs on the model's thread
    connect(_tree.get(), &EntityTree::addingEntity, this, &EntityTreeModel::onEntityAdded, Qt::QueuedConnection);
    connect(_tree.get(), &EntityTree::deletingEntity, this, &EntityTreeModel::onEntityDeleted, Qt::QueuedConnection);
    connect(_tree.get(), &EntityTree::clearingEntities, this, &EntityTreeModel::clearTree, Qt::QueuedConnection);
    connect(_tree.get(), &EntityTree::editingEntityPointer, this, [this](const EntityItemPointer& entity) {
        if (!entity) {
            return;
        }
        QUuid id = entity->getID();
        QMetaObject::invokeMethod(this, [this, id] {
            _pendingEdits.insert(id);
            if (!_editFlush.isActive()) {
                _editFlush.start();
            }
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

    rebuild();
    _rescan.start();
}

// --- reading the tree ---------------------------------------------------------------------------

bool EntityTreeModel::readEntity(const QUuid& id, Node& node) const {
    if (!_tree) {
        return false;
    }
    bool found = false;
    _tree->withReadLock([&] {
        auto entity = _tree->findEntityByEntityItemID(id);
        // the Hierarchy is the world: not the scripts' local helpers, not avatar wearables
        if (!entity || !entity->isDomainEntity()) {
            return;
        }
        found = true;
        fillNode(node, entity);
    });
    return found;
}

void EntityTreeModel::fillNode(Node& node, const EntityItemPointer& entity) {
    node.id = entity->getID();
    node.name = entity->getName();
    node.type = entity->getType();
    node.locked = entity->getLocked();
    node.visible = entity->getVisible();
    bool hasScripts = !entity->getScript().isEmpty() || !entity->getServerScripts().isEmpty();
    node.scripts = !hasScripts ? NoScripts : entity->getScriptsEnabled() ? ScriptsRun : ScriptsOff;
    node.created = entity->getCreated();
    node.parentID = entity->getParentID();
    node.loaded = true;
}

void EntityTreeModel::fillNode(Node& node, const EntityListMessage::Record& record) {
    node.id = record.id;
    node.name = record.name;
    node.type = record.type;
    node.locked = record.locked;
    node.visible = record.visible;
    node.created = record.created;
    node.parentID = record.parentID;
    node.loaded = false;
    node.cube = record.cube;
}

void EntityTreeModel::rebuild() {
    beginResetModel();
    _nodes.clear();
    _pendingEdits.clear();
    _root.reset(new Node());

    QVector<Node*> nodes;
    if (_tree) {
        _tree->withReadLock([&] {
            _tree->recurseTreeWithOperation([&](const OctreeElementPointer& element, void*) {
                auto entityElement = std::static_pointer_cast<EntityTreeElement>(element);
                entityElement->forEachEntity([&](const EntityItemPointer& entity) {
                    if (!entity->isDomainEntity()) {
                        return;
                    }
                    auto node = new Node();
                    fillNode(*node, entity);
                    nodes << node;
                });
                return true;
            });
        });
    }
    for (auto node : nodes) {
        _nodes[node->id] = node;
    }
    // and what the server listed that the tree does not hold
    for (const auto& record : _listed) {
        if (!_nodes.contains(record.id)) {
            auto node = new Node();
            fillNode(*node, record);
            _nodes[node->id] = node;
            nodes << node;
        }
    }
    // parents first, then everything else hangs off its parent or the root
    for (auto node : nodes) {
        Node* parent = parentNodeFor(node->parentID);
        if (parent != _root.get() && isDescendant(parent, node)) {
            parent = _root.get();   // a cycle in the data; keep the tree a tree
        }
        node->parent = parent;
        parent->children.append(node);
    }
    endResetModel();
}

void EntityTreeModel::clearTree() {
    // the list was this domain's; the editor asks the new one
    _listed.clear();
    _hasList = false;
    rebuild();
    emit listCleared();
}

void EntityTreeModel::rescan() {
    if (!_tree) {
        return;
    }
    // everything the tree holds right now, in one pass under the lock
    QHash<QUuid, Node> current;
    _tree->withReadLock([&] {
        _tree->recurseTreeWithOperation([&](const OctreeElementPointer& element, void*) {
            auto entityElement = std::static_pointer_cast<EntityTreeElement>(element);
            entityElement->forEachEntity([&](const EntityItemPointer& entity) {
                if (!entity->isDomainEntity()) {
                    return;
                }
                Node node;
                fillNode(node, entity);
                current.insert(node.id, node);
            });
            return true;
        });
    });

    // gone (a row that is not loaded was never in the tree)
    QList<QUuid> stale;
    for (auto it = _nodes.constBegin(); it != _nodes.constEnd(); ++it) {
        if (it.value()->loaded && !current.contains(it.key())) {
            stale << it.key();
        }
    }
    for (const auto& id : stale) {
        _listed.remove(id);
        if (auto node = _nodes.value(id)) {
            removeNode(node);
        }
    }
    // new, then changed
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        if (!_nodes.contains(it.key())) {
            auto node = new Node(it.value());
            node->children.clear();
            insertNode(node, parentNodeFor(node->parentID));
        }
    }
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        if (auto node = _nodes.value(it.key())) {
            refreshNode(node, it.value());
        }
    }
}

void EntityTreeModel::refreshNode(Node* node, const Node& fresh) {
    bool arrived = !node->loaded;
    bool changed = arrived || node->name != fresh.name || node->locked != fresh.locked
        || node->visible != fresh.visible || node->scripts != fresh.scripts || node->type != fresh.type
        || node->created != fresh.created;
    if (changed) {
        node->name = fresh.name;
        node->locked = fresh.locked;
        node->visible = fresh.visible;
        node->scripts = fresh.scripts;
        node->type = fresh.type;
        node->created = fresh.created;
        node->loaded = true;
        QModelIndex index = indexFor(node);
        emit dataChanged(index, index);
    }
    if (node->parentID != fresh.parentID) {
        node->parentID = fresh.parentID;
        moveNode(node, parentNodeFor(fresh.parentID));
    }
    if (arrived) {
        emit entityLoaded(node->id);
    }
}

void EntityTreeModel::setListed(const QVector<EntityListMessage::Record>& records) {
    _listed.clear();
    _listed.reserve(records.size());
    for (const auto& record : records) {
        if (!record.id.isNull()) {
            _listed.insert(record.id, record);
        }
    }
    _hasList = true;

    // rows that were not loaded and are no longer listed: deleted on the server since the last list
    QList<Node*> gone;
    for (auto node : _nodes) {
        if (!node->loaded && !_listed.contains(node->id)) {
            gone << node;
        }
    }
    for (auto node : gone) {
        removeNode(node);
    }
    // rows that are still not loaded take the list's fields
    for (const auto& record : _listed) {
        auto node = _nodes.value(record.id);
        if (!node || node->loaded) {
            continue;
        }
        node->cube = record.cube;
        if (node->name != record.name || node->locked != record.locked || node->visible != record.visible
            || node->type != record.type) {
            node->name = record.name;
            node->locked = record.locked;
            node->visible = record.visible;
            node->type = record.type;
            QModelIndex index = indexFor(node);
            emit dataChanged(index, index);
        }
        if (node->parentID != record.parentID) {
            node->parentID = record.parentID;
            moveNode(node, parentNodeFor(record.parentID));
        }
    }

    // the new ones. They are put together first, children under their new parents out of the model's sight, then
    // go in with one insertion per parent that is already in the model, rather than a row at a time
    QHash<QUuid, Node*> fresh;
    for (const auto& record : _listed) {
        if (!_nodes.contains(record.id)) {
            auto node = new Node();
            fillNode(*node, record);
            fresh.insert(node->id, node);
        }
    }
    if (!fresh.isEmpty()) {
        QHash<Node*, QVector<Node*>> byParent;   // a parent in the model (or the root), and the new rows for it
        for (auto node : fresh) {
            // a chain of new parents that loops (bad data) goes to the root
            int steps = 0;
            QUuid ancestor = node->parentID;
            while (fresh.contains(ancestor) && ancestor != node->id && steps <= fresh.size()) {
                ancestor = fresh.value(ancestor)->parentID;
                ++steps;
            }
            bool loops = ancestor == node->id || steps > fresh.size();
            Node* newParent = loops ? nullptr : fresh.value(node->parentID);
            if (newParent) {
                node->parent = newParent;
                newParent->children.append(node);
            } else {
                byParent[loops ? _root.get() : parentNodeFor(node->parentID)].append(node);
            }
        }
        for (auto it = byParent.begin(); it != byParent.end(); ++it) {
            Node* parent = it.key();
            const auto& children = it.value();
            int first = parent->children.size();
            beginInsertRows(indexFor(parent), first, first + children.size() - 1);
            for (auto child : children) {
                child->parent = parent;
                parent->children.append(child);
            }
            endInsertRows();
        }
        for (auto node : fresh) {
            _nodes[node->id] = node;
        }
        // loaded entities whose parent had not arrived sat at the root; their parent has a row now
        QVector<Node*> orphans;
        for (auto child : _root->children) {
            if (child->loaded && fresh.contains(child->parentID)) {
                orphans << child;
            }
        }
        for (auto child : orphans) {
            moveNode(child, _nodes.value(child->parentID));
        }
    }
}

bool EntityTreeModel::isLoaded(const QUuid& id) const {
    auto node = _nodes.value(id);
    return node && node->loaded;
}

bool EntityTreeModel::listedPlace(const QUuid& id, AACube& cube, QString& name) const {
    auto node = _nodes.value(id);
    if (!node || node->loaded) {
        return false;
    }
    cube = node->cube;
    name = node->name.isEmpty() ? EntityTypes::getEntityTypeName(node->type) : node->name;
    return true;
}

int EntityTreeModel::loadedCount() const {
    int loaded = 0;
    for (auto node : _nodes) {
        if (node->loaded) {
            ++loaded;
        }
    }
    return loaded;
}

int EntityTreeModel::scriptsOffCount() const {
    int off = 0;
    for (auto node : _nodes) {
        if (node->loaded && node->scripts == ScriptsOff) {
            ++off;
        }
    }
    return off;
}

// --- incremental updates ------------------------------------------------------------------------

void EntityTreeModel::onEntityAdded(const EntityItemID& id) {
    if (auto existing = _nodes.value(id)) {
        if (!existing->loaded) {
            // a listed row's entity has streamed in: the row turns live where it is
            Node fresh;
            if (readEntity(id, fresh)) {
                refreshNode(existing, fresh);
            }
            return;
        }
        _pendingEdits.insert(id);
        if (!_editFlush.isActive()) {
            _editFlush.start();
        }
        return;
    }
    auto node = new Node();
    if (!readEntity(id, *node)) {
        delete node;
        return;
    }
    insertNode(node, parentNodeFor(node->parentID));
}

void EntityTreeModel::onEntityDeleted(const EntityItemID& id) {
    _pendingEdits.remove(id);
    _listed.remove(id);
    if (auto node = _nodes.value(id)) {
        removeNode(node);
    }
}

void EntityTreeModel::flushEdits() {
    QSet<QUuid> pending;
    pending.swap(_pendingEdits);
    for (const auto& id : pending) {
        auto node = _nodes.value(id);
        if (!node) {
            onEntityAdded(id);
            continue;
        }
        updateNode(node);
    }
}

void EntityTreeModel::updateNode(Node* node) {
    Node fresh;
    if (!readEntity(node->id, fresh)) {
        if (node->loaded) {
            _listed.remove(node->id);
            removeNode(node);
        }
        return;
    }
    refreshNode(node, fresh);
}

void EntityTreeModel::insertNode(Node* node, Node* parent) {
    if (!parent) {
        parent = _root.get();
    }
    int row = parent->children.size();
    beginInsertRows(indexFor(parent), row, row);
    node->parent = parent;
    parent->children.append(node);
    _nodes[node->id] = node;
    endInsertRows();
    adoptOrphans(node);
}

void EntityTreeModel::removeNode(Node* node) {
    if (!node || node == _root.get()) {
        return;
    }
    // the children may well outlive the parent in the world; keep them, at the root for now
    while (!node->children.isEmpty()) {
        moveNode(node->children.first(), _root.get());
    }
    Node* parent = node->parent;
    int row = node->row();
    beginRemoveRows(indexFor(parent), row, row);
    parent->children.removeAt(row);
    _nodes.remove(node->id);
    endRemoveRows();
    delete node;
}

void EntityTreeModel::moveNode(Node* node, Node* newParent) {
    if (!newParent) {
        newParent = _root.get();
    }
    if (newParent == node || isDescendant(newParent, node)) {
        newParent = _root.get();
    }
    Node* oldParent = node->parent;
    if (oldParent == newParent) {
        return;
    }
    int row = node->row();
    int destination = newParent->children.size();
    if (!beginMoveRows(indexFor(oldParent), row, row, indexFor(newParent), destination)) {
        return;
    }
    oldParent->children.removeAt(row);
    node->parent = newParent;
    newParent->children.append(node);
    endMoveRows();
}

void EntityTreeModel::adoptOrphans(Node* parent) {
    // children that arrived before their parent sit at the root until now
    QVector<Node*> orphans;
    for (auto child : _root->children) {
        if (child != parent && child->parentID == parent->id) {
            orphans << child;
        }
    }
    for (auto child : orphans) {
        moveNode(child, parent);
    }
}

EntityTreeModel::Node* EntityTreeModel::parentNodeFor(const QUuid& parentID) const {
    if (parentID.isNull()) {
        return _root.get();
    }
    // an avatar or an entity we do not have is not in the model: hang off the root
    auto node = _nodes.value(parentID);
    return node ? node : _root.get();
}

bool EntityTreeModel::isDescendant(Node* node, Node* ancestor) const {
    for (Node* current = node; current; current = current->parent) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

// --- QAbstractItemModel -------------------------------------------------------------------------

EntityTreeModel::Node* EntityTreeModel::nodeFor(const QModelIndex& index) const {
    if (!index.isValid()) {
        return _root.get();
    }
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex EntityTreeModel::indexFor(Node* node) const {
    if (!node || node == _root.get()) {
        return QModelIndex();
    }
    return createIndex(node->row(), 0, node);
}

QModelIndex EntityTreeModel::indexForId(const QUuid& id) const {
    return indexFor(_nodes.value(id));
}

QUuid EntityTreeModel::idForIndex(const QModelIndex& index) const {
    Node* node = nodeFor(index);
    return (node && node != _root.get()) ? node->id : QUuid();
}

QList<QUuid> EntityTreeModel::allIds() const {
    return _nodes.keys();
}

QModelIndex EntityTreeModel::index(int row, int column, const QModelIndex& parent) const {
    Node* parentNode = nodeFor(parent);
    if (column != 0 || row < 0 || !parentNode || row >= parentNode->children.size()) {
        return QModelIndex();
    }
    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex EntityTreeModel::parent(const QModelIndex& index) const {
    Node* node = nodeFor(index);
    if (!node || node == _root.get()) {
        return QModelIndex();
    }
    return indexFor(node->parent);
}

int EntityTreeModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) {
        return 0;
    }
    Node* node = nodeFor(parent);
    return node ? node->children.size() : 0;
}

int EntityTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 1;
}

QVariant EntityTreeModel::data(const QModelIndex& index, int role) const {
    Node* node = nodeFor(index);
    if (!node || node == _root.get()) {
        return QVariant();
    }
    const QString& typeName = EntityTypes::getEntityTypeName(node->type);
    switch (role) {
        case Qt::DisplayRole:
            return node->name.isEmpty() ? typeName : node->name;
        case Qt::EditRole:
            return node->name;
        case Qt::ToolTipRole: {
            QString tip = typeName + "  " + node->id.toString(QUuid::WithoutBraces);
            if (!node->loaded) {
                tip += "\nNot loaded yet. Click to go there.";
            }
            if (!node->loadProblem.isEmpty()) {
                tip += "\nCouldn't load:\n" + node->loadProblem;
            }
            return tip;
        }
        case Qt::ForegroundRole:
            // red over grey: red, an entity whose model, image or texture failed to load (Caity, 10-24: an Image that
            // stayed blank and said nothing); grey, one the entity server lists that the client has not loaded; the
            // selected row keeps the style sheet's white
            if (!node->loadProblem.isEmpty()) {
                return QBrush(_failedColor);
            }
            if (!node->loaded) {
                return QBrush(_unloadedColor);
            }
            return QVariant();
        case LoadedRole:
            return node->loaded;
        case IdRole:
            return node->id;
        case TypeRole:
            return (int)node->type;
        case TypeNameRole:
            return typeName;
        case LockedRole:
            return node->locked;
        case VisibleRole:
            return node->visible;
        case ScriptsRole:
            return (int)node->scripts;
        case CreatedRole:
            return (qulonglong)node->created;
        default:
            return QVariant();
    }
}

bool EntityTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    Node* node = nodeFor(index);
    if (!node || node == _root.get() || role != Qt::EditRole) {
        return false;
    }
    QString name = value.toString().trimmed();
    if (name == node->name) {
        return false;
    }
    emit renameRequested(node->id, name);
    return true;
}

Qt::ItemFlags EntityTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;   // the empty space below the rows: a drop there is to the top level
    }
    // a row that is not loaded can be chosen (to go there) but not renamed, dragged or dropped onto: a move
    // needs the entity, and a new parent's transform
    Node* node = nodeFor(index);
    if (node && !node->loaded) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

QStringList EntityTreeModel::mimeTypes() const {
    return { ID_MIME_TYPE };
}

QMimeData* EntityTreeModel::mimeData(const QModelIndexList& indexes) const {
    QStringList ids;
    for (const auto& index : indexes) {
        QUuid id = idForIndex(index);
        if (index.column() == 0 && !id.isNull()) {
            ids << id.toString();
        }
    }
    if (ids.isEmpty()) {
        return nullptr;
    }
    auto data = new QMimeData();
    data->setData(ID_MIME_TYPE, ids.join('\n').toUtf8());
    return data;
}

bool EntityTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                      const QModelIndex& parent) const {
    Q_UNUSED(row);
    Q_UNUSED(column);
    if (action != Qt::MoveAction || !data || !data->hasFormat(ID_MIME_TYPE)) {
        return false;
    }
    // onto an entity, or onto the empty space; not onto one of the dragged or anything below it
    QUuid target = idForIndex(parent);
    if (!target.isNull() && !isLoaded(target)) {
        return false;
    }
    for (const auto& id : draggedIds(data)) {
        if (!isLoaded(id) || (!target.isNull() && isDescendantOf(target, id))) {
            return false;
        }
    }
    return true;
}

bool EntityTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
    if (!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }
    // the editor makes the move (through undo) and the tree follows the entities: nothing is taken out here
    emit reparentRequested(draggedIds(data), idForIndex(parent));
    return true;
}

void EntityTreeModel::setLoadProblem(const QUuid& id, const QString& problem) {
    Node* node = _nodes.value(id);
    if (!node || node->loadProblem == problem) {
        return;
    }
    node->loadProblem = problem;
    QModelIndex index = indexFor(node);
    emit dataChanged(index, index, { Qt::ForegroundRole, Qt::ToolTipRole });
}

void EntityTreeModel::setRowColors(const QColor& failed, const QColor& unloaded) {
    if (failed == _failedColor && unloaded == _unloadedColor) {
        return;
    }
    _failedColor = failed;
    _unloadedColor = unloaded;
    // every row may be drawn in one: Qt has no dataChanged for a whole tree, so the views draw it again
    emit layoutAboutToBeChanged();
    emit layoutChanged();
}

QColor EntityTreeModel::rowColor(const QUuid& id) const {
    QVariant brush = data(indexForId(id), Qt::ForegroundRole);
    return brush.isValid() ? brush.value<QBrush>().color() : QColor();
}

QString EntityTreeModel::loadProblem(const QUuid& id) const {
    Node* node = _nodes.value(id);
    return node ? node->loadProblem : QString();
}

bool EntityTreeModel::isDescendantOf(const QUuid& id, const QUuid& ancestor) const {
    Node* node = _nodes.value(id);
    Node* ancestorNode = _nodes.value(ancestor);
    return node && ancestorNode && isDescendant(node, ancestorNode);
}

QHash<int, QByteArray> EntityTreeModel::roleNames() const {
    auto roles = QAbstractItemModel::roleNames();
    roles[IdRole] = "id";
    roles[TypeRole] = "type";
    roles[TypeNameRole] = "typeName";
    roles[LockedRole] = "locked";
    roles[VisibleRole] = "visible";
    roles[LoadedRole] = "loaded";
    roles[ScriptsRole] = "scripts";
    return roles;
}
