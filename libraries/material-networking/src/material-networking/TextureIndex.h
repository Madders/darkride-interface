//
//  TextureIndex.h
//  libraries/material-networking/src/material-networking
//
//  Which KTX cache entry a texture URL loaded last time, kept across runs (ARCHITECTURE.md §3.12, phase 2b). A KTX
//  texture starts with two byte-range requests (its header and its top mips), and only the header names the source
//  hash under which the KTX cache holds the texture; Qt never serves a range request from its disk cache, so every
//  visit paid both requests for every texture it already had on disk. The index remembers, per URL, the hash and the
//  original file's key-value size (the cached KTX carries more key-values, and the later mip requests' byte ranges
//  are the original's), so a texture in the KTX cache is built from disk with no request; a URL whose entry has no
//  file any more, or that is refreshed (Reload Content), loads from the network as before. Entries expire after a
//  day, the same freshness Qt's heuristic grants the client's other textures. The map is ktx_index.json beside the
//  KTX cache, written at most every few seconds and at exit.
//
//  Distributed under the MIT License.
//

#ifndef hifi_TextureIndex_h
#define hifi_TextureIndex_h

#include <string>

#include <QtCore/QUrl>

class TextureIndex {
public:
    struct Entry {
        std::string hash;          // the source hash, hex: the KTX cache's key
        uint32_t keyValueBytes;    // the original KTX header's bytesOfKeyValueData
    };
    /// The remembered entry for url, or an entry with an empty hash.
    static Entry lookup(const QUrl& url);
    static void remember(const QUrl& url, const std::string& hash, uint32_t keyValueBytes);
    static void forget(const QUrl& url);
    /// Writes the map if it changed since the last write.
    static void save();

    static const qint64 MAX_AGE_MSECS = 24 * 60 * 60 * 1000;
};

#endif // hifi_TextureIndex_h
