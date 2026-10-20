//
//  VehiclePartsEditor.h
//  interface/src/editor/ui
//
//  A vehicle's wheels or seats in the Inspector (PHYSICS.md 4.8). The entity holds each as a JSON list of objects (a
//  wheel with Unity's WheelCollider names, a seat with a position and a yaw); this shows the list as rows of names with
//  the selected one's fields under them, the way Unity shows a WheelCollider, instead of as JSON text. Every edit
//  writes the whole list back through the property's usual path (editBegan / liveValue / committed with the JSON text),
//  so a scrub previews live, undo takes one step, and the simulator rebuilds the vehicle from the new list as it does
//  for any change. The selected row goes to the Editor, which draws that wheel or seat bright in the viewport
//  (EditorVehicleGizmos).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_VehiclePartsEditor_h
#define hifi_VehiclePartsEditor_h

#include <functional>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QVector>

#include "PropertyEditors.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;

class VehiclePartsEditor : public PropertyEditor {
    Q_OBJECT
public:
    // spec.kind is VehicleWheels or VehicleSeats
    VehiclePartsEditor(const PropertySpec& spec, QWidget* parent = nullptr);
    // the editor carries its own heading: nothing beside it
    QWidget* createLabel(QWidget* parent) override;
    void setValue(const QVariant& value, bool mixed) override;
    bool isEditing() const override;
    void revertTyping() override;
    // the list as JSON text
    QVariant currentValue() const override;
    bool isMixed() const override { return _mixed; }

signals:
    // the selected row's index in the list, -1 for none: the part the viewport's gizmo draws bright
    void partChosen(int index);

private:
    struct Field {
        QString key;                        // in the part's JSON object
        PropertyEditor* editor { nullptr };
        // the editor's value as the JSON holds it (undefined leaves the key out), given what the key held; and
        // the editor's value from the part, with the default for a key it lacks
        std::function<QJsonValue(const QVariant& value, const QJsonValue& was)> toJson;
        std::function<QVariant(const QJsonObject& part)> fromJson;
    };
    PropertySpec fieldSpec(const char* key, const char* label, PropertyKind kind, double min, double max, double step,
                           int decimals, const char* unit) const;
    void addField(const PropertySpec& spec, const char* toolTip,
                  std::function<QJsonValue(const QVariant&, const QJsonValue&)> toJson,
                  std::function<QVariant(const QJsonObject&)> fromJson);
    void addNumberField(const char* key, const char* label, PropertyKind kind, double min, double max, double step,
                        int decimals, const char* unit, double fallback, const char* toolTip);
    void addVec3Field(const char* key, const char* label, const char* toolTip);
    void addBoolField(const char* key, const char* label, bool fallback, const char* toolTip);
    void wireField(int index);
    bool applyField(int index, const QVariant& value);
    void rebuildList();
    void markSelectedRow();
    void showSelected();
    QWidget* makeRow(const QJsonObject& part, int index);
    QString nameOf(const QJsonObject& part, int index) const;
    QString summaryOf(const QJsonObject& part) const;
    QString json() const;
    void commitList();
    void addPart(bool mirrored);
    void removePart();

    bool _wheels { true };
    QJsonArray _items;
    QJsonArray _before;        // at the start of a scrub, for a cancel
    QString _shown;            // the JSON text as last shown, so a refresh that changes nothing leaves the list be
    int _selected { -1 };
    bool _mixed { false };
    bool _menuOpen { false };

    QLabel* _note { nullptr };
    QWidget* _body { nullptr };
    QListWidget* _list { nullptr };
    QToolButton* _addButton { nullptr };
    QToolButton* _removeButton { nullptr };
    QWidget* _fields { nullptr };
    QLineEdit* _name { nullptr };
    QVector<Field> _fieldList;
};

#endif // hifi_VehiclePartsEditor_h
