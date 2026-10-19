//
//  TerrainCapsEditor.h
//  interface/src/editor/ui
//
//  The Inspector's Closed Box row of a Terrain: a checkbox that closes every face of the terrain's box (the ground
//  is capped there: a raise that reaches the top has a top, the box's edges show walls) or opens them again (the
//  ground runs out of the box open, as the edges of terrains laid side by side want). Not an entity property: the
//  setting lives in the terrain file with the grid, read and set through the Terrain tool (EditorTerrain::capsOf,
//  setCaps), one undo step a change, saved with the terrain.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TerrainCapsEditor_h
#define hifi_TerrainCapsEditor_h

#include <QtCore/QUuid>

#include "PropertyEditors.h"

class QCheckBox;
class EditorTerrain;

class TerrainCapsEditor : public PropertyEditor {
    Q_OBJECT
public:
    TerrainCapsEditor(const PropertySpec& spec, QWidget* parent = nullptr);
    void setValue(const QVariant& value, bool mixed) override;   // nothing: not a property
    QVariant currentValue() const override { return QVariant(); }

    void setTerrainEditor(EditorTerrain* terrain) { _terrain = terrain; }
    // the terrain whose box to show, at every refresh of the Inspector; none, or several selected, disables the box
    void showTerrain(const QUuid& id, bool multiple);

private:
    EditorTerrain* _terrain { nullptr };
    QCheckBox* _check { nullptr };
    QUuid _id;
};

#endif // hifi_TerrainCapsEditor_h
