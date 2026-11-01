//
//  PropertySpec.h
//  interface/src/editor
//
//  The property table the Inspector is generated from: every editable entity property with its
//  kind, label, section, range and enum values, which entity types show which sections, and the
//  accessors that read and write a property by key on an EntityItemProperties. One table, so a
//  property cannot be editable in one place and not another.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PropertySpec_h
#define hifi_PropertySpec_h

#include <cfloat>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <EntityItemProperties.h>
#include <EntityTypes.h>

// How a property is edited. The QVariant carried for each kind:
//   Bool: bool  Int, Float, Slider: double  Degrees: double degrees (stored in radians)
//   String, Url, Json, Image (the picture's URL), UserData (the whole text), Enum (the value name),
//   CollisionMask (the comma list): QString   TerrainLayers: the property's text, unused (the layers live in the
//   terrain file on this client, which the editor reads and sets directly)   Impostor: the entry's JSON, empty for
//   none (setting it empty removes the entry; the bake itself goes through the ImpostorBaker)
//   Color, FloatColor: QColor  Vec2, Polar (elevation and azimuth in degrees): QVector2D
//   Vec3, Euler (degrees): QVector3D  Rect: QRect
enum class PropertyKind {
    Bool,
    Int,
    Float,
    Slider,     // a number in a range, with a slider beside its field, as Unity shows a shader's Range
    Degrees,
    String,
    Url,
    Json,
    Image,      // a picture, by URL: a thumbnail, Choose… (uploads one from this computer) and drag and drop
    UserData,   // userData as a list of entries, each with a field that suits its value
    Color,
    FloatColor,
    Vec2,
    Vec3,
    Euler,
    Polar,
    Rect,
    Enum,
    CollisionMask,
    TerrainLayers,  // a Terrain's material layers: the list, with the selected layer's settings (TerrainLayersEditor)
    TerrainCaps,    // a Terrain's closed box: a checkbox read and set through the Terrain tool, not a property (TerrainCapsEditor)
    Impostor,       // an entity's impostor card (IMPOSTORS.md): the entry's summary and atlases, Bake and Remove (ImpostorEditor)
    VehicleWheels,  // a vehicle's wheels, a JSON list of Unity WheelCollider-like objects, as a list with the selected wheel's fields (VehiclePartsEditor)
    VehicleSeats,   // a vehicle's seats, a JSON list, the same way
    TrackPoints     // a Track's control points with their bank and stop, as one JSON list over three properties (TrackPointsEditor)
};

struct PropertySpec {
    QString key;            // the property path, e.g. "keyLight.color"
    QString label;
    PropertyKind kind;
    QString section;
    double min { -DBL_MAX };
    double max { DBL_MAX };
    double step { 0.1 };
    int decimals { 2 };
    QString unit;
    QStringList enumValues; // "value:Label" entries
    // "key=value", "key!=value", "key in a,b,c", "key not in a,b,c", any of these joined by " and " and then by " or "
    // (no brackets: "a and b or c" is a and b, or c), or empty
    QString showWhen;
    bool readOnly { false };
    bool header { false };  // in the Inspector's header (Unity's): Visible beside the name, the other ticks under them
    bool headerIcon { false };  // shown as one of the header's toggle icons (the Hierarchy rows'), not as a field
};

struct PropertySection {
    QString id;
    QString label;
    bool minor { false };   // a sub-section (the particle groups)
};

namespace PropertySpecs {
    const QVector<PropertySpec>& all();
    QVector<const PropertySpec*> forSection(const QString& section);
    const PropertySpec* find(const QString& key);
    const PropertySection* section(const QString& id);
    // the sections an entity type shows, in order; `multiple` for a mixed selection
    QStringList sectionsForType(EntityTypes::EntityType type, bool multiple);

    // the property's value, as the kind's QVariant; invalid when the key is unknown
    QVariant get(const QString& key, const EntityItemProperties& properties);
    // sets the property (and only it) on the properties; false when the key is unknown
    bool set(const QString& key, EntityItemProperties& properties, const QVariant& value);

    // An entry of a JSON property shown as a field of its own has the key "<property>:<entry>": "userData:cookie" (a
    // light's Cookie), "materialData:smoothness" (a material's Smoothness). Its value is read from the property's
    // JSON, and set() writes it back into the JSON the properties already hold, so the entity's current value of the
    // property must be put there first; the JSON's other entries are kept. A field may also stand for part of an
    // entry: "materialData:surfaceType" and "materialData:alphaClipping" are two fields over opacityMapMode.
    bool isEntry(const QString& key);
    bool isUserDataEntry(const QString& key);
    // the entity's property can hold the key's entry: userData that is empty or a JSON object (not a script's own
    // text), materialData that is empty or a set of materials
    bool takesEntry(const QString& key, const EntityItemProperties& properties);
    // the properties an edit of the key changes, for undo: dimensions for Scale, the JSON property for an entry of
    // it, and for a light's Shadows (userData:shadows) also canCastShadow, which Off turns off
    QStringList storedKeys(const QString& key);

    // an entity's own size, what Scale 1 means: a model's natural dimensions (zero until the model
    // has loaded), 1 m for everything else, as for Unity's primitives
    glm::vec3 naturalSize(const EntityItemProperties& properties);
}

#endif // hifi_PropertySpec_h
