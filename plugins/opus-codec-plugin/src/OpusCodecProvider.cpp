//
//  OpusCodecProvider.cpp
//  plugins/opus-codec-plugin/src
//
//  The plug-in's entry point: the Opus codec, for the client and the audio mixer alike (see OpusCodec.h).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QtPlugin>

#include <plugins/RuntimePlugin.h>
#include <plugins/CodecPlugin.h>

#include "OpusCodec.h"

class OpusCodecProvider : public QObject, public CodecProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CodecProvider_iid FILE "plugin.json")
    Q_INTERFACES(CodecProvider)

public:
    OpusCodecProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~OpusCodecProvider() {}

    virtual CodecPluginList getCodecPlugins() override {
        static std::once_flag once;
        std::call_once(once, [&] {
            CodecPluginPointer opusCodec(new OpusCodec());
            if (opusCodec->isSupported()) {
                _codecPlugins.push_back(opusCodec);
            }
        });
        return _codecPlugins;
    }

private:
    CodecPluginList _codecPlugins;
};

#include "OpusCodecProvider.moc"
