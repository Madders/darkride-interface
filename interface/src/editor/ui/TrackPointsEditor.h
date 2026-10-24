//
//  TrackPointsEditor.h
//  interface/src/editor/ui
//
//  A Track's control points in the Inspector (PHYSICS.md 4.8): the list, with the selected point's position (in the
//  track's own metres), bank and stop, and buttons to add a point after it or remove it. The value is the three
//  properties (trackPoints, trackBank, trackDwell) as one JSON list, [{"x","y","z","bank","dwell"}, ...], which the
//  spec's accessor splits again. Choosing a row chooses the point in the viewport too (bright, with the Move gizmo
//  on it), and a click on a point's diamond there chooses the row here. Modelled on VehiclePartsEditor.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TrackPointsEditor_h
#define hifi_TrackPointsEditor_h

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVector>

#include "PropertyEditors.h"

class QLabel;
class QListWidget;
class QToolButton;

class TrackPointsEditor : public PropertyEditor {
    Q_OBJECT
public:
    TrackPointsEditor(const PropertySpec& spec, QWidget* parent = nullptr);
    // the editor carries its own heading: nothing beside it
    QWidget* createLabel(QWidget* parent) override;
    void setValue(const QVariant& value, bool mixed) override;
    bool isEditing() const override;
    void revertTyping() override;
    QVariant currentValue() const override;
    bool isMixed() const override { return _mixed; }

    // the viewport chose a point (a click on its diamond): the row follows, without a pointChosen back
    void choose(int index);
    // another selection is about to be shown: no row is chosen in it (a row kept from the track shown before was
    // chosen in the next one, so Delete removed its point instead of deleting it, 10-21 review)
    void forgetChoice();
    int chosen() const { return _selected; }

signals:
    // the selected row's index, -1 for none: the point the viewport draws bright and the Move gizmo stands on
    void pointChosen(int index);

private:
    void rebuildList();
    void markSelectedRow();
    void showSelected();
    QWidget* makeRow(const QJsonObject& point, int index);
    QString summaryOf(const QJsonObject& point) const;
    QString json() const;
    void commitList();
    void addPoint();
    void removePoint();
    bool applyField(int field, const QVariant& value);
    void wireField(int field, PropertyEditor* editor);
    void refreshHeading();

    QJsonArray _items;
    QJsonArray _before;        // at the start of a scrub, for a cancel
    QString _shown;            // the JSON text as last shown, so a refresh that changes nothing leaves the list be
    int _selected { -1 };
    bool _mixed { false };
    bool _closed { false };    // whether the track loops, for the heading's length and the add button's place

    QLabel* _heading { nullptr };
    QLabel* _note { nullptr };
    QWidget* _body { nullptr };
    QListWidget* _list { nullptr };
    QToolButton* _addButton { nullptr };
    QToolButton* _removeButton { nullptr };
    QWidget* _fields { nullptr };
    PropertyEditor* _position { nullptr };
    PropertyEditor* _bank { nullptr };
    PropertyEditor* _dwell { nullptr };
};

#endif // hifi_TrackPointsEditor_h
