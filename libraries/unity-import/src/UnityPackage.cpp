//
//  UnityPackage.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnityPackage.h"

#include <algorithm>
#include <cstring>

#include <QFile>

#include <zlib.h>

namespace unity {

namespace {

void setError(QString* error, const QString& message) {
    if (error && error->isEmpty()) {
        *error = message;
    }
}

QString tarField(const char* p, int length) {
    return QString::fromUtf8(p, (int)strnlen(p, length));
}

// A tar number: octal text, or base-256 when the first byte's high bit is set.
qint64 tarNumber(const unsigned char* p, int length) {
    if (p[0] & 0x80) {
        qint64 value = p[0] & 0x7f;
        for (int k = 1; k < length; k++) {
            value = (value << 8) | p[k];
        }
        return value;
    }
    qint64 value = 0;
    for (int k = 0; k < length && p[k]; k++) {
        if (p[k] >= '0' && p[k] <= '7') {
            value = value * 8 + (p[k] - '0');
        }
    }
    return value;
}

bool isZeroBlock(const char* p) {
    for (int k = 0; k < 512; k++) {
        if (p[k]) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool gunzip(const QByteArray& compressed, QByteArray& out, QString* error) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        setError(error, "zlib would not start");
        return false;
    }
    out.clear();
    out.reserve(compressed.size() * 3);
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    stream.avail_in = (uInt)compressed.size();
    QByteArray chunk(1 << 20, Qt::Uninitialized);
    int result = Z_OK;
    while (true) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = (uInt)chunk.size();
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            setError(error, QString("not a gzip file, or a damaged one (zlib %1)").arg(result));
            inflateEnd(&stream);
            return false;
        }
        out.append(chunk.constData(), chunk.size() - (int)stream.avail_out);
        if (result == Z_STREAM_END) {
            if (stream.avail_in == 0) {
                break;
            }
            inflateReset(&stream);  // another gzip member follows
        }
    }
    inflateEnd(&stream);
    return true;
}

bool Package::open(const QString& fileName, QString* error) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, "cannot open " + fileName);
        return false;
    }
    QByteArray tar;
    if (!gunzip(file.readAll(), tar, error)) {
        return false;
    }
    return readTar(tar, error);
}

bool Package::readTar(const QByteArray& tar, QString* error) {
    QHash<QString, Asset> byGuid;
    QString longName;
    qint64 pos = 0;
    while (pos + 512 <= tar.size()) {
        const char* header = tar.constData() + pos;
        if (isZeroBlock(header)) {
            break;
        }
        QString name = tarField(header, 100);
        if (memcmp(header + 257, "ustar", 5) == 0) {
            const QString prefix = tarField(header + 345, 155);
            if (!prefix.isEmpty()) {
                name = prefix + '/' + name;
            }
        }
        const qint64 size = tarNumber(reinterpret_cast<const unsigned char*>(header + 124), 12);
        const char type = header[156];
        pos += 512;
        if (pos + size > tar.size()) {
            setError(error, "the package ends in the middle of " + name);
            return false;
        }
        const QByteArray content = tar.mid((int)pos, (int)size);
        pos += (size + 511) / 512 * 512;

        if (type == 'x') {  // a pax header: "<length> path=<name>\n" records for the next entry
            for (const QByteArray& record : content.split('\n')) {
                const int space = record.indexOf(' ');
                const QByteArray body = record.mid(space + 1);
                if (body.startsWith("path=")) {
                    longName = QString::fromUtf8(body.mid(5));
                }
            }
            continue;
        }
        if (type == 'L') {  // a GNU long name for the next entry
            longName = QString::fromUtf8(content.constData(), (int)strnlen(content.constData(), content.size()));
            continue;
        }
        if (type == 'g') {
            continue;
        }
        if (!longName.isEmpty()) {
            name = longName;
            longName.clear();
        }
        if (type != '0' && type != '\0') {
            continue;  // folders and links carry nothing a package needs
        }
        if (name.startsWith(QLatin1String("./"))) {
            name = name.mid(2);
        }
        const int slash = name.indexOf('/');
        if (slash <= 0) {
            continue;
        }
        const QString guid = name.left(slash);
        const QString part = name.mid(slash + 1);
        Asset& asset = byGuid[guid];
        asset.guid = guid;
        if (part == QLatin1String("asset")) {
            asset.data = content;
            asset.hasData = true;
        } else if (part == QLatin1String("asset.meta")) {
            asset.meta = content;
        } else if (part == QLatin1String("pathname")) {
            asset.path = QString::fromUtf8(content).split('\n').first().trimmed();
        }
    }
    _assets.clear();
    for (auto it = byGuid.begin(); it != byGuid.end(); ++it) {
        if (!it->path.isEmpty()) {
            _assets.push_back(*it);
        }
    }
    std::sort(_assets.begin(), _assets.end(), [](const Asset& a, const Asset& b) { return a.path < b.path; });
    _byGuid.clear();
    _byPath.clear();
    for (int k = 0; k < _assets.size(); k++) {
        _byGuid.insert(_assets[k].guid, k);
        _byPath.insert(_assets[k].path, k);
    }
    if (_assets.isEmpty()) {
        setError(error, "the file holds no Unity assets");
        return false;
    }
    return true;
}

const Package::Asset* Package::byGuid(const QString& guid) const {
    auto it = _byGuid.find(guid);
    return it == _byGuid.end() ? nullptr : &_assets[*it];
}

const Package::Asset* Package::byPath(const QString& path) const {
    auto it = _byPath.find(path);
    return it == _byPath.end() ? nullptr : &_assets[*it];
}

QList<const Package::Asset*> Package::withExtension(const QString& extension) const {
    QList<const Asset*> found;
    for (const Asset& asset : _assets) {
        if (asset.hasData && asset.path.endsWith(extension, Qt::CaseInsensitive)) {
            found.append(&asset);
        }
    }
    return found;
}

const QVector<YamlObject>& Package::objects(const QString& guid) const {
    auto it = _objects.find(guid);
    if (it == _objects.end()) {
        const Asset* asset = byGuid(guid);
        QString error;
        QVector<YamlObject> parsed = asset ? parseObjects(asset->data, &error) : QVector<YamlObject>();
        if (!error.isEmpty() && _parseError.isEmpty()) {
            _parseError = (asset ? asset->path : guid) + ": " + error;
        }
        it = _objects.insert(guid, parsed);
    }
    return *it;
}

const QVariantMap& Package::meta(const QString& guid) const {
    auto it = _metas.find(guid);
    if (it == _metas.end()) {
        const Asset* asset = byGuid(guid);
        QString error;
        QVariantMap parsed = asset ? parseDocument(asset->meta, &error).toMap() : QVariantMap();
        if (!error.isEmpty() && _parseError.isEmpty()) {
            _parseError = (asset ? asset->path : guid) + ".meta: " + error;
        }
        it = _metas.insert(guid, parsed);
    }
    return *it;
}

}  // namespace unity
