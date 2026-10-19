//
//  TerrainCapsEditor.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TerrainCapsEditor.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <terrain/VoxelGrid.h>

#include "../EditorTerrain.h"

TerrainCapsEditor::TerrainCapsEditor(const PropertySpec& spec, QWidget* parent) : PropertyEditor(spec, parent) {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    _check = new QCheckBox(this);
    _check->setToolTip("Close every face of the terrain's box: the ground is capped there, so a raise that reaches the top "
                       "has a top and the box's edges show walls. Open, the ground runs out of the box, as the edges of "
                       "terrains laid side by side want. Kept in the terrain file.");
    _check->setEnabled(false);
    layout->addWidget(_check);
    layout->addStretch(1);
    connect(_check, &QCheckBox::clicked, this, [this](bool checked) {
        if (_updating || !_terrain || _id.isNull()) {
            return;
        }
        if (!_terrain->setCaps(_id, checked ? terrain::CAP_ALL : terrain::CAP_NONE,
                               checked ? QStringLiteral("Close Terrain Box") : QStringLiteral("Open Terrain Box"))) {
            showTerrain(_id, false);   // refused (locked): the box shows what is
        }
    });
}

void TerrainCapsEditor::setValue(const QVariant& value, bool mixed) {
    Q_UNUSED(value);
    Q_UNUSED(mixed);
}

void TerrainCapsEditor::showTerrain(const QUuid& id, bool multiple) {
    const int caps = (!multiple && !id.isNull() && _terrain) ? _terrain->capsOf(id) : -1;
    _updating = true;
    if (caps < 0) {
        _id = QUuid();
        _check->setEnabled(false);
        _check->setChecked(false);
    } else {
        _id = id;
        _check->setEnabled(true);
        _check->setChecked(caps != terrain::CAP_NONE);
    }
    _updating = false;
}
