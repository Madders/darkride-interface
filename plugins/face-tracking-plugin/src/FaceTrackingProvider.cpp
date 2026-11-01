//
//  FaceTrackingProvider.cpp
//  plugins/face-tracking-plugin/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QtPlugin>

#include <plugins/RuntimePlugin.h>
#include <plugins/InputPlugin.h>

#include "FaceTrackingPlugin.h"

class FaceTrackingProvider : public QObject, public InputProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InputProvider_iid FILE "plugin.json")
    Q_INTERFACES(InputProvider)

public:
    FaceTrackingProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~FaceTrackingProvider() {}

    virtual InputPluginList getInputPlugins() override {
        static std::once_flag once;
        std::call_once(once, [&] {
            InputPluginPointer plugin(new FaceTrackingPlugin());
            if (plugin->isSupported()) {
                _inputPlugins.push_back(plugin);
            }
        });
        return _inputPlugins;
    }

    virtual void destroyInputPlugins() override {
        _inputPlugins.clear();
    }

private:
    InputPluginList _inputPlugins;
};

#include "FaceTrackingProvider.moc"
