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
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtGui/QPixmap>
#include <QtWidgets/QDockWidget>

#include <EntityItemProperties.h>
#include <EntityTypes.h>

#include "HierarchyPanel.h"

class QAbstractButton;
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
    // whether this player may lock and unlock here (the world's Lock / Unlock right): without it Locked greys out and
    // says why, as the world refuses the change with nothing on the screen
    void setCanLock(bool canLock, const QString& why);
    // what the selection could not load (its model, image or textures, with the reasons), in red under the title, and
    // the title red too; empty hides it (Caity, 10-24: an Image that stayed blank and said nothing)
    void setLoadProblem(const QString& problem);
    // everything shown, whatever is scrolled into view (a scripted test's picture)
    QPixmap contentPicture() const;
    // the header's four toggle icons for the selection (HierarchyPanel::statesFor): hidden in this view, clicks pass
    // through here, locked, scripts off
    void setToggleStates(const QVector<HierarchyPanel::ToggleState>& states);
    // the Terrain tool, which a Terrain's Layers section reads and sets the layers through (they live in the
    // terrain file on this client, not in a property); set once, before any terrain is shown
    void setTerrainEditor(EditorTerrain* terrain) { _terrainEditor = terrain; }

signals:
    // a header toggle icon clicked, for the selection (a HierarchyPanel::Toggle)
    void toggleClicked(int toggle);
    // a Terrain's Layers section selected a layer: the one the Paint tools paint
    void terrainLayerChosen(int slot);
    // a Terrain's Layers section showed its list again: names or order may have changed
    void terrainLayersShown();
    // a vehicle's Wheels or Seats list selected a row (-1 none): the part the viewport draws bright
    void vehiclePartChosen(const QString& key, int index);
    // a Track's Points list selected a row (-1 none): the point the viewport draws bright and the Move gizmo stands on
    void trackPointChosen(int index);
    void editBegan(const QString& key);
    void liveEdit(const QString& key, const QVariant& value);
    void committed(const QString& key, const QVariant& value);
    void editCancelled(const QString& key);
    // a picture on this computer chosen for a field (a light's Cookie): to be uploaded, its URL then set
    void uploadRequested(const QString& key, const QString& localPath);

public slots:
    // the viewport chose a track point (a click on its diamond): the Points list follows
    void chooseTrackPoint(int index);

protected:
    // the content follows the scroll area's width exactly: rows compress rather than run off the edge
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        const PropertySpec* spec { nullptr };
        QWidget* label { nullptr };   // none in the header: a tick's name is beside its box
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
    bool _canLock { true };
    QString _noLockWhy;
    bool _allLocked { false };   // everything selected is locked: only the lock can change
    QVector<HierarchyPanel::ToggleState> _toggleStates;
    QList<QAbstractButton*> _toggleIcons;   // in the header, made with it
    QScrollArea* _scroll { nullptr };
    QWidget* _empty { nullptr };
    QLabel* _emptyLabel { nullptr };
    QWidget* _content { nullptr };
    QVBoxLayout* _contentLayout { nullptr };
    QLabel* _title { nullptr };
    QLabel* _loadProblem { nullptr };
    QPointer<QLabel> _lockNote;   // in the header, made with it
    QString _layoutKey;
    QHash<QString, Row> _rows;
    QHash<QString, QWidget*> _sections;     // each section's header and body, by id
    QHash<QString, bool> _collapsed;
    EditorTerrain* _terrainEditor { nullptr };
};

#endif // hifi_InspectorPanel_h
