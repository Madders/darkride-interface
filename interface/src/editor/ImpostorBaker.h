//
//  ImpostorBaker.h
//  interface/src/editor
//
//  Bakes a model entity's impostor (IMPOSTORS.md 2.2): six orthographic frames of the entity's models (its own and its
//  descendants') drawn by the render engine's bake task, composed into three 3 x 2 atlases (albedo + coverage;
//  object-space normal + depth; roughness + metallic), written to the local impostors folder and to My Files, and
//  recorded in the entity's userData as the "impostor" entry the card renderer reads. Scripts reach it as
//  Impostors.bake(id, size); Impostors.bakeAll walks the world for everything that could take a card.
//
//  Created by the editor session on 2026-10-18.
//

#ifndef hifi_ImpostorBaker_h
#define hifi_ImpostorBaker_h

#include <memory>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QVector>

#include <DependencyManager.h>
#include <EntityItem.h>
#include <ImpostorBake.h>
#include <Model.h>

class EditorUpload;

class ImpostorBaker : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    ImpostorBaker();

    // bakes the entity, size x size pixels a frame (128 to 1024); queued behind a bake under way; baked() says how it went
    Q_INVOKABLE void bake(const QUuid& entityID, int size = 256);
    Q_INVOKABLE bool isBusy() const { return _job != nullptr || !_pending.empty(); }
    // where the atlases are written locally
    Q_INVOKABLE QString folder() const;
    // Bakes every entity of the world that is a model or groups models and has no entry yet (every one, with rebake):
    // as one card where the bounding sphere of its models is within maxRadius metres, else its children in turn (a
    // whole room's parent is no subject; its props are). Returns how many bakes were queued. From a script's thread
    // it waits for the main thread's answer.
    // Only the world's own entities (not an avatar's, not this client's local ones), and nothing smaller than
    // minRadius (a die needs no card).
    Q_INVOKABLE int bakeAll(int size = 256, float maxRadius = 4.0f, float minRadius = 0.3f, bool rebake = false);
    // what bakeAll would bake, one line each with the name and the radius: a dry run
    Q_INVOKABLE QStringList listBakeAll(float maxRadius = 4.0f, float minRadius = 0.3f, bool rebake = false);
    // takes the entry off the entity; the atlases stay where they are
    Q_INVOKABLE void remove(const QUuid& entityID);

signals:
    // message: the albedo atlas's URL when ok, the reason otherwise
    void baked(const QUuid& entityID, bool ok, const QString& message);

private:
    struct Job {
        QUuid id;
        int size { 256 };
        std::vector<ModelPointer> models;
        std::vector<ImpostorBake::FramePointer> frames;
        glm::vec3 center;       // the bounding sphere, in the entity's own frame
        float radius { 0.0f };
        QString stem;           // <name>-<id8>: the entity's files, whatever their bake time
        QString name;           // the stem, the bake time and ".impostor"
        QString albedoUrl;
        QString normalUrl;
        QString materialUrl;
        int uploadsLeft { 0 };
        QString uploadError;
        QElapsedTimer clock;
    };

    // meshWaits: how often the job has waited for its models' meshes already
    void start(const QUuid& entityID, int size, int meshWaits = 0);
    // asks the models under the entity that do not draw yet for their meshes (a far model's waits behind its card,
    // ENTITY-STREAMING.md 3.1) and says how many there are
    int askForMeshes(const EntityItemPointer& entity) const;
    // the entity's own loaded model and its descendants'
    void collectModels(const EntityItemPointer& entity, std::vector<ModelPointer>& models, bool log) const;
    // the entities bakeAll takes, with a line each for the listing
    QVector<QUuid> candidates(float maxRadius, float minRadius, bool rebake, QStringList& lines) const;
    static bool hasEntry(const EntityItemPointer& entity);
    void poll();
    void compose();
    void upload(const QByteArray& png, const QString& fileName, QString& urlOut);
    void writeEntry();
    void untag();
    void finish(bool ok, const QString& message);

    std::unique_ptr<Job> _job;
    std::vector<std::pair<QUuid, int>> _pending;
    QTimer _timer;
};

#endif // hifi_ImpostorBaker_h
