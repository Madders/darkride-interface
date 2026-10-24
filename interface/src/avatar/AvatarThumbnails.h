//
//  AvatarThumbnails.h
//  interface/src/avatar
//
//  An avatar's thumbnail (Caity, 10-24: "a little picture of each one ... like a snap taken from a camera or an image
//  you can upload"): the picture the panels show for an avatar in My Files, kept beside its .fst as
//  <name>.preview.jpg, the file an .fst's standard `preview` line names. A picture is taken with the secondary camera
//  (the spectator camera's), placed in front of you for one frame and then put back as it was; or made from an image
//  on this computer. Either is cropped to 4:3, 640×480, a JPEG. The Avatar panel uploads one beside the avatar worn
//  when that avatar is in your own My Files; the avatar importer takes one when it puts an avatar there.
//  QML panels only, as MetaverseRequests: scripts never see the object.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_AvatarThumbnails_h
#define hifi_AvatarThumbnails_h

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtGui/QImage>
#include <QtQml/QJSValue>

#include <DependencyManager.h>

class AvatarThumbnails : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    using Done = std::function<void(const QByteArray& jpeg, const QString& error)>;

    static const int WIDTH { 640 };
    static const int HEIGHT { 480 };

    /// a picture of you from the front, chest up, once the avatar worn has loaded (at most 20 s), as a JPEG
    void capture(Done done);

    /// the same picture for a panel: callback(error, url), url a file:// address of the JPEG kept here
    Q_INVOKABLE void take(QJSValue callback);
    /// a picture made from the image at path (a file:// address or a local path): callback(error, url)
    Q_INVOKABLE void fromFile(const QString& path, QJSValue callback);
    /// { folder, name } of an avatar's .fst in the signed-in user's My Files ("/avatars/meehae", "meehae"), or {}
    Q_INVOKABLE QVariantMap myFilesPlace(const QString& fstUrl) const;
    /// the path in My Files an avatar's picture has ("/avatars/meehae/meehae.preview.jpg"), or "" when it isn't yours
    Q_INVOKABLE QString pictureKey(const QString& fstUrl) const;
    /// uploads the picture at pictureUrl as <name>.preview.jpg beside the .fst: callback(error, url)
    Q_INVOKABLE void upload(const QString& pictureUrl, const QString& fstUrl, QJSValue callback);

    /// the picture's file name for an .fst named name, as the uploads store it ("Auxi Day" → "Auxi_Day.preview.jpg")
    static QString pictureName(const QString& name);
    /// an image cropped to 4:3 and scaled to 640×480, as a JPEG
    static QByteArray fit(const QImage& image);

private:
    AvatarThumbnails() = default;

    void shoot(Done done);
    QString keep(const QByteArray& jpeg, QString& error) const;

    bool _taking { false };
};

#endif // hifi_AvatarThumbnails_h
