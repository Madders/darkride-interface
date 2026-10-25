//
//  EditorCreate.h
//  interface/src/editor
//
//  The Create catalogue: what the Create menus list and the properties a new entity starts with;
//  and the upload of a dropped file to the user's files on the metaverse.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorCreate_h
#define hifi_EditorCreate_h

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <glm/glm.hpp>

#include <EntityItemProperties.h>
#include <EntityTypes.h>

namespace EditorCreate {
    struct Item {
        QString key;                    // "cube", "spot", "model" ...
        QString label;                  // the menu text
        QString group;                  // a submenu, or empty for the top level; "/" nests ("Special items/Vehicles")
        EntityTypes::EntityType type;
        bool separatorBefore { false }; // a separator above this item (or above its group)
        // a ready-made family of entities: an entity file in the app's resources, placed whole (Editor::createPreset)
        QString preset {};
        float ahead { 0.0f };           // a big one goes at least this far from the camera (the airship)
        // greyed in the menu with this as its tooltip (a kind the engine can't make yet); or a tooltip for one it can
        bool disabled { false };
        QString tooltip {};
    };
    const QVector<Item>& items();
    const Item* item(const QString& key);

    // the starting properties of a new entity of this kind at position; panels (text, image, web,
    // quad, circle) turn to face the camera, whose yaw is given
    EntityItemProperties properties(const QString& key, const glm::vec3& position, float cameraYaw);
    // a new model is a box this size until it loads and reports its own
    const glm::vec3 MODEL_PLACEHOLDER_DIMENSIONS { 1.0f };
    EntityItemProperties modelProperties(const QUrl& url, const glm::vec3& position);
    EntityItemProperties imageProperties(const QUrl& url, const glm::vec3& position, float cameraYaw);

    // how far above the surface it was placed on the new entity's position sits
    float lift(const EntityItemProperties& properties);

    bool isModelFile(const QString& name);
    bool isImageFile(const QString& name);
}

// uploads a local file, or bytes made here, to the user's files on the metaverse (PUT /api/user/files,
// multipart) and reports the file's public URL; a file sent to a path again replaces the one there
class EditorUpload : public QObject {
    Q_OBJECT
public:
    explicit EditorUpload(QObject* parent = nullptr);

    // starts the upload into remoteFolder ("/models"); false, with the reason, if it cannot start
    bool start(const QString& localPath, const QString& remoteFolder, QString& error);
    // the same for bytes already in memory (a terrain's file), named fileName in remoteFolder
    bool startData(const QByteArray& data, const QString& fileName, const QString& remoteFolder, const QString& mimeType,
                   QString& error);
    QString fileName() const { return _fileName; }
    // the name a file is stored under: letters, digits, dot, dash and underscore; anything else becomes "_"
    static QString safeFileName(const QString& name);

signals:
    void progress(qint64 sent, qint64 total);
    void finished(const QString& url);
    void failed(const QString& error);

private:
    // takes the body (deleted here if the upload cannot start, owned by the request otherwise)
    bool send(QIODevice* body, const QString& fileName, const QString& remoteFolder, const QString& mimeType, qint64 size,
              QString& error);

    QString _fileName;
    QElapsedTimer _timer;
};

#endif // hifi_EditorCreate_h
