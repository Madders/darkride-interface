//
//  ImpostorEditor.h
//  interface/src/editor/ui
//
//  The Impostor section of an entity's Inspector (IMPOSTORS.md): what the entity's replacement card is, if it has
//  one (when it was baked, the frames' size, the bounding sphere's radius, the three atlases as thumbnails: albedo,
//  normal and depth, roughness and metallic), what the view shows of it now (the card, the model, or the two
//  dissolving, and how big the sphere looks from the camera against the switch angle), a size to bake at, Bake (the
//  ImpostorBaker, as Build > Bake Impostor) and Remove (the entry taken off userData, one undo step), the switch angle
//  every card shares (LODManager.impostorAngleDeg; 0 is automatic, four times the LOD angle) and Tint Cards (every
//  card drawn tinted blue, to tell it from its model; not saved). The row's value is the entry's JSON, read and
//  written through the "userData:impostor" property (PropertySpec).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ImpostorEditor_h
#define hifi_ImpostorEditor_h

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QUuid>

#include "PropertyEditors.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QTimer;
class QNetworkReply;
class QPushButton;

class ImpostorEditor : public PropertyEditor {
    Q_OBJECT
public:
    ImpostorEditor(const PropertySpec& spec, QWidget* parent = nullptr);
    // the section's header names it: the editor takes the whole width, with no label beside it
    QWidget* createLabel(QWidget* parent) override;
    void setValue(const QVariant& value, bool mixed) override;
    QVariant currentValue() const override { return QVariant(_entryText); }
    bool isMixed() const override { return _mixed; }
    // the entities shown, which Bake bakes
    void setEntities(const QList<QUuid>& ids) { _ids = ids; }

private:
    struct Atlas {
        QLabel* view { nullptr };
        QPointer<QNetworkReply> reply;
        QString shown;
    };
    void bake();
    void remove();
    void showAtlas(Atlas& atlas, const QString& url);
    // the line saying what the view shows of the entity now, from its card's switch
    void showState();

    QLabel* _summary { nullptr };
    QLabel* _state { nullptr };
    QTimer* _stateTimer { nullptr };
    QCheckBox* _tint { nullptr };
    Atlas _albedo;
    Atlas _normal;
    Atlas _material;
    QComboBox* _size { nullptr };
    QPushButton* _bake { nullptr };
    QPushButton* _remove { nullptr };
    QDoubleSpinBox* _angle { nullptr };
    QString _entryText;
    bool _mixed { false };
    QList<QUuid> _ids;
    QSet<QUuid> _baking;
};

#endif // hifi_ImpostorEditor_h
