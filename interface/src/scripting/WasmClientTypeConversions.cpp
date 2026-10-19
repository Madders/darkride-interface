//
//  WasmClientTypeConversions.cpp
//  interface/src/scripting
//
//  The WebAssembly script engine's conversions (ValueCodec, libraries/wasm-scripting) for the types of the client
//  and of the libraries it alone links (the Graphics API) that have only QtScript conversions, registered when the
//  program starts, as
//  libraries/script-engine/src/WasmTypeConversions.cpp does for the library types. Each gives a module the
//  shape the QtScript conversion gives a script, member for member and in the same order, and reads one back
//  as the QtScript conversion reads it; the QtScript original is named beside each.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>

#include <BoxBase.h>
#include <RegisteredMetaTypes.h>
#include <ValueCodec.h>
#include <graphics/GpuHelpers.h>
#include <graphics/Material.h>
#include <graphics-scripting/ScriptableMesh.h>
#include <graphics-scripting/ScriptableMeshPart.h>
#include <graphics-scripting/ScriptableModel.h>

#include "AccountServicesScriptingInterface.h"
#include "ui/InteractiveWindow.h"
#include "ui/overlays/Overlays.h"

namespace {

QVariant variantOf(const ValueCodec& codec, const QCborMap& object, const char* name) {
    return codec.fromCbor(object.value(QLatin1String(name)));
}

template <typename T>
T typed(const ValueCodec& codec, const QCborValue& value) {
    return codec.fromCbor(value, qMetaTypeId<T>()).template value<T>();
}

// RayToOverlayIntersectionResultToScriptValue, ...FromScriptValue (ui/overlays/Overlays.cpp)
QCborValue encodeRayToOverlayIntersectionResult(const ValueCodec& codec, const QVariant& value) {
    const auto result = value.value<RayToOverlayIntersectionResult>();
    QCborMap map;
    map.insert(QStringLiteral("intersects"), result.intersects);
    map.insert(QStringLiteral("overlayID"), result.overlayID.isNull() ? QCborValue(nullptr) : QCborValue(result.overlayID.toString()));
    map.insert(QStringLiteral("distance"), double(result.distance));
    map.insert(QStringLiteral("face"), boxFaceToString(result.face));
    map.insert(QStringLiteral("intersection"), codec.toCbor(QVariant::fromValue(result.intersection)));
    map.insert(QStringLiteral("surfaceNormal"), codec.toCbor(QVariant::fromValue(result.surfaceNormal)));
    map.insert(QStringLiteral("extraInfo"), codec.toCbor(result.extraInfo));
    return map;
}

bool decodeRayToOverlayIntersectionResult(const ValueCodec& codec, const QCborValue& value, QVariant& out) {
    const QCborMap object = value.toMap();
    RayToOverlayIntersectionResult result;
    result.intersects = variantOf(codec, object, "intersects").toBool();
    result.overlayID = typed<QUuid>(codec, object.value(QLatin1String("overlayID")));
    result.distance = variantOf(codec, object, "distance").toFloat();
    result.face = boxFaceFromString(variantOf(codec, object, "face").toString());
    if (object.contains(QLatin1String("intersection"))) {
        result.intersection = typed<glm::vec3>(codec, object.value(QLatin1String("intersection")));
    }
    if (object.contains(QLatin1String("surfaceNormal"))) {
        result.surfaceNormal = typed<glm::vec3>(codec, object.value(QLatin1String("surfaceNormal")));
    }
    result.extraInfo = variantOf(codec, object, "extraInfo").toMap();
    out = QVariant::fromValue(result);
    return true;
}

// DownloadInfoResultToScriptValue, ...FromScriptValue (scripting/AccountServicesScriptingInterface.cpp)
QCborValue encodeDownloadInfoResult(const ValueCodec&, const QVariant& value) {
    const auto result = value.value<DownloadInfoResult>();
    QCborArray downloading;
    for (float percent : result.downloading) {
        downloading.append(double(percent));
    }
    QCborMap map;
    map.insert(QStringLiteral("downloading"), downloading);
    map.insert(QStringLiteral("pending"), double(result.pending));
    return map;
}

bool decodeDownloadInfoResult(const ValueCodec& codec, const QCborValue& value, QVariant& out) {
    const QCborMap object = value.toMap();
    DownloadInfoResult result;
    for (const QVariant& percent : variantOf(codec, object, "downloading").toList()) {
        result.downloading.append(percent.toFloat());
    }
    result.pending = variantOf(codec, object, "pending").toFloat();
    out = QVariant::fromValue(result);
    return true;
}

// interactiveWindowPointerToScriptValue gives the window to the script with ScriptOwnership (ui/InteractiveWindow.cpp):
// the engine's object table owns it. A window the script let go of stays open until the script stops (QtScript's
// collector closed it at a time of its own; a script that opens a window and keeps no reference means it to stay).
QCborValue encodeInteractiveWindow(const ValueCodec& codec, const QVariant& value) {
    return codec.toCborOwned(value.value<InteractiveWindowPointer>(), [](QObject*) { return true; });
}

bool decodeInteractiveWindow(const ValueCodec& codec, const QCborValue& value, QVariant& out) {
    out = QVariant::fromValue(qobject_cast<InteractiveWindowPointer>(codec.fromCbor(value).value<QObject*>()));
    return true;
}

// --- the Graphics API (libraries/graphics-scripting/src/graphics-scripting/GraphicsScriptingInterface.cpp) ---------

// scriptableMaterialToScriptValue: what the material sets, a property that falls through to the material below as
// "fallthrough", the PBR model's properties only for that model
bool fallsThrough(const scriptable::ScriptableMaterial& material, uint property) {
    auto found = material.propertyFallthroughs.find(property);
    return found != material.propertyFallthroughs.end() && found->second;
}

QCborValue colorOf(const glm::vec3& color) {
    // vec3ColorToScriptValue: {red, green, blue}, the components as they are (0 to 1)
    QCborMap map;
    map.insert(QStringLiteral("red"), double(color.x));
    map.insert(QStringLiteral("green"), double(color.y));
    map.insert(QStringLiteral("blue"), double(color.z));
    return map;
}

QCborValue encodeMaterial(const ValueCodec& codec, const scriptable::ScriptableMaterial& material) {
    using Key = graphics::MaterialKey;
    using Material = graphics::Material;
    const QString FALLTHROUGH = QStringLiteral("fallthrough");
    QCborMap map;
    map.insert(QStringLiteral("name"), material.name);
    map.insert(QStringLiteral("model"), material.model);
    // a value, or "fallthrough", or nothing
    auto put = [&](const char* name, uint property, bool present, const QCborValue& value) {
        if (fallsThrough(material, property)) {
            map.insert(QLatin1String(name), FALLTHROUGH);
        } else if (present) {
            map.insert(QLatin1String(name), value);
        }
    };
    put("opacity", Key::OPACITY_VAL_BIT, material.key.isTranslucentFactor(), double(material.opacity));
    put("albedo", Key::ALBEDO_VAL_BIT, material.key.isAlbedo(), colorOf(material.albedo));
    if (material.model.toStdString() == Material::HIFI_PBR) {
        put("opacityCutoff", Key::OPACITY_CUTOFF_VAL_BIT, material.key.isOpacityCutoff(), double(material.opacityCutoff));
        put("opacityMapMode", Key::OPACITY_MAP_MODE_BIT, material.key.isOpacityMapMode(), material.opacityMapMode);
        put("roughness", Key::GLOSSY_VAL_BIT, material.key.isGlossy(), double(material.roughness));
        put("metallic", Key::METALLIC_VAL_BIT, material.key.isMetallic(), double(material.metallic));
        put("scattering", Key::SCATTERING_VAL_BIT, material.key.isScattering(), double(material.scattering));
        put("unlit", Key::UNLIT_VAL_BIT, material.key.isUnlit(), material.unlit);
        put("emissive", Key::EMISSIVE_VAL_BIT, material.key.isEmissive(), colorOf(material.emissive));
        put("emissiveMap", Key::EMISSIVE_MAP_BIT, !material.emissiveMap.isEmpty(), material.emissiveMap);
        put("albedoMap", Key::ALBEDO_MAP_BIT, !material.albedoMap.isEmpty(), material.albedoMap);
        if (!material.opacityMap.isEmpty()) {
            map.insert(QStringLiteral("opacityMap"), material.opacityMap);
        }
        put("occlusionMap", Key::OCCLUSION_MAP_BIT, !material.occlusionMap.isEmpty(), material.occlusionMap);
        put("lightMap", Key::LIGHT_MAP_BIT, !material.lightMap.isEmpty(), material.lightMap);
        put("scatteringMap", Key::SCATTERING_MAP_BIT, !material.scatteringMap.isEmpty(), material.scatteringMap);
        // only one of each of these
        if (fallsThrough(material, Key::METALLIC_MAP_BIT)) {
            map.insert(QStringLiteral("metallicMap"), FALLTHROUGH);
        } else if (!material.metallicMap.isEmpty()) {
            map.insert(QStringLiteral("metallicMap"), material.metallicMap);
        } else if (!material.specularMap.isEmpty()) {
            map.insert(QStringLiteral("specularMap"), material.specularMap);
        }
        if (fallsThrough(material, Key::ROUGHNESS_MAP_BIT)) {
            map.insert(QStringLiteral("roughnessMap"), FALLTHROUGH);
        } else if (!material.roughnessMap.isEmpty()) {
            map.insert(QStringLiteral("roughnessMap"), material.roughnessMap);
        } else if (!material.glossMap.isEmpty()) {
            map.insert(QStringLiteral("glossMap"), material.glossMap);
        }
        if (fallsThrough(material, Key::NORMAL_MAP_BIT)) {
            map.insert(QStringLiteral("normalMap"), FALLTHROUGH);
        } else if (!material.normalMap.isEmpty()) {
            map.insert(QStringLiteral("normalMap"), material.normalMap);
        } else if (!material.bumpMap.isEmpty()) {
            map.insert(QStringLiteral("bumpMap"), material.bumpMap);
        }
        put("texCoordTransform0", Material::TEXCOORDTRANSFORM0, material.texCoordTransforms[0] != glm::mat4(),
            codec.toCbor(QVariant::fromValue(material.texCoordTransforms[0])));
        put("texCoordTransform1", Material::TEXCOORDTRANSFORM1, material.texCoordTransforms[1] != glm::mat4(),
            codec.toCbor(QVariant::fromValue(material.texCoordTransforms[1])));
        // these two are only ever "fallthrough"
        put("lightmapParams", Material::LIGHTMAP_PARAMS, false, QCborValue());
        put("materialParams", Material::MATERIAL_PARAMS, false, QCborValue());
        put("cullFaceMode", Material::CULL_FACE_MODE, !material.cullFaceMode.isEmpty(), material.cullFaceMode);
    } else if (material.model.toStdString() == Material::HIFI_SHADER_SIMPLE) {
        map.insert(QStringLiteral("procedural"), material.procedural);
    }
    map.insert(QStringLiteral("defaultFallthrough"), material.defaultFallthrough);
    return map;
}

QCborValue encodeMaterialValue(const ValueCodec& codec, const QVariant& value) {
    return encodeMaterial(codec, value.value<scriptable::ScriptableMaterial>());
}

// scriptableMaterialLayerToScriptValue: {material, priority}
QCborValue encodeMaterialLayer(const ValueCodec& codec, const QVariant& value) {
    const auto layer = value.value<scriptable::ScriptableMaterialLayer>();
    QCborMap map;
    map.insert(QStringLiteral("material"), encodeMaterial(codec, layer.material));
    map.insert(QStringLiteral("priority"), qint64(layer.priority));
    return map;
}

// multiMaterialMapToScriptValue: the layers by material name, in the map's own order
QCborValue encodeMultiMaterialMap(const ValueCodec& codec, const QVariant& value) {
    const auto layersByName = value.value<scriptable::MultiMaterialMap>();
    QCborMap map;
    for (const QString& name : layersByName.keys()) {
        QCborArray layers;
        for (const scriptable::ScriptableMaterialLayer& layer : layersByName.value(name)) {
            layers.append(encodeMaterialLayer(codec, QVariant::fromValue(layer)));
        }
        map.insert(name, layers);
    }
    return map;
}

// No method takes a material or a layer from a script (their QtScript conversions read nothing either).
bool decodeNone(const ValueCodec&, const QCborValue&, QVariant&) {
    return false;
}

// A topology crosses as its name ("triangles"), as registerDebugEnum gives it a script, and back from the name.
QCborValue encodeTopology(const ValueCodec&, const QVariant& value) {
    return graphics::TOPOLOGIES.value(value.value<graphics::Mesh::Topology>());
}

bool decodeTopology(const ValueCodec& codec, const QCborValue& value, QVariant& out) {
    out = QVariant::fromValue(graphics::TOPOLOGIES.key(codec.fromCbor(value, QMetaType::QString).toString()));
    return true;
}

// QPointer<T>: the object's handle (QtOwnership), and back from a handle of that class (registerQPointerMetaType).
template <typename T>
void addGraphicsPointer() {
    ValueCodec::registerType(qMetaTypeId<QPointer<T>>(),
        [](const ValueCodec& codec, const QVariant& value) { return codec.toCbor(QVariant::fromValue<QObject*>(value.value<QPointer<T>>().data())); },
        [](const ValueCodec& codec, const QCborValue& value, QVariant& out) {
            out = QVariant::fromValue(QPointer<T>(qobject_cast<T*>(codec.fromCbor(value).value<QObject*>())));
            return true;
        });
    ValueCodec::registerType(qMetaTypeId<QVector<QPointer<T>>>(),
        [](const ValueCodec& codec, const QVariant& value) {
            QCborArray array;
            for (const QPointer<T>& object : value.value<QVector<QPointer<T>>>()) {
                array.append(codec.toCbor(QVariant::fromValue<QObject*>(object.data())));
            }
            return QCborValue(array);
        },
        [](const ValueCodec& codec, const QCborValue& value, QVariant& out) {
            QVector<QPointer<T>> objects;
            for (const QCborValue& item : value.toArray()) {
                objects.append(QPointer<T>(qobject_cast<T*>(codec.fromCbor(item).value<QObject*>())));
            }
            out = QVariant::fromValue(objects);
            return true;
        });
}

[[maybe_unused]] const bool registered = [] {
    ValueCodec::registerType(qMetaTypeId<RayToOverlayIntersectionResult>(), &encodeRayToOverlayIntersectionResult,
                             &decodeRayToOverlayIntersectionResult);
    ValueCodec::registerType(qMetaTypeId<DownloadInfoResult>(), &encodeDownloadInfoResult, &decodeDownloadInfoResult);
    ValueCodec::registerType(qMetaTypeId<InteractiveWindowPointer>(), &encodeInteractiveWindow, &decodeInteractiveWindow);

    ValueCodec::registerType(qMetaTypeId<scriptable::ScriptableMaterial>(), &encodeMaterialValue, &decodeNone);
    ValueCodec::registerType(qMetaTypeId<scriptable::ScriptableMaterialLayer>(), &encodeMaterialLayer, &decodeNone);
    ValueCodec::registerType(qMetaTypeId<scriptable::MultiMaterialMap>(), &encodeMultiMaterialMap, &decodeNone);
    ValueCodec::registerType(qMetaTypeId<graphics::Mesh::Topology>(), &encodeTopology, &decodeTopology);
    addGraphicsPointer<scriptable::ScriptableModel>();
    addGraphicsPointer<scriptable::ScriptableMesh>();
    addGraphicsPointer<scriptable::ScriptableMeshPart>();
    return true;
}();

} // namespace
