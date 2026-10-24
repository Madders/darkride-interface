//
//  UserFiles.h
//  interface/src/editor/ui
//
//  The user's My Files, as the metaverse lists them (NATIVE-EDITOR.md §7): the whole tree in one listing, cached
//  here, and the calls that change it (a folder made, a move or rename, a delete, the private flag). Every change
//  is one API call followed by a fresh listing, which the index answers in milliseconds.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_UserFiles_h
#define hifi_UserFiles_h

#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtNetwork/QNetworkAccessManager>

class QNetworkReply;

// one file or folder of My Files
struct UserFile {
    enum class Kind { Folder, Picture, Model, Avatar, Sound, Script, Other };

    QString path;               // "/images/a.png"; a folder's path ends in "/"
    QString name;               // "a.png", "images"
    bool folder { false };
    qint64 size { 0 };
    QDateTime updated;          // invalid for a folder the listing did not name (one implied by a file's path)
    bool isPrivate { false };   // served only to its owner (the server calls it tea-only)
    QString id;                 // 16 letters and digits, fixed for the file's life; empty for a folder
    QString url;                // the id URL, <files host>/f/<id>/<name>, which survives moves; empty for a folder
    QString pathUrl;            // <files host>/<username>/<path>
    Kind kind { Kind::Other };

    static Kind kindOf(const QString& name, bool folder);
    static QString kindName(Kind kind);
    // the folder a path is in: "/images/a.png" -> "/images/", "/images/" -> "/", "/" -> ""
    static QString folderOf(const QString& path);
    static QString nameOf(const QString& path);
    static QString sizeText(qint64 bytes);
};

class UserFiles : public QObject {
    Q_OBJECT
public:
    explicit UserFiles(QObject* parent = nullptr);

    static bool isLoggedIn();
    bool isLoading() const { return _loading; }
    bool hasListing() const { return _listed; }
    const QVector<UserFile>& files() const { return _files; }
    const UserFile* find(const QString& path) const;
    // a folder's own entries ("/" for the root): folders first, then names in natural order, case-blind
    QVector<const UserFile*> childrenOf(const QString& folderPath) const;
    // every file under a folder, at any depth ("/" for all of them)
    int fileCountUnder(const QString& folderPath) const { return _countUnder.value(folderPath); }
    // a name no entry of the folder has: "New Folder", "New Folder 2", ...
    QString freeName(const QString& folderPath, const QString& wanted) const;
    qint64 usedSize() const { return _usedSize; }
    qint64 maxSize() const { return _maxSize; }
    // where the user's files are by path: "<files host>/<username>", from the listing
    QString baseUrl() const { return _baseUrl; }
    // the text with every link to one of the user's files by its path (<files host>/<username>/<path>, with any query
    // or fragment kept) replaced by the file's stable link, from the listing (Build > Use Stable Links). `rewritten`
    // counts the links replaced; `missing` gathers, once each, the path links to the user's files the listing has no
    // file for (moved, renamed or deleted since; the files host still follows a moved file's former paths). Links to
    // other users' files, stable links and private tea:// links are left as they are.
    QString withStableLinks(const QString& text, int& rewritten, QStringList& missing) const;

public slots:
    // fetches the listing (and the quota); nothing happens while one is on its way
    void refresh();
    // a listing only when the last is older than this: build mode coming on uses one that is fresh
    void refreshIfStale(qint64 maxAgeMs);
    void clear();
    void createFolder(const QString& folderPath);               // "/a/b/"
    // a rename is a move within the folder; a folder's paths end in "/"
    void move(const QString& path, const QString& newPath);
    void remove(const QString& path);
    void togglePrivate(const QString& path);

signals:
    void loadingChanged(bool loading);
    void listed();                       // the listing has arrived, or was dropped (clear)
    void listingFailed(const QString& why);   // the listing could not be fetched (failed says so too)
    void quotaChanged();                 // usedSize and maxSize are known
    // an operation has succeeded (a listing follows) or failed, in words for the status bar
    void changed(const QString& what);
    void failed(const QString& what);

private:
    QNetworkReply* send(const QString& route, QNetworkAccessManager::Operation operation,
                        const QList<QPair<QString, QString>>& query = QList<QPair<QString, QString>>());
    // runs an operation: on success says so and refreshes, on failure says why
    void run(const QString& route, QNetworkAccessManager::Operation operation,
             const QList<QPair<QString, QString>>& query, const QString& what);
    void onListing(QNetworkReply* reply);
    void setLoading(bool loading);
    static QString errorText(QNetworkReply* reply);

    QVector<UserFile> _files;
    QHash<QString, int> _byPath;
    QHash<QString, int> _countUnder;   // files at any depth, by folder path
    // each folder's entries (indexes into _files), sorted as childrenOf gives them: built once a listing
    QHash<QString, QVector<int>> _childrenByFolder;
    QString _baseUrl;               // <files host>/<username>
    bool _listed { false };
    QElapsedTimer _listedAt;
    bool _loading { false };
    bool _refreshWanted { false };  // a refresh asked for while one was on its way
    qint64 _usedSize { 0 };
    qint64 _maxSize { 0 };
};

#endif // hifi_UserFiles_h
