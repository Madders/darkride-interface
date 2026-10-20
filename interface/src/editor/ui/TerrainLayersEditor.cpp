//
//  TerrainLayersEditor.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TerrainLayersEditor.h"

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <NetworkAccessManager.h>

#include "../EditorTerrain.h"

namespace {
    const int SWATCH_PIXELS = 22;
    const int ROW_PIXELS = 26;
    const char* LIBRARY_PATH = ":/terrain/materials/library.json";
    // the material menu's item data besides a library index
    const int MATERIAL_HEADING = -1;
    const int MATERIAL_CUSTOM = -2;
    const QStringList BLEND_NAMES = { "Normal", "Multiply", "Screen", "Overlay", "Height" };

    // the swatches by URL, shared by every Layers section there has been
    QHash<QString, QPixmap>& swatches() {
        static QHash<QString, QPixmap> cache;
        return cache;
    }

    QColor toQColor(const glm::vec3& color) {
        return QColor::fromRgbF(glm::clamp(color.r, 0.0f, 1.0f), glm::clamp(color.g, 0.0f, 1.0f), glm::clamp(color.b, 0.0f, 1.0f));
    }

    glm::vec3 fromQColor(const QColor& color) {
        return glm::vec3((float)color.redF(), (float)color.greenF(), (float)color.blueF());
    }

    PropertySpec numberSpec(const char* key, const char* label, PropertyKind kind, double min, double max, double step,
                            int decimals, const char* unit) {
        PropertySpec spec;
        spec.key = key;
        spec.label = label;
        spec.kind = kind;
        spec.min = min;
        spec.max = max;
        spec.step = step;
        spec.decimals = decimals;
        spec.unit = unit;
        return spec;
    }

    PropertySpec colorSpec(const char* key, const char* label) {
        PropertySpec spec;
        spec.key = key;
        spec.label = label;
        spec.kind = PropertyKind::FloatColor;
        return spec;
    }

    QPixmap colorSwatch(const QColor& color, int pixels) {
        QPixmap swatch(pixels, pixels);
        swatch.fill(color);
        QPainter painter(&swatch);
        painter.setPen(QColor("#1b1b1b"));
        painter.drawRect(0, 0, pixels - 1, pixels - 1);
        painter.end();
        return swatch;
    }

    // the picture in what is dragged: the first web address naming a picture file, as a file from the Assets panel
    // (its stable link) or a picture from a browser does; a file from Finder is not taken here, it goes into My Files
    // through the Assets panel first
    QUrl droppedPicture(const QMimeData* mime) {
        static const QStringList PICTURE_SUFFIXES { "png", "jpg", "jpeg", "webp", "bmp", "gif", "tga", "tif", "tiff", "ktx", "exr", "hdr" };
        for (const QUrl& url : mime->urls()) {
            if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) {
                continue;
            }
            const QString path = url.path();
            const QString suffix = path.mid(path.lastIndexOf('.') + 1).toLower();
            if (path.contains('.') && PICTURE_SUFFIXES.contains(suffix)) {
                return url;
            }
        }
        return QUrl();
    }

    // a drop taken as a copy where the drag allows it: a move would have the Assets panel take the item out of its view
    void acceptCopy(QDropEvent* event) {
        if (event->possibleActions() & Qt::CopyAction) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        } else {
            event->acceptProposedAction();
        }
    }

    QPixmap framed(const QPixmap& picture, int pixels) {
        QPixmap swatch = picture.scaled(pixels, pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter painter(&swatch);
        painter.setPen(QColor("#1b1b1b"));
        painter.drawRect(0, 0, pixels - 1, pixels - 1);
        painter.end();
        return swatch;
    }
}

const QVector<TerrainLayersEditor::LibraryEntry>& TerrainLayersEditor::library() {
    static QVector<LibraryEntry> entries;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QFile file(LIBRARY_PATH);
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonArray list = QJsonDocument::fromJson(file.readAll()).array();
            for (const QJsonValue& value : list) {
                const QJsonObject object = value.toObject();
                LibraryEntry entry;
                entry.name = object.value("name").toString();
                entry.category = object.value("category").toString();
                entry.url = object.value("url").toString();
                entry.normal = object.value("normal").toString();
                entry.roughness = object.value("roughness").toString();
                entry.occlusion = object.value("occlusion").toString();
                entry.height = object.value("height").toString();
                entry.tiling = (float)object.value("tiling").toDouble(4.0);
                const QJsonArray color = object.value("color").toArray();
                if (color.size() == 3) {
                    entry.color = glm::vec3((float)color[0].toDouble(), (float)color[1].toDouble(), (float)color[2].toDouble());
                }
                if (!entry.name.isEmpty()) {
                    entries.push_back(entry);
                }
            }
        }
    }
    return entries;
}

TerrainLayersEditor::TerrainLayersEditor(const PropertySpec& spec, QWidget* parent) : PropertyEditor(spec, parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    _note = new QLabel(this);
    _note->setObjectName("EditorInspectorNote");
    _note->setWordWrap(true);
    layout->addWidget(_note);

    _body = new QWidget(this);
    auto bodyLayout = new QVBoxLayout(_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);
    layout->addWidget(_body);

    // the list: the top of the stack at the top, dragged to reorder
    _list = new QListWidget(_body);
    _list->setObjectName("EditorTerrainLayerList");
    _list->setSelectionMode(QAbstractItemView::SingleSelection);
    _list->setDragDropMode(QAbstractItemView::InternalMove);
    _list->setDefaultDropAction(Qt::MoveAction);
    _list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _list->setMinimumHeight(ROW_PIXELS * terrain::MAX_LAYERS + 6);
    _list->setToolTip("The terrain's layers, the top one drawn last; drag a layer to change the order. The selected layer is the one Paint "
                      "and Paint Voxels paint");
    connect(_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (_updating || _reordering) {
            return;
        }
        auto item = _list->item(row);
        _selectedSlot = item ? item->data(Qt::UserRole).toInt() : -1;
        markSelectedRow();
        showSelected();
        if (_selectedSlot >= 0) {
            emit layerChosen(_selectedSlot);
        }
    });
    connect(_list->model(), &QAbstractItemModel::rowsMoved, this, [this] { dragReordered(); });
    _list->viewport()->installEventFilter(this);   // a picture dropped on a row: that layer's colour texture
    bodyLayout->addWidget(_list);

    // the buttons under it
    auto buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(2);
    auto button = [this](const QString& text, const QString& toolTip) {
        auto b = new QToolButton(_body);
        b->setText(text);
        b->setToolTip(toolTip);
        b->setAutoRaise(true);
        return b;
    };
    _addButton = button("+ ▾", "Add a layer from the material library, or an empty one (eight at most)");
    _addButton->setObjectName("EditorTerrainLayerAdd");   // the text carries the menu's chevron; the stylesheet drops Qt's
    _addButton->setPopupMode(QToolButton::InstantPopup);
    auto addMenu = new QMenu(_addButton);
    const QVector<LibraryEntry>& entries = library();
    // a submenu a category, in the library's order (the standard materials number some forty)
    QMenu* categoryMenu = nullptr;
    QString category;
    for (int i = 0; i < entries.size(); ++i) {
        if (!categoryMenu || entries[i].category != category) {
            category = entries[i].category;
            categoryMenu = addMenu->addMenu(category.isEmpty() ? QString("Other") : category);
        }
        categoryMenu->addAction(entries[i].name, this, [this, i] { addLayer(i); });
    }
    if (!entries.isEmpty()) {
        addMenu->addSeparator();
    }
    addMenu->addAction("Empty Layer", this, [this] { addLayer(-1); });
    connect(addMenu, &QMenu::aboutToShow, this, [this] { _menuOpen = true; });
    connect(addMenu, &QMenu::aboutToHide, this, [this] { _menuOpen = false; });
    _addButton->setMenu(addMenu);
    _removeButton = button("−", "Remove the selected layer. What was painted or sculpted with it stays, unseen, until a layer takes its slot again");
    connect(_removeButton, &QToolButton::clicked, this, [this] { removeLayer(); });
    _upButton = button("▲", "Move the selected layer up: it draws over the ones below it");
    connect(_upButton, &QToolButton::clicked, this, [this] { moveLayer(1); });
    _downButton = button("▼", "Move the selected layer down");
    connect(_downButton, &QToolButton::clicked, this, [this] { moveLayer(-1); });
    _resetButton = button("Reset", "Back to the eight default layers");
    connect(_resetButton, &QToolButton::clicked, this, [this] { resetLayers(); });
    buttons->addWidget(_addButton);
    buttons->addWidget(_removeButton);
    buttons->addWidget(_upButton);
    buttons->addWidget(_downButton);
    buttons->addStretch(1);
    buttons->addWidget(_resetButton);
    bodyLayout->addLayout(buttons);

    // the selected layer's own settings
    _fields = new QWidget(_body);
    auto grid = new QGridLayout(_fields);
    grid->setContentsMargins(0, 2, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);
    int row = 0;
    auto addRow = [&](QWidget* label, QWidget* field) {
        label->setObjectName("EditorInspectorLabel");
        grid->addWidget(label, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
        ++row;
    };

    _name = new QLineEdit(_fields);
    _name->setToolTip("The layer's name, for this list");
    connect(_name, &QLineEdit::editingFinished, this, [this] {
        auto layer = selectedLayer();
        if (_updating || !layer || !_name->isModified()) {
            return;
        }
        layer->name = _name->text().trimmed().toStdString();
        _name->setModified(false);
        commit("Rename Layer");
    });
    addRow(new QLabel("Name", _fields), _name);

    _material = new QComboBox(_fields);
    _material->setToolTip("The layer's material: one of the library's, with its maps, or Custom for any textures by their URLs below");
    // grouped under a disabled heading a category; an item's data is its library index (MATERIAL_HEADING, MATERIAL_CUSTOM)
    QString heading;
    for (int i = 0; i < entries.size(); ++i) {
        if (i == 0 || entries[i].category != heading) {
            heading = entries[i].category;
            _material->addItem(heading.isEmpty() ? QString("Other") : heading, MATERIAL_HEADING);
            if (auto model = qobject_cast<QStandardItemModel*>(_material->model())) {
                auto item = model->item(_material->count() - 1);
                item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            }
        }
        _material->addItem("    " + entries[i].name, i);
    }
    _material->addItem("Custom…", MATERIAL_CUSTOM);
    connect(_material, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        auto layer = selectedLayer();
        if (_updating || !layer) {
            return;
        }
        const int libraryIndex = _material->itemData(index).toInt();
        if (libraryIndex >= 0 && libraryIndex < library().size()) {
            const LibraryEntry& entry = library()[libraryIndex];
            layer->materialURL = entry.url.toStdString();
            layer->normalURL = entry.normal.toStdString();
            layer->roughnessURL = entry.roughness.toStdString();
            layer->occlusionURL = entry.occlusion.toStdString();
            layer->heightURL = entry.height.toStdString();
            layer->color = entry.color;
            commit("Layer Material");
        } else {
            _url->setFocus();
            _url->selectAll();
        }
    });
    addRow(new QLabel("Material", _fields), _material);

    _url = new QLineEdit(_fields);
    _url->setPlaceholderText("https://… (none draws the plain colour)");
    _url->setToolTip("The layer's base map: its colour texture, by URL; empty draws the plain colour");
    connect(_url, &QLineEdit::editingFinished, this, [this] {
        auto layer = selectedLayer();
        if (_updating || !layer || !_url->isModified()) {
            return;
        }
        layer->materialURL = _url->text().trimmed().toStdString();
        _url->setModified(false);
        commit("Layer Base Map");
    });
    addRow(new QLabel("Base Map", _fields), _url);
    _mapFields.push_back({ _url, "Layer Base Map", [](terrain::Layer& layer, const std::string& url) { layer.materialURL = url; } });
    _url->installEventFilter(this);

    // the other maps, by URL: one line each, applied when editing ends
    auto mapField = [&](QLineEdit*& edit, const char* label, const char* tooltip, const char* undoText,
                        std::function<void(terrain::Layer&, const std::string&)> apply) {
        edit = new QLineEdit(_fields);
        edit->setPlaceholderText("https://… (none)");
        edit->setToolTip(tooltip);
        QLineEdit* field = edit;
        QString text = undoText;
        connect(edit, &QLineEdit::editingFinished, this, [this, field, apply, text] {
            auto layer = selectedLayer();
            if (_updating || !layer || !field->isModified()) {
                return;
            }
            apply(*layer, field->text().trimmed().toStdString());
            field->setModified(false);
            commit(text);
        });
        addRow(new QLabel(label, _fields), edit);
        _mapFields.push_back({ edit, text, apply });
        edit->installEventFilter(this);   // a picture dropped on it: the map
    };
    mapField(_normalURL, "Normal Map", "The layer's normal map, tangent space with green up (OpenGL's way); none leaves the ground's own normal",
             "Layer Normal Map", [](terrain::Layer& layer, const std::string& url) { layer.normalURL = url; });

    _useNormal = new QCheckBox(_fields);
    _useNormal->setToolTip("Whether the normal map is drawn");
    connect(_useNormal, &QCheckBox::toggled, this, [this](bool on) {
        auto layer = selectedLayer();
        if (_updating || !layer) {
            return;
        }
        layer->useNormal = on;
        commit(on ? "Normal Map On" : "Normal Map Off");
    });
    addRow(new QLabel("Normal Map On", _fields), _useNormal);

    auto number = [&](NumberEditor*& editor, const PropertySpec& spec, const char* undoText, std::function<void(terrain::Layer&, double)> apply) {
        editor = new NumberEditor(spec, _fields);
        connect(editor, &PropertyEditor::editBegan, this, [this] { beginScrub(); });
        connect(editor, &PropertyEditor::liveValue, this, [this, apply](const QVariant& value) {
            if (auto layer = selectedLayer()) {
                apply(*layer, value.toDouble());
                preview();
            }
        });
        QString text = undoText;
        connect(editor, &PropertyEditor::committed, this, [this, apply, text](const QVariant& value) {
            if (auto layer = selectedLayer()) {
                apply(*layer, value.toDouble());
                commit(text, true);
            }
        });
        connect(editor, &PropertyEditor::editCancelled, this, [this] { cancelScrub(); });
        addRow(editor->createLabel(_fields), editor);
    };
    number(_normalStrength, numberSpec("normalStrength", "Normal Strength", PropertyKind::Float, 0.0, 4.0, 0.05, 2, ""), "Layer Normal Strength",
           [](terrain::Layer& layer, double value) { layer.normalStrength = (float)value; });
    _normalStrength->setToolTip("How far the normal map tilts the ground: 0 is flat, 1 as the map says, more exaggerates it");
    mapField(_roughnessURL, "Roughness Map", "One channel, black smooth to white rough; none is the terrain's plain roughness",
             "Layer Roughness Map", [](terrain::Layer& layer, const std::string& url) { layer.roughnessURL = url; });
    mapField(_occlusionURL, "Occlusion Map", "One channel, black shaded to white open; none is no occlusion",
             "Layer Occlusion Map", [](terrain::Layer& layer, const std::string& url) { layer.occlusionURL = url; });
    mapField(_heightURL, "Height Map", "One channel, black low to white high, read by the Height blend; none is half height",
             "Layer Height Map", [](terrain::Layer& layer, const std::string& url) { layer.heightURL = url; });
    number(_tiling, numberSpec("tiling", "Tiling", PropertyKind::Float, 0.05, 1000.0, 0.1, 2, "m"), "Layer Tiling",
           [](terrain::Layer& layer, double value) { layer.tiling = (float)value; });
    _tiling->setToolTip("Metres per repeat of the texture");
    number(_offsetX, numberSpec("offsetX", "Offset X", PropertyKind::Float, -10000.0, 10000.0, 0.1, 2, "m"), "Layer Offset",
           [](terrain::Layer& layer, double value) { layer.offset.x = (float)value; });
    number(_offsetZ, numberSpec("offsetZ", "Offset Z", PropertyKind::Float, -10000.0, 10000.0, 0.1, 2, "m"), "Layer Offset",
           [](terrain::Layer& layer, double value) { layer.offset.y = (float)value; });
    number(_opacity, numberSpec("opacity", "Opacity", PropertyKind::Slider, 0.0, 100.0, 1.0, 0, "%"), "Layer Opacity",
           [](terrain::Layer& layer, double value) { layer.opacity = (float)(value / 100.0); });
    _opacity->setToolTip("How much of the layer's paint shows");

    _blend = new QComboBox(_fields);
    _blend->addItems(BLEND_NAMES);
    _blend->setToolTip("How the layer meets what is below it where it is painted: covers it, darkens it (Multiply), lightens it "
                       "(Screen), deepens its contrast (Overlay), or shows where its height map plus its paint rises above what is "
                       "below (Height)");
    connect(_blend, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        auto layer = selectedLayer();
        if (_updating || !layer || index < 0 || index >= terrain::LAYER_BLEND_COUNT) {
            return;
        }
        layer->blend = (terrain::LayerBlend)index;
        commit("Layer Blend");
    });
    addRow(new QLabel("Blend", _fields), _blend);

    auto color = [&](ColorEditor*& editor, const PropertySpec& spec, const char* undoText, std::function<void(terrain::Layer&, const QColor&)> apply) {
        editor = new ColorEditor(spec, _fields);
        connect(editor, &PropertyEditor::editBegan, this, [this] { beginScrub(); });
        connect(editor, &PropertyEditor::liveValue, this, [this, apply](const QVariant& value) {
            if (auto layer = selectedLayer()) {
                apply(*layer, value.value<QColor>());
                preview();
            }
        });
        QString text = undoText;
        connect(editor, &PropertyEditor::committed, this, [this, apply, text](const QVariant& value) {
            if (auto layer = selectedLayer()) {
                apply(*layer, value.value<QColor>());
                commit(text, true);
            }
        });
        connect(editor, &PropertyEditor::editCancelled, this, [this] { cancelScrub(); });
        addRow(new QLabel(spec.label, _fields), editor);
    };
    color(_tint, colorSpec("tint", "Tint"), "Layer Tint", [](terrain::Layer& layer, const QColor& value) { layer.tint = fromQColor(value); });
    _tint->setToolTip("Multiplies the texture; white leaves it as it is");
    color(_color, colorSpec("color", "Colour"), "Layer Colour", [](terrain::Layer& layer, const QColor& value) { layer.color = fromQColor(value); });
    _color->setToolTip("The plain colour: drawn without a texture, and until the texture is in");

    _visible = new QCheckBox(_fields);
    _visible->setToolTip("Whether the layer's paint is drawn; the voxels of its material show either way");
    connect(_visible, &QCheckBox::toggled, this, [this](bool on) {
        auto layer = selectedLayer();
        if (_updating || !layer) {
            return;
        }
        layer->visible = on;
        commit(on ? "Show Layer" : "Hide Layer");
    });
    addRow(new QLabel("Shown", _fields), _visible);

    bodyLayout->addWidget(_fields);

    _note->setText("No terrain");
    _body->hide();
}

bool TerrainLayersEditor::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type type = event->type();
    if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::DragLeave && type != QEvent::Drop) {
        return PropertyEditor::eventFilter(watched, event);
    }

    // a row of the list: the picture becomes that layer's colour texture, and the layer the selected one
    if (watched == _list->viewport()) {
        if (type == QEvent::DragLeave) {
            setDropRow(-1);
            return false;
        }
        auto drop = static_cast<QDropEvent*>(event);   // the enter and move events are drop events too
        if (drop->source() == _list) {
            return false;   // a row dragged within the list: the list reorders the layers
        }
        const QUrl url = droppedPicture(drop->mimeData());
        QListWidgetItem* item = url.isEmpty() ? nullptr : _list->itemAt(drop->pos());
        if (!item) {
            setDropRow(-1);
            drop->ignore();
            return true;
        }
        if (type != QEvent::Drop) {
            setDropRow(_list->row(item));
            acceptCopy(drop);
            return true;
        }
        setDropRow(-1);
        acceptCopy(drop);
        const int slot = item->data(Qt::UserRole).toInt();
        if (auto layer = _stack.find((uint8_t)slot)) {
            layer->materialURL = url.toString().toStdString();
            _selectedSlot = slot;
            commit("Layer Base Map");
            emit layerChosen(slot);
        }
        return true;
    }

    // a map field: the picture becomes that map of the selected layer
    for (const MapField& field : _mapFields) {
        if (watched != field.edit) {
            continue;
        }
        if (type == QEvent::DragLeave) {
            return false;
        }
        auto drop = static_cast<QDropEvent*>(event);
        const QUrl url = droppedPicture(drop->mimeData());
        if (url.isEmpty() || !selectedLayer()) {
            drop->ignore();
            return true;   // not a picture: nor is it pasted in as typing
        }
        acceptCopy(drop);
        if (type == QEvent::Drop) {
            const QString text = url.toString();
            field.edit->setText(text);
            field.edit->setModified(false);
            field.apply(*selectedLayer(), text.toStdString());
            commit(field.undoText);
        }
        return true;
    }
    return PropertyEditor::eventFilter(watched, event);
}

void TerrainLayersEditor::setDropRow(int row) {
    if (row == _dropRow) {
        return;
    }
    auto mark = [this](int index, bool on) {
        QListWidgetItem* item = index >= 0 ? _list->item(index) : nullptr;
        QWidget* widget = item ? _list->itemWidget(item) : nullptr;
        if (!widget) {
            return;
        }
        widget->setProperty("dropTarget", on);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    };
    mark(_dropRow, false);
    _dropRow = row;
    mark(_dropRow, true);
}

QWidget* TerrainLayersEditor::createLabel(QWidget* parent) {
    auto label = new QWidget(parent);
    label->setFixedSize(0, 0);
    return label;
}

void TerrainLayersEditor::setValue(const QVariant& value, bool mixed) {
    Q_UNUSED(value);
    Q_UNUSED(mixed);
}

bool TerrainLayersEditor::isEditing() const {
    if (_menuOpen || _reordering) {
        return true;
    }
    for (NumberEditor* editor : { _tiling, _offsetX, _offsetZ, _opacity, _normalStrength }) {
        if (editor->isEditing()) {
            return true;
        }
    }
    if (_tint->isEditing() || _color->isEditing()) {
        return true;
    }
    for (QLineEdit* edit : { _name, _url, _normalURL, _roughnessURL, _occlusionURL, _heightURL }) {
        if (edit->hasFocus() && edit->isModified()) {
            return true;
        }
    }
    return false;
}

void TerrainLayersEditor::showTerrain(const QUuid& id, bool multiple) {
    const terrain::LayerStack* layers = (!multiple && !id.isNull() && _terrain) ? _terrain->layersOf(id) : nullptr;
    if (!layers) {
        _note->setText(multiple ? "Select one terrain to edit its layers" : "No terrain");
        _note->show();
        _body->hide();
        _id = QUuid();
        return;
    }
    const uint64_t version = _terrain->layersVersionOf(id);
    if (id == _id && version == _shownVersion && _body->isVisible()) {
        return;
    }
    if (id != _id) {
        _selectedSlot = -1;
    }
    _id = id;
    _shownVersion = version;
    _stack = *layers;
    _note->hide();
    _body->show();
    rebuildList();
}

int TerrainLayersEditor::selectedIndex() const {
    return _selectedSlot >= 0 ? _stack.indexOf((uint8_t)_selectedSlot) : -1;
}

terrain::Layer* TerrainLayersEditor::selectedLayer() {
    const int index = selectedIndex();
    return index >= 0 ? &_stack.layers[index] : nullptr;
}

void TerrainLayersEditor::rebuildList() {
    _updating = true;
    _dropRow = -1;   // the rows are made again
    _list->clear();
    int selectRow = -1;
    for (int index = (int)_stack.layers.size() - 1; index >= 0; --index) {
        const terrain::Layer& layer = _stack.layers[index];
        auto item = new QListWidgetItem(_list);
        item->setSizeHint(QSize(0, ROW_PIXELS));
        item->setData(Qt::UserRole, (int)layer.slot);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
        _list->setItemWidget(item, makeRow(layer));
        if ((int)layer.slot == _selectedSlot) {
            selectRow = _list->count() - 1;
        }
    }
    if (selectRow < 0 && _list->count() > 0) {
        selectRow = 0;
        _selectedSlot = _list->item(0)->data(Qt::UserRole).toInt();
    }
    _list->setCurrentRow(selectRow);
    _addButton->setEnabled(_stack.freeSlot() >= 0);
    _updating = false;
    markSelectedRow();
    showSelected();
    emit layersShown();
}

void TerrainLayersEditor::markSelectedRow() {
    for (int i = 0; i < _list->count(); ++i) {
        auto item = _list->item(i);
        auto row = _list->itemWidget(item);
        if (!row) {
            continue;
        }
        const bool selected = item->data(Qt::UserRole).toInt() == _selectedSlot;
        if (row->property("selected").toBool() != selected) {
            row->setProperty("selected", selected);
            for (QWidget* widget : row->findChildren<QWidget*>()) {
                widget->style()->unpolish(widget);
                widget->style()->polish(widget);
            }
            row->style()->unpolish(row);
            row->style()->polish(row);
        }
    }
}

QWidget* TerrainLayersEditor::makeRow(const terrain::Layer& layer) {
    auto row = new QWidget(_list);
    row->setObjectName("EditorTerrainLayerRow");
    row->setAttribute(Qt::WA_StyledBackground);   // a plain widget paints its stylesheet background only when asked to
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(4, 1, 4, 1);
    layout->setSpacing(6);
    auto eye = new QCheckBox(row);
    eye->setChecked(layer.visible);
    eye->setToolTip("Shown");
    const int slot = layer.slot;
    connect(eye, &QCheckBox::toggled, this, [this, slot](bool on) {
        auto found = _stack.find((uint8_t)slot);
        if (_updating || !found) {
            return;
        }
        found->visible = on;
        commit(on ? "Show Layer" : "Hide Layer");
    });
    layout->addWidget(eye);
    auto swatch = new QLabel(row);
    swatch->setObjectName("EditorTerrainLayerSwatch");
    swatch->setFixedSize(SWATCH_PIXELS, SWATCH_PIXELS);
    swatch->setPixmap(swatchFor(layer));
    layout->addWidget(swatch);
    auto name = new QLabel(QString::fromStdString(layer.name), row);
    name->setObjectName("EditorTerrainLayerName");
    name->setToolTip(QString("Slot %1: material index %1 of the voxels, plane %1 of the paint map").arg(layer.slot));
    layout->addWidget(name, 1);
    if (!layer.materialURL.empty()) {
        requestSwatch(QString::fromStdString(layer.materialURL));
    }
    return row;
}

QPixmap TerrainLayersEditor::swatchFor(const terrain::Layer& layer) const {
    const QString url = QString::fromStdString(layer.materialURL);
    if (!url.isEmpty()) {
        auto found = swatches().find(url);
        if (found != swatches().end() && !found->isNull()) {
            return framed(*found, SWATCH_PIXELS);
        }
    }
    return colorSwatch(toQColor(layer.color), SWATCH_PIXELS);
}

void TerrainLayersEditor::requestSwatch(const QString& url) {
    if (url.isEmpty() || swatches().contains(url)) {
        return;
    }
    const QUrl address(url);
    if (address.scheme() == QStringLiteral("qrc")) {
        swatches().insert(url, QPixmap(QStringLiteral(":") + address.path()));
        refreshSwatches();
        return;
    }
    if (address.isLocalFile()) {
        swatches().insert(url, QPixmap(address.toLocalFile()));
        refreshSwatches();
        return;
    }
    if (address.scheme() != QStringLiteral("http") && address.scheme() != QStringLiteral("https")) {
        swatches().insert(url, QPixmap());   // nothing to fetch: the plain colour stands in
        return;
    }
    swatches().insert(url, QPixmap());   // taken, until the picture comes
    QNetworkReply* reply = NetworkAccessManager::getInstance().get(QNetworkRequest(address));
    QPointer<TerrainLayersEditor> self(this);
    connect(reply, &QNetworkReply::finished, reply, [self, reply, url] {
        reply->deleteLater();
        QPixmap pixmap;
        if (reply->error() == QNetworkReply::NoError && pixmap.loadFromData(reply->readAll())) {
            swatches().insert(url, pixmap.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        if (self) {
            self->refreshSwatches();
        }
    });
}

void TerrainLayersEditor::refreshSwatches() {
    for (int row = 0; row < _list->count(); ++row) {
        auto item = _list->item(row);
        auto widget = _list->itemWidget(item);
        const terrain::Layer* layer = _stack.find((uint8_t)item->data(Qt::UserRole).toInt());
        if (!widget || !layer) {
            continue;
        }
        if (auto swatch = widget->findChild<QLabel*>("EditorTerrainLayerSwatch")) {
            swatch->setPixmap(swatchFor(*layer));
        }
    }
    // the material menu's icons too
    const QVector<LibraryEntry>& entries = library();
    for (int item = 0; item < _material->count(); ++item) {
        const int i = _material->itemData(item).toInt();
        if (i < 0 || i >= entries.size()) {
            continue;
        }
        auto found = swatches().find(entries[i].url);
        if (found != swatches().end() && !found->isNull()) {
            _material->setItemIcon(item, QIcon(framed(*found, 16)));
        }
    }
}

void TerrainLayersEditor::showSelected() {
    _updating = true;
    const terrain::Layer* layer = selectedLayer();
    _fields->setEnabled(layer != nullptr);
    _removeButton->setEnabled(layer != nullptr);
    const int index = selectedIndex();
    _upButton->setEnabled(index >= 0 && index + 1 < (int)_stack.layers.size());
    _downButton->setEnabled(index > 0);
    if (layer) {
        _name->setText(QString::fromStdString(layer->name));
        _name->setModified(false);
        const QString url = QString::fromStdString(layer->materialURL);
        int materialIndex = MATERIAL_CUSTOM;
        for (int i = 0; i < library().size(); ++i) {
            if (library()[i].url == url) {
                materialIndex = i;
                break;
            }
        }
        _material->setCurrentIndex(_material->findData(materialIndex));
        _url->setText(url);
        _url->setModified(false);
        _normalURL->setText(QString::fromStdString(layer->normalURL));
        _normalURL->setModified(false);
        _useNormal->setChecked(layer->useNormal);
        _normalStrength->setValue(QVariant((double)layer->normalStrength), false);
        _roughnessURL->setText(QString::fromStdString(layer->roughnessURL));
        _roughnessURL->setModified(false);
        _occlusionURL->setText(QString::fromStdString(layer->occlusionURL));
        _occlusionURL->setModified(false);
        _heightURL->setText(QString::fromStdString(layer->heightURL));
        _heightURL->setModified(false);
        _tiling->setValue(QVariant((double)layer->tiling), false);
        _offsetX->setValue(QVariant((double)layer->offset.x), false);
        _offsetZ->setValue(QVariant((double)layer->offset.y), false);
        _opacity->setValue(QVariant((double)layer->opacity * 100.0), false);
        _blend->setCurrentIndex(glm::clamp((int)layer->blend, 0, terrain::LAYER_BLEND_COUNT - 1));
        _tint->setValue(QVariant(toQColor(layer->tint)), false);
        _color->setValue(QVariant(toQColor(layer->color)), false);
        _visible->setChecked(layer->visible);
    }
    _updating = false;
}

void TerrainLayersEditor::preview() {
    if (!_terrain || _id.isNull()) {
        return;
    }
    _terrain->setLayers(_id, _stack, QString());
    _shownVersion = _terrain->layersVersionOf(_id);
}

void TerrainLayersEditor::commit(const QString& text, bool fromScrub) {
    if (!_terrain || _id.isNull()) {
        return;
    }
    if (!_terrain->setLayers(_id, _stack, text, fromScrub ? &_before : nullptr)) {
        // refused (the terrain is locked, or gone): back to what it holds
        if (const terrain::LayerStack* layers = _terrain->layersOf(_id)) {
            _stack = *layers;
        }
    }
    _shownVersion = _terrain->layersVersionOf(_id);
    rebuildList();
}

void TerrainLayersEditor::beginScrub() {
    _before = (_terrain && !_id.isNull() && _terrain->layersOf(_id)) ? *_terrain->layersOf(_id) : _stack;
}

void TerrainLayersEditor::cancelScrub() {
    _stack = _before;
    preview();
    showSelected();
}

void TerrainLayersEditor::addLayer(int libraryIndex) {
    const int slot = _stack.freeSlot();
    if (slot < 0) {
        return;
    }
    terrain::Layer layer;
    layer.slot = (uint8_t)slot;
    if (libraryIndex >= 0 && libraryIndex < library().size()) {
        const LibraryEntry& entry = library()[libraryIndex];
        layer.name = entry.name.toStdString();
        layer.materialURL = entry.url.toStdString();
        // the maps too: only the eight defaults' would be filled in by their colour texture's URL
        layer.normalURL = entry.normal.toStdString();
        layer.roughnessURL = entry.roughness.toStdString();
        layer.occlusionURL = entry.occlusion.toStdString();
        layer.heightURL = entry.height.toStdString();
        layer.tiling = entry.tiling;
        layer.color = entry.color;
    } else {
        layer.name = QString("Layer %1").arg(slot + 1).toStdString();
    }
    _stack.layers.push_back(layer);
    _selectedSlot = slot;
    commit("Add Layer");
    emit layerChosen(slot);
}

void TerrainLayersEditor::removeLayer() {
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    _stack.layers.erase(_stack.layers.begin() + index);
    _selectedSlot = -1;
    commit("Remove Layer");
    if (_selectedSlot >= 0) {
        emit layerChosen(_selectedSlot);
    }
}

void TerrainLayersEditor::moveLayer(int by) {
    const int index = selectedIndex();
    const int other = index + by;
    if (index < 0 || other < 0 || other >= (int)_stack.layers.size()) {
        return;
    }
    std::swap(_stack.layers[index], _stack.layers[other]);
    commit("Reorder Layers");
}

void TerrainLayersEditor::dragReordered() {
    if (_updating || _reordering) {
        return;
    }
    _reordering = true;
    // the rows top to bottom are the stack top to bottom
    terrain::LayerStack reordered;
    for (int row = _list->count() - 1; row >= 0; --row) {
        if (const terrain::Layer* layer = _stack.find((uint8_t)_list->item(row)->data(Qt::UserRole).toInt())) {
            reordered.layers.push_back(*layer);
        }
    }
    if (reordered.layers.size() == _stack.layers.size()) {
        _stack = reordered;
    }
    _reordering = false;
    commit("Reorder Layers");
}

void TerrainLayersEditor::resetLayers() {
    _stack = terrain::LayerStack::defaults();
    commit("Reset Layers");
}
