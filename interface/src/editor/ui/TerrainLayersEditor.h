//
//  TerrainLayersEditor.h
//  interface/src/editor/ui
//
//  The Layers section of a Terrain entity's Inspector: the terrain's material layers (terrain::LayerStack,
//  TERRAIN.md section 5) as Photoshop lists layers and Unity shows a terrain's layer palette. One row per layer,
//  the top of the stack at the top: an eye that shows or hides it, a swatch of its texture and its name. A row
//  dragged, or moved with the arrows, changes the order the layers draw in; + adds a layer from the material
//  library (interface/resources/terrain/materials/library.json) or an empty one, − removes the selected one (its
//  slot's paint and voxels stay, unseen, until a layer takes the slot again) and Reset brings back the eight
//  defaults. Below the list, the selected layer's own settings: name, material (the library, or any texture's
//  URL), its normal map (with an on/off and a strength), roughness, occlusion and height maps by URL, tiling and
//  offset in metres, opacity, blend mode, tint, and the plain colour drawn until the texture is in. The selected
//  row is also the layer the Paint tools paint. A picture dragged from the Assets panel (or a browser) onto a row
//  becomes that layer's colour texture, and onto one of the map fields that map. Every change is an undo step and is
//  saved with the terrain; the layers are not an entity property, so the editor reads and sets them through
//  EditorTerrain.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TerrainLayersEditor_h
#define hifi_TerrainLayersEditor_h

#include <functional>

#include <QtCore/QUuid>
#include <QtCore/QVector>
#include <QtGui/QPixmap>

#include <terrain/Layers.h>

#include "PropertyEditors.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QToolButton;
class EditorTerrain;

class TerrainLayersEditor : public PropertyEditor {
    Q_OBJECT
public:
    TerrainLayersEditor(const PropertySpec& spec, QWidget* parent = nullptr);
    // the section's header names it: the list takes the whole width, with no label beside it
    QWidget* createLabel(QWidget* parent) override;
    void setValue(const QVariant& value, bool mixed) override;   // nothing: the layers are not a property
    bool isEditing() const override;
    QVariant currentValue() const override { return QVariant(); }

    void setTerrainEditor(EditorTerrain* terrain) { _terrain = terrain; }
    // drops on the rows and on the map fields (pictures from the Assets panel or a browser)
    bool eventFilter(QObject* watched, QEvent* event) override;
    // the terrain whose layers to show, at every refresh of the Inspector; none, or several selected, shows a note
    void showTerrain(const QUuid& id, bool multiple);

signals:
    // the selected row: the layer the Paint tools paint
    void layerChosen(int slot);
    // the list was (re)built: the layers' names or order may have changed
    void layersShown();

private:
    struct LibraryEntry {
        QString name;
        QString category;    // the group it is listed under in the menus (library.json keeps a group's entries together)
        QString url;         // the colour texture
        QString normal;      // the maps; empty for none
        QString roughness;
        QString occlusion;
        QString height;
        float tiling { 4.0f };
        glm::vec3 color { 0.5f };
    };
    static const QVector<LibraryEntry>& library();

    int selectedIndex() const;   // in the stack, or -1
    terrain::Layer* selectedLayer();
    void rebuildList();
    // the selected row's widget gets a "selected" property for the stylesheet, so the row reads as chosen beyond the
    // list's own highlight, which its widgets cover
    void markSelectedRow();
    void showSelected();
    QWidget* makeRow(const terrain::Layer& layer);
    QPixmap swatchFor(const terrain::Layer& layer) const;
    void requestSwatch(const QString& url);
    void refreshSwatches();
    // the stack as shown goes to the terrain: a preview while a scrub goes on, or an undo step
    void preview();
    void commit(const QString& text, bool fromScrub = false);
    void beginScrub();
    void cancelScrub();
    void addLayer(int libraryIndex);   // -1: an empty layer
    void removeLayer();
    void moveLayer(int by);            // +1 up the list (drawn later), -1 down
    void dragReordered();
    void resetLayers();
    // the row a picture is being dragged over is marked for the stylesheet; -1 marks none
    void setDropRow(int row);

    // a field holding one of the selected layer's maps by URL: what a drop or an edit of it sets, and its undo text
    struct MapField {
        QLineEdit* edit { nullptr };
        QString undoText;
        std::function<void(terrain::Layer&, const std::string&)> apply;
    };
    QVector<MapField> _mapFields;
    int _dropRow { -1 };

    EditorTerrain* _terrain { nullptr };
    QUuid _id;
    uint64_t _shownVersion { 0 };
    terrain::LayerStack _stack;
    terrain::LayerStack _before;   // at the start of a scrub, for the step at its end
    int _selectedSlot { -1 };

    QLabel* _note { nullptr };
    QWidget* _body { nullptr };
    QListWidget* _list { nullptr };
    QToolButton* _addButton { nullptr };
    QToolButton* _removeButton { nullptr };
    QToolButton* _upButton { nullptr };
    QToolButton* _downButton { nullptr };
    QToolButton* _resetButton { nullptr };
    QWidget* _fields { nullptr };
    QLineEdit* _name { nullptr };
    QComboBox* _material { nullptr };
    QLineEdit* _url { nullptr };
    QLineEdit* _normalURL { nullptr };
    QCheckBox* _useNormal { nullptr };
    NumberEditor* _normalStrength { nullptr };
    QLineEdit* _roughnessURL { nullptr };
    QLineEdit* _occlusionURL { nullptr };
    QLineEdit* _heightURL { nullptr };
    NumberEditor* _tiling { nullptr };
    NumberEditor* _offsetX { nullptr };
    NumberEditor* _offsetZ { nullptr };
    NumberEditor* _opacity { nullptr };
    QComboBox* _blend { nullptr };
    ColorEditor* _tint { nullptr };
    ColorEditor* _color { nullptr };
    QCheckBox* _visible { nullptr };
    bool _menuOpen { false };
    bool _reordering { false };
};

#endif // hifi_TerrainLayersEditor_h
