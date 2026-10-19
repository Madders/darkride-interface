//
//  TerrainToolBar.h
//  interface/src/editor/ui
//
//  The second row of the build toolbar while the Terrain tool is active: the sculpt modes (Raise, Lower, Smooth,
//  Flatten, Dig, Fill), the two paint modes (Paint, for the paint map, and Paint Voxels, for the voxels' own
//  materials) and the brush's size, strength, hardness, shape (round, or a picture, turned by the angle) and
//  layer, as Unity's terrain inspector has them;
//  Resample, which remakes a terrain's voxels at its voxel size after it was scaled or its voxel size changed; and
//  Detail, how far out every terrain is drawn at full detail (TERRAIN.md section 4, level of detail).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TerrainToolBar_h
#define hifi_TerrainToolBar_h

#include <functional>

#include <QtCore/QStringList>
#include <QtWidgets/QToolBar>

#include "../EditorTerrain.h"

class QAbstractSpinBox;
class QAction;
class QActionGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QMenu;
class QToolButton;
class ScrubLabel;

class TerrainToolBar : public QToolBar {
    Q_OBJECT
public:
    explicit TerrainToolBar(QWidget* parent = nullptr);

    EditorTerrainBrush brush() const;
    void setMode(terrain::SculptMode mode);
    void setRadius(float metres);
    // the layer (slot 0 to 7) Paint and Paint Voxels paint: the Inspector's Layers section sets it as a layer is selected
    void setLayer(int slot);
    // where the Layer field's names come from (eight, by slot; empty for an unused slot): asked whenever the field
    // is refreshed, which refreshLayerNames() does and the field's own popup does
    void setLayerNameSource(std::function<QStringList()> source);

public slots:
    void refreshLayerNames();

signals:
    void brushChanged();
    // the selected terrain (or the one under the pointer) should be resampled at its Voxel Size
    void resampleRequested();

private:
    // erase: the Paint mode's eraser, a mode of its own on the bar
    QAction* addMode(const QString& text, const QString& toolTip, terrain::SculptMode mode, bool erase = false);
    // the Erase box serves Paint Voxels alone (Paint has its own Erase mode): enabled with that mode
    void updateEraseVoxels();
    // The brush's shape: the round brush (an empty path) or a picture (terrain::BrushMask) from the client's
    // brushes (interface/resources/terrain/brushes) or a file; the thumbnail is the picture, or round.png.
    QAction* addShape(const QString& name, const QString& path, const QString& thumbnail);
    void chooseShape(const QString& name, const QString& path, const QString& thumbnail);
    void loadShapePicture();

    QActionGroup* _modes { nullptr };
    QCheckBox* _eraseVoxels { nullptr };
    QToolButton* _shape { nullptr };
    QMenu* _shapeMenu { nullptr };
    QAction* _loadShape { nullptr };   // the menu's last entry: pictures loaded from disk go before it
    QString _shapePath;
    // The field's label scrubs its value when dragged sideways, as the Inspector's do: step per pixel, Shift ten
    // times that, Alt a tenth; proportional makes the step a share of the current value, for a size.
    void attachScrub(ScrubLabel* label, QAbstractSpinBox* box, double step, bool proportional);

    QDoubleSpinBox* _size { nullptr };
    QDoubleSpinBox* _strength { nullptr };
    QDoubleSpinBox* _hardness { nullptr };
    QDoubleSpinBox* _angle { nullptr };
    QComboBox* _layer { nullptr };
    std::function<QStringList()> _layerNames;
    QDoubleSpinBox* _detail { nullptr };
};

#endif // hifi_TerrainToolBar_h
