//
//  EntityRightsTests.cpp
//  tests/entity-rights/src
//
//  What the entity server lets a node change, delete and add, by the rights the domain gives it: someone who may only
//  make temporary entities may only change and delete temporary ones, nobody but a world's editors deletes a locked
//  entity, and a new entity's certificate counts only from an editor. The deletes are sent as the server gets them, so
//  a locked entity's id goes in as it would from any client. A refused edit goes back out to its sender.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "EntityRightsTests.h"

#include <AddressManager.h>
#include <EntityItemProperties.h>
#include <EntityTree.h>
#include <EntityTypes.h>
#include <Node.h>
#include <NodeList.h>
#include <NodePermissions.h>
#include <ReceivedMessage.h>
#include <SharedUtil.h>
#include <SpatialParentFinder.h>
#include <udt/PacketHeaders.h>

QTEST_GUILESS_MAIN(EntityRightsTests)

namespace {

using Permission = NodePermissions::Permission;

// an entity's parent, found in the tree the entity is in (as the entity server's own finder does), so a delete sees
// a parent's children
class TreeParentFinder : public SpatialParentFinder {
public:
    SpatiallyNestableWeakPointer find(QUuid parentID, bool& success, SpatialParentTree* entityTree) const override {
        SpatiallyNestablePointer found = entityTree ? entityTree->findByID(parentID) : SpatiallyNestablePointer();
        success = found != nullptr;
        return found;
    }
};

SharedNodePointer makeNode(std::initializer_list<Permission> rights, bool all = false) {
    SharedNodePointer node(new Node(QUuid::createUuid(), NodeType::Agent, HifiSockAddr(), HifiSockAddr()));
    NodePermissions permissions;
    if (all) {
        permissions.setAll(true);
    }
    permissions.set(Permission::canConnectToDomain);
    for (auto right : rights) {
        permissions.set(right);
    }
    node->setPermissions(permissions);
    return node;
}

// who's sending: a visitor with the temporary rights (as a world gives everyone signed in), one with only the certified
// temporary right, a builder who may make permanent entities, a world's editor (every right), and someone who may only
// come in
struct Senders {
    SharedNodePointer visitor { makeNode({ Permission::canRezTemporaryEntities, Permission::canRezTemporaryCertifiedEntities }) };
    SharedNodePointer certifiedVisitor { makeNode({ Permission::canRezTemporaryCertifiedEntities }) };
    SharedNodePointer builder { makeNode({ Permission::canRezPermanentEntities, Permission::canRezTemporaryEntities }) };
    SharedNodePointer editor { makeNode({}, true) };
    SharedNodePointer guest { makeNode({}) };
};

struct World {
    EntityTreePointer tree;
    EntityItemID permanentID { QUuid("{a0000000-0000-0000-0000-000000000001}") };
    EntityItemID temporaryID { QUuid("{a0000000-0000-0000-0000-000000000002}") };
    EntityItemID lockedID { QUuid("{a0000000-0000-0000-0000-000000000003}") };
    EntityItemID grabbableID { QUuid("{a0000000-0000-0000-0000-000000000004}") };
    EntityItemID parentID { QUuid("{a0000000-0000-0000-0000-000000000005}") };   // temporary
    EntityItemID childID { QUuid("{a0000000-0000-0000-0000-000000000006}") };    // permanent, the parent's child

    World() {
        tree = EntityTreePointer(new EntityTree(true));
        tree->createRootElement();
        tree->setIsServer(true);
        add(permanentID, "permanent");
        add(temporaryID, "temporary", 300.0f);
        add(lockedID, "locked", ENTITY_ITEM_IMMORTAL_LIFETIME, true);
        add(grabbableID, "grabbable", ENTITY_ITEM_IMMORTAL_LIFETIME, false, true);
        add(parentID, "parent", 300.0f);
        add(childID, "child", ENTITY_ITEM_IMMORTAL_LIFETIME, false, false, parentID);
    }

    void add(const EntityItemID& id, const QString& name, float lifetime = ENTITY_ITEM_IMMORTAL_LIFETIME, bool locked = false,
             bool grabbable = false, const QUuid& parent = QUuid()) {
        EntityItemProperties properties;
        properties.setType(EntityTypes::Box);
        properties.setName(name);
        properties.setPosition(glm::vec3(100.0f));  // well inside the octree's domain, which starts at the origin
        properties.setDimensions(glm::vec3(0.5f));
        properties.setLifetime(lifetime);
        properties.setLocked(locked);
        properties.getGrab().setGrabbable(grabbable);
        if (!parent.isNull()) {
            properties.setParentID(parent);
        }
        properties.setCreated(usecTimestampNow());
        tree->withWriteLock([&] { tree->addEntity(id, properties); });
    }

    EntityItemPointer find(const EntityItemID& id) {
        EntityItemPointer entity;
        tree->withReadLock([&] { entity = tree->findEntityByEntityItemID(id); });
        return entity;
    }

    QString name(const EntityItemID& id) {
        auto entity = find(id);
        return entity ? entity->getName() : QString();
    }

    // an edit of the name (and, given, the position), as a client sends it; when it was made
    quint64 rename(const EntityItemID& id, const SharedNodePointer& sender, const QString& name,
                   const glm::vec3* position = nullptr) {
        EntityItemProperties properties;
        properties.setName(name);
        if (position) {
            properties.setPosition(*position);
        }
        properties.setLastEdited(usecTimestampNow() + 1);
        tree->withWriteLock([&] { tree->updateEntity(id, properties, sender); });
        return properties.getLastEdited();
    }

    // whether the entity goes back out last edited after `edit`, so a client that showed that edit takes it back
    bool sentBackAfter(const EntityItemID& id, quint64 edit) {
        auto entity = find(id);
        return entity && entity->getLastEdited() > edit;
    }

    // a delete, as the entity server gets it from a client
    void erase(const EntityItemID& id, const SharedNodePointer& sender) {
        QByteArray buffer(64, 0);
        QVERIFY(EntityItemProperties::encodeEraseEntityMessage(id, buffer));
        tree->withWriteLock([&] { tree->processEraseMessageDetails(buffer, sender); });
    }

    // an add, as the entity server gets it from a client; the new entity, if it was made
    EntityItemPointer addFrom(const SharedNodePointer& sender, const QString& certificateID) {
        EntityItemID id(QUuid::createUuid());
        EntityItemProperties properties;
        properties.setType(EntityTypes::Box);
        properties.setName("new");
        properties.setPosition(glm::vec3(100.0f));
        properties.setDimensions(glm::vec3(0.5f));
        properties.setLifetime(60.0f);
        properties.setCertificateID(certificateID);
        properties.setLastEdited(usecTimestampNow());
        QByteArray buffer(NLPacket::maxPayloadSize(PacketType::EntityAdd) * 10, 0);
        EntityPropertyFlags didntFit;
        auto result = EntityItemProperties::encodeEntityEditPacket(PacketType::EntityAdd, id, properties, buffer,
                                                                   properties.getChangedProperties(), didntFit);
        if (result != OctreeElement::COMPLETED) {
            qWarning() << "the add didn't encode in one go";
            return EntityItemPointer();
        }
        ReceivedMessage message(buffer, PacketType::EntityAdd, versionForPacketType(PacketType::EntityAdd), HifiSockAddr());
        tree->withWriteLock([&] {
            tree->processEditPacketData(message, reinterpret_cast<const unsigned char*>(buffer.constData()), buffer.size(),
                                        sender);
        });
        return find(id);
    }
};

}  // namespace

void EntityRightsTests::initTestCase() {
    // the tree adds an entity only with a node list about (as in the edit filter tests)
    DependencyManager::set<AddressManager>();
    DependencyManager::set<NodeList>(NodeType::Agent, INVALID_PORT);
    DependencyManager::set<SpatialParentFinder, TreeParentFinder>();
}

void EntityRightsTests::testEdits() {
    World world;
    Senders senders;
    QVERIFY(world.find(world.permanentID) && world.find(world.grabbableID));

    // a temporary right changes temporary entities only
    world.rename(world.temporaryID, senders.visitor, "visitor's");
    QCOMPARE(world.name(world.temporaryID), QString("visitor's"));
    auto refused = world.rename(world.permanentID, senders.visitor, "visitor's");
    QCOMPARE(world.name(world.permanentID), QString("permanent"));
    QVERIFY(world.sentBackAfter(world.permanentID, refused));

    // a permanent entity that can be grabbed may still be moved by anyone, as by someone without rez rights, but not changed
    glm::vec3 elsewhere(101.0f, 100.0f, 100.0f);
    world.rename(world.grabbableID, senders.visitor, "visitor's", &elsewhere);
    QCOMPARE(world.name(world.grabbableID), QString("grabbable"));
    QCOMPARE(world.find(world.grabbableID)->getWorldPosition(), elsewhere);

    // someone who may only come in changes nothing; a permanent right, or an editor's, changes permanent entities
    refused = world.rename(world.temporaryID, senders.guest, "guest's");
    QCOMPARE(world.name(world.temporaryID), QString("visitor's"));
    QVERIFY(world.sentBackAfter(world.temporaryID, refused));
    world.rename(world.permanentID, senders.builder, "builder's");
    QCOMPARE(world.name(world.permanentID), QString("builder's"));
    world.rename(world.permanentID, senders.editor, "editor's");
    QCOMPARE(world.name(world.permanentID), QString("editor's"));

    // a locked entity stays as it is for all but its lock, as before
    refused = world.rename(world.lockedID, senders.builder, "builder's");
    QCOMPARE(world.name(world.lockedID), QString("locked"));
    QVERIFY(world.sentBackAfter(world.lockedID, refused));
}

void EntityRightsTests::testDeletes() {
    World world;
    Senders senders;

    // someone who may only come in deletes nothing, not even a temporary entity
    world.erase(world.temporaryID, senders.guest);
    QVERIFY(world.find(world.temporaryID));

    // a temporary right deletes temporary entities, not permanent ones, nor a temporary one with a permanent child
    world.erase(world.permanentID, senders.visitor);
    QVERIFY(world.find(world.permanentID));
    world.erase(world.parentID, senders.visitor);
    QVERIFY(world.find(world.parentID));
    QVERIFY(world.find(world.childID));
    world.erase(world.temporaryID, senders.visitor);
    QVERIFY(!world.find(world.temporaryID));

    // nobody but an editor deletes a locked entity, whatever the client sends
    world.erase(world.lockedID, senders.visitor);
    QVERIFY(world.find(world.lockedID));
    world.erase(world.lockedID, senders.builder);
    QVERIFY(world.find(world.lockedID));

    // a permanent right deletes permanent entities, children with their parent; an editor deletes locked ones
    world.erase(world.parentID, senders.builder);
    QVERIFY(!world.find(world.parentID));
    QVERIFY(!world.find(world.childID));
    world.erase(world.permanentID, senders.builder);
    QVERIFY(!world.find(world.permanentID));
    world.erase(world.lockedID, senders.editor);
    QVERIFY(!world.find(world.lockedID));
}

void EntityRightsTests::testCertificateClaims() {
    World world;
    Senders senders;

    // the claim is dropped: with only the certified temporary right, nothing is made
    QVERIFY(!world.addFrom(senders.certifiedVisitor, "a certificate nobody issued"));

    // with the uncertified temporary right, the entity is made, without the claim
    auto made = world.addFrom(senders.visitor, "a certificate nobody issued");
    QVERIFY(made);
    QVERIFY(made->getCertificateID().isEmpty());

    // an editor's add keeps its certificate (content brought in from elsewhere)
    auto brought = world.addFrom(senders.editor, "an old certificate");
    QVERIFY(brought);
    QCOMPARE(brought->getCertificateID(), QString("an old certificate"));
}
