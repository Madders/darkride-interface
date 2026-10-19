//
//  InspectorPanel.h
//  interface/src/editor/ui
//
//  The Inspector dock: the selected entities' properties in sections, generated from the
//  property table. Multi-selection shows "—" where values differ and applies an edit to all.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_InspectorPanel_h
#define hifi_InspectorPanel_h

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtWidgets/QDockWidget>

#include <EntityItemProperties.h>
#include <EntityTypes.h>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QWidget;
class EditorTerrain;
class PropertyEditor;
struct PropertySpec;

class InspectorPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(QWidget* parent = nullptr);

    // nothing selected: a note instead of fields
    void setSummary(const QString& text);
    // the selection: one snapshot per entity, the common type (or multiple), and the title line
    void showEntities(const QVector<EntityItemProperties>& snapshots, EntityTypes::EntityType type, bool multiple,
        const QString& title);
    // fresh values for the same selection; fields being edited are left alone
    void refreshValues(const QVector<EntityItemProperties>& snapshots);
    // the world cannot be edited: fields greyed out, with a note at the top saying why
    void setReadOnly(bool readOnly, const QString& reason);
    // the Terrain tool, which a Terrain's Layers section reads and sets the layers through (they live in the
    // terrain file on this client, not in a property); set once, before any terrain is shown
    void setTerrainEditor(EditorTerrain* terrain) { _terrainEditor = terrain; }

signals:
    // a Terrain's Layers section selected a layer: the one the Paint tools paint
    void terrainLayerChosen(int slot);
    // a Terrain's Layers section showed its list again: names or order may have changed
    void terrainLayersShown();
    void editBegan(const QString& key);
    void liveEdit(const QString& key, const QVariant& value);
    void committed(const QString& key, const QVariant& value);
    void editCancelled(const QString& key);
    // a picture on this computer chosen for a field (a light's Cookie): to be uploaded, its URL then set
    void uploadRequested(const QString& key, const QString& localPath);

protected:
    // the content follows the scroll area's width exactly: rows compress rather than run off the edge
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        const PropertySpec* spec { nullptr };
        QWidget* label { nullptr };
        PropertyEditor* editor { nullptr };
    };

    void rebuild(const QStringList& sections);
    void applyValues(const QVector<EntityItemProperties>& snapshots, bool force);
    void applyVisibility(const EntityItemProperties& first);
    bool conditionHolds(const QString& showWhen, const EntityItemProperties& first) const;
    QWidget* buildSection(const QString& sectionId, QWidget* parent);

    void applyReadOnly();

    QLabel* _banner { nullptr };
    bool _readOnly { false };
    QScrollArea* _scroll { nullptr };
    QWidget* _empty { nullptr };
    QLabel* _emptyLabel { nullptr };
    QWidget* _content { nullptr };
    QVBoxLayout* _contentLayout { nullptr };
    QLabel* _title { nullptr };
    QString _layoutKey;
    QHash<QString, Row> _rows;
    QHash<QString, QWidget*> _sections;     // each section's header and body, by id
    QHash<QString, bool> _collapsed;
    EditorTerrain* _terrainEditor { nullptr };
};

#endif // hifi_InspectorPanel_h
