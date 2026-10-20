//
//  Editor.h
//  interface/src/editor
//
//  Build mode: the native scene editor. Owns the scene-view camera, the grid, the selection,
//  the undo stack, the gizmo and the panels docked around the viewport; Application hands it
//  the viewport's input while it is active.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Editor_h
#define hifi_Editor_h

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtGui/QVector3D>

#include <glm/gtc/quaternion.hpp>

#include <AABox.h>
#include <DependencyManager.h>
#include <EntityItemProperties.h>
#include <RegisteredMetaTypes.h>
#include <SettingHandle.h>

#include "EditorBounds.h"
#include "EditorCamera.h"
#include "EditorClipboard.h"
#include <QtGui/QPixmap>

#include "EditorGizmo.h"
#include "EditorLightGizmos.h"
#include "EditorVehicleGizmos.h"
#include "EditorGrid.h"
#include "EditorSelection.h"
#include "EditorTerrain.h"
#include "EditorUndo.h"
#include "ui/EditorToolBar.h"

class QAction;
class QDropEvent;
class QKeyEvent;
class QLabel;
class QMenu;
class QMouseEvent;
class QStatusBar;
class QUrl;
class QWheelEvent;
class AssetsPanel;
class EditorUpload;
class EditorWorld;
class EntityTreeModel;
class HierarchyPanel;
class InspectorPanel;
class TerrainToolBar;
class UserFiles;

class Editor : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY gridVisibleChanged)
    Q_PROPERTY(float flySpeed READ flySpeed NOTIFY cameraChanged)
    // the world axes in camera space (x right, y up, z towards the viewer), for the scene gizmo
    Q_PROPERTY(QVector3D axisX READ axisX NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D axisY READ axisY NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D axisZ READ axisZ NOTIFY cameraChanged)
    // the selection rectangle being dragged in the viewport, in viewport pixels
    Q_PROPERTY(bool marqueeActive READ isMarqueeActive NOTIFY marqueeChanged)
    Q_PROPERTY(QRectF marquee READ marquee NOTIFY marqueeChanged)

public:
    Editor();

    bool isActive() const { return _active; }
    bool isGridVisible() const { return _gridVisible; }
    float flySpeed() const { return _camera.flySpeed(); }
    QVector3D axisX() const { return _axes[0]; }
    QVector3D axisY() const { return _axes[1]; }
    QVector3D axisZ() const { return _axes[2]; }
    bool isMarqueeActive() const { return _marqueeActive; }
    QRectF marquee() const { return _marqueeRect; }

    EditorCamera& camera() { return _camera; }
    EditorGrid& grid() { return _grid; }
    EditorSelection& selection() { return _selection; }
    EditorUndo& undoStack() { return _undo; }

    // Application calls this once the window, the scene and the desktop UI exist. Until then build
    // mode cannot be entered: the menu restores its saved state at startup by triggering the item.
    void setReady();

    // called by Application every frame while active, before the camera is read
    void update(float deltaTime);

    // viewport input; each returns true when the editor consumed the event
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleMouseDoubleClick(QMouseEvent* event);
    bool handleWheel(QWheelEvent* event);
    bool handleKeyPress(QKeyEvent* event);
    bool handleKeyRelease(QKeyEvent* event);
    // the editor's keys when the focus is in one of its panels rather than the viewport
    bool eventFilter(QObject* watched, QEvent* event) override;

    // a Create menu with every entity kind, for the toolbar and context menus; with a viewport
    // position, new entities go where that position points instead of the middle of the view
    QMenu* buildCreateMenu(QWidget* parent, const QPoint* viewportPos = nullptr);
    // a file or URL dropped on the viewport: models and images are placed where it landed
    bool handleDrop(QDropEvent* event);
    // a Build menu item that only works while build mode is on
    void addMenuAction(QAction* action);

    Q_INVOKABLE void viewFromAxis(int axis, int sign);

    // the whole Inspector as drawn, scrolled or not (a scripted test's picture of it, Test.saveInspectorPicture)
    QPixmap inspectorPicture() const;

public slots:
    void setActive(bool active);
    void toggleActive() { setActive(!_active); }
    // selects these entities, turning build mode on first (a scripted test, Test.selectInEditor)
    void select(const QList<QUuid>& ids);
    void setGridVisible(bool visible);
    void setTool(EditorToolBar::Tool tool);
    void setLocalSpace(bool local);
    void setPivotMode(bool pivot);

    void undo();
    void redo();
    // docks the panels back where they started, for a panel that was torn off and lost
    void resetLayout();
    void frameSelection();
    void goToSelection();           // Shift+F: frame it and take the avatar there
    void bringSelectionHere();      // Option+Shift+F: move it to the avatar
    void create(const QString& key);
    void createModel();             // asks for a URL
    void createFromMenu();          // the sender's data() is the catalogue key
    void deleteSelection();
    void duplicateSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void selectAll();
    // Build > Use Stable Links: every link in the world to one of the user's own files by its path becomes the file's
    // stable link, read from a fresh listing of My Files; one undo step, and a report of what could not be matched
    // (NATIVE-EDITOR.md section 7.4)
    void useStableLinks();
    // Build > Bake Impostor: every selected entity without a selected ancestor, one after the other (ImpostorBaker)
    void bakeImpostor();
    // Build > Bake All Impostors: every prop of the world without a card yet (ImpostorBaker::bakeAll), the progress
    // on the status bar; Build > Remove Impostor: the selected entities' entries
    void bakeAllImpostors();
    void removeImpostor();

signals:
    void activeChanged(bool active);
    void gridVisibleChanged(bool visible);
    void cameraChanged();
    void marqueeChanged();

private:
    enum class DragKind { None, Move, Rotate, Scale };

    void createPanels();
    void placeToolBars();
    void showPanels(bool show);
    void focusViewport();
    void takeFocusUnlessTyping();
    void setMenuActionsEnabled(bool enabled);
    void updateAxes();
    void updateGizmo();
    // the tool has changed: the gizmo, the Terrain tool's row and its outline follow
    void applyTool();
    void updateTerrainBar();
    void updateRenderMethod();
    void onSelectionChanged();
    // the selection has changed by the user's hand: a step in the history, as in Unity
    void recordSelection();
    void rename(const QUuid& id, const QString& name);
    // makes the entities children of the parent (none: top-level), where they are in the world staying so
    void reparent(const QList<QUuid>& ids, const QUuid& parentId);

    // the Inspector: snapshots of the selection, and edits coming back from its fields
    void refreshInspector(bool selectionChanged);
    bool selectionSnapshots(QVector<EntityItemProperties>& snapshots, EntityTypes::EntityType& type, bool& multiple) const;
    void onInspectorEditBegan(const QString& key);
    void onInspectorLiveEdit(const QString& key, const QVariant& value);
    void applyLiveEdit(const QString& key, QVariant value);   // a copy: the paced send passes _liveEditValue, which this clears
    void onInspectorCommitted(const QString& key, const QVariant& value);
    // an Inspector value for the given entities, as one undo step
    void commitInspectorEdit(const QString& key, const QVariant& value, const QList<QUuid>& targets);
    void onInspectorEditCancelled(const QString& key);
    // a picture chosen for an Inspector field (a light's Cookie): uploaded, then set on the entities selected now
    void onInspectorUpload(const QString& key, const QString& localPath);
    // the edit an Inspector value amounts to for one entity: Scale is turned into dimensions, an entry of
    // userData is put into the userData the entity had (`before`, which holds it)
    EntityItemProperties inspectorEdit(const QString& key, const QVariant& value, const glm::vec3& naturalSize,
                                       const EntityItemProperties& before) const;

    PickRay pickRay(const QPoint& viewportPos) const;
    QUuid pickEntity(const QPoint& viewportPos) const;
    // where the cursor points in the world: a surface, or the grid plane; false when off the viewport
    bool cursorWorldPoint(glm::vec3& point) const;
    glm::vec3 selectionAnchor() const;
    // the last selected entity's world transform; false when it is not in the tree
    bool primaryTransform(glm::vec3& position, glm::quat& rotation) const;
    glm::vec3 gizmoPosition() const;
    glm::quat gizmoRotation() const;
    glm::vec3 viewDirection() const;

    void clickSelect(const QPoint& viewportPos, Qt::KeyboardModifiers modifiers);
    void updateMarquee(const QPoint& viewportPos);
    void finishMarquee(Qt::KeyboardModifiers modifiers);
    QList<QUuid> entitiesInRect(const QRectF& rect) const;

    bool snapActive() const;
    void beginGizmoDrag(GizmoHandle handle, const PickRay& ray);
    void moveGizmoDrag(const QPoint& viewportPos);
    // V held while moving: the vertex of another object's mesh under the cursor, or its surface
    bool vertexSnapTarget(const QPoint& viewportPos, glm::vec3& target) const;
    void applyDrag(bool final);

    // creating
    glm::vec3 placementPoint(const QPoint* viewportPos = nullptr) const;
    float cameraYaw() const;
    void createAt(const QString& key, const glm::vec3& point);
    void createModelAt(const glm::vec3& point);
    void pasteAt(const glm::vec3& target);
    QList<QUuid> place(EntityItemProperties properties, const QString& text, const glm::vec3& point);
    void placeModel(const QUrl& url, const glm::vec3& point);
    void placeImage(const QUrl& url, const glm::vec3& point);
    void beginUpload(const QString& localPath, const glm::vec3& point, bool image);
    // the toolbar follows an upload: its progress, and why it failed
    void showUploadProgress(EditorUpload* upload);
    void pollModelDimensions();
    void showHierarchyMenu(const QPoint& globalPos);
    // Use Stable Links, once the listing is in
    void rewriteStableLinks(const UserFiles& files);
    void showViewportMenu(const QPoint& viewportPos, const QPoint& globalPos);
    void addSelectionActions(QMenu& menu, const glm::vec3* pasteTarget);
    // a note in the middle of the toolbar; one with a timeout clears itself
    void setStatus(const QString& text, int clearAfterMs = 0);
    // The QML overlay (chat, a dialog, a web view with a field) has the keyboard: the editor leaves its keys alone.
    bool qmlHasTextFocus() const;

    // read-only: the domain gives no building rights (an instance before World Edit is on)
    void updateReadOnly();
    QString readOnlyReason() const;
    // true when the world can be changed; otherwise says why on the toolbar and returns false
    bool editable();

    // wireframes for selected zones, lights and particle systems
    void updateBounds();

    // the lights' icons, and the handles of a light selected on its own (see EditorLightGizmos.h)
    void updateLightGizmos();
    // a selected vehicle's wheels, seats and force points, the part chosen in the Inspector bright (EditorVehicleGizmos.h)
    void updateVehicleGizmos();
    void setGizmosVisible(bool visible);
    // what a click there selects: a light's icon, or else the surface under it
    QUuid pickAt(const QPoint& viewportPos) const;
    bool pointerOverHierarchy() const;
    void beginLightHandleDrag(const EditorLightHandle& handle, const PickRay& ray);
    void moveLightHandleDrag(const QPoint& viewportPos);
    void endLightHandleDrag(bool cancel);
    // the value a light handle's drag asks for, as the Inspector shows it: a range in metres, an angle in degrees
    float lightHandleValue(const PickRay& ray) const;

    bool _ready { false };
    bool _active { false };
    bool _gridVisible { true };
    EditorCamera _camera;
    EditorGrid _grid;
    EditorSelection _selection;
    EditorUndo _undo;
    QList<QUuid> _selectionWas;     // the selection before its latest change, for the history
    int _historyDepth { 0 };        // undo or redo is running: what the selection does then is not a new step
    int _selectionBatch { 0 };      // a gesture changing the selection in several steps (a marquee): one step at its end
    EditorClipboard _clipboard;
    EditorGizmo _gizmo;
    EditorToolBar::Tool _tool { EditorToolBar::Tool::Move };
    bool _localSpace { false };
    bool _pivotMode { false };
    bool _snapAlways { false };
    bool _vertexSnap { false };         // V is held
    float _moveSnap { 0.25f };          // metres
    float _rotateSnap { 15.0f };        // degrees
    float _scaleSnap { 0.1f };          // of the scale factor

    QVector3D _axes[3];
    glm::quat _axesOrientation;
    bool _axesValid { false };
    int _frameCounter { 0 };

    // a click or a marquee in the viewport
    bool _clickPending { false };
    QPoint _pressPos;
    Qt::KeyboardModifiers _pressModifiers;
    bool _marqueeActive { false };
    QRectF _marqueeRect;

    // a gizmo drag: the top-level selected entities and where they started
    DragKind _dragKind { DragKind::None };
    QList<QUuid> _dragIds;
    QVector<glm::vec3> _dragPositions;
    QVector<glm::quat> _dragRotations;
    QVector<glm::vec3> _dragDimensions;
    QVector<EntityItemProperties> _dragBefore;
    glm::vec3 _dragPivot;
    glm::quat _dragGizmoRotation;
    glm::vec3 _dragDelta;
    glm::quat _dragRotationDelta;
    glm::vec3 _dragScale { 1.0f };
    QPoint _lastDragPos;
    QElapsedTimer _editTimer;

    // an edit in progress in the Inspector: the ids and the values before, for undo
    QTimer _inspectorRefresh;
    // a scrub in the Inspector sends its edits no faster than a gizmo drag does: the latest value waits here
    QTimer _liveEditTimer;
    QElapsedTimer _liveEditClock;
    QVariant _liveEditValue;
    QString _inspectorEditKey;
    QList<QUuid> _inspectorEditIds;
    QVector<EntityItemProperties> _inspectorBefore;

    // a teleport puts the camera back on the avatar: what Shift+F framed is framed again
    bool _reframePending { false };
    AABox _reframeBox;
    int _reframeFrames { 0 };
    // a new model is a 1 m box until it loads and reports its size
    QTimer _modelDimensionsTimer;
    // a new model, until it has taken its own size and kept it
    struct AwaitedModel {
        int polls { 0 };
        quint64 createdAt { 0 };   // usecTimestampNow()
        quint64 sizedAt { 0 };     // when its own size was last applied; 0 until then
        int sizings { 0 };
    };
    QHash<QUuid, AwaitedModel> _modelsAwaitingDimensions;
    QList<QPointer<QAction>> _menuActions;
    int _uploadsActive { 0 };
    EditorWorld* _world { nullptr };
    void updateWorldControls();
    bool _readOnly { false };
    QTimer _statusTimer;
    QPoint _rightPressPos;
    EditorBounds _bounds;
    std::vector<EditorBoundsShape> _boundsShapes;
    EditorLightGizmos _lightGizmos;
    EditorVehicleGizmos _vehicleGizmos;
    std::vector<EditorVehicleGizmoVehicle> _vehicleGizmoVehicles;
    QHash<QString, int> _vehiclePartChosen;   // the row selected in each of the Inspector's vehicle lists, by key ("vehicle.wheels")
    EditorTerrain _terrain;             // the Terrain tool: its brushes, strokes and outline
    bool _stableLinksPending { false };  // Use Stable Links waits for the listing of My Files
    bool _gizmosVisible { true };       // every light's icon, not only the selected ones'
    QVector<QUuid> _nearbyLights;       // the lights round the camera, looked for every so often
    int _lightSearchFrame { -1 };       // when they were last looked for; -1 to look at once
    // a light handle's drag: an Inspector edit of the handle's property, from the viewport
    EditorLightHandle _lightHandleDrag;
    QString _lightHandleKey;
    float _lightHandleOffset { 0.0f };  // where on the handle it was taken, so it doesn't jump to the pointer
    float _lightHandleValue { 0.0f };
    bool _lightHandleMoved { false };
    ulong _lastEscapeTime { 0 };        // the key event time of a first Escape, waiting for a second
    // per entity in an Inspector edit of Scale: its own size, to turn scale into dimensions
    QVector<glm::vec3> _inspectorNatural;

    bool _panelsCreated { false };
    bool _syncingSelection { false };
    HierarchyPanel* _hierarchy { nullptr };
    InspectorPanel* _inspector { nullptr };
    AssetsPanel* _assets { nullptr };     // My Files, along the bottom
    EditorToolBar* _toolBar { nullptr };
    TerrainToolBar* _terrainBar { nullptr };
    int _impostorBakesQueued { 0 };       // Bake All's count, for the status bar
    int _impostorBakesDone { 0 };
    bool _impostorBakerConnected { false };
    QStatusBar* _statusBar { nullptr };   // the bottom of the window, in build mode: notes go there
    QLabel* _statusLabel { nullptr };
    EntityTreeModel* _treeModel { nullptr };
    Setting::Handle<QByteArray> _layoutSetting { "editor/layout3", QByteArray() };   // 2: the panels no longer float; 3: the Assets dock
    // build mode narrows the field of view; the user's own is kept here so it survives a crash
    Setting::Handle<float> _fovBeforeBuild { "editor/fovBeforeBuild", 0.0f };
    float _savedFov { 0.0f };
};

#endif // hifi_Editor_h
