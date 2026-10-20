//
//  ImpostorEditor.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ImpostorEditor.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QTimer>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <DependencyManager.h>
#include <EntityTree.h>
#include <EntityTreeRenderer.h>
#include <ImpostorSwitch.h>
#include <NetworkAccessManager.h>
#include <RenderableEntityItem.h>

#include "../ImpostorBaker.h"
#include "../../LODManager.h"

namespace {
    const int ATLAS_WIDTH = 90;    // a 3 x 2 atlas's thumbnail
    const int ATLAS_HEIGHT = 60;
    const int SIZES[] = { 128, 256, 512, 1024 };
    const int DEFAULT_SIZE_INDEX = 1;
    const int STATE_REFRESH_MS = 250;
    const int TINT_DEBUG_VIEW = 10;   // impostor.slf: the card drawn as it is, tinted blue
}

ImpostorEditor::ImpostorEditor(const PropertySpec& spec, QWidget* parent) : PropertyEditor(spec, parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    _summary = new QLabel(this);
    _summary->setWordWrap(true);
    layout->addWidget(_summary);
    _state = new QLabel(this);
    _state->setWordWrap(true);
    _state->setToolTip("What the view draws of this entity now. The card shows where its bounding sphere looks smaller than the switch angle; the change is a short dissolve.");
    layout->addWidget(_state);

    auto atlases = new QHBoxLayout();
    atlases->setSpacing(4);
    for (Atlas* atlas : { &_albedo, &_normal, &_material }) {
        atlas->view = new QLabel(this);
        atlas->view->setFixedSize(ATLAS_WIDTH, ATLAS_HEIGHT);
        atlas->view->setAlignment(Qt::AlignCenter);
        atlas->view->setFrameShape(QFrame::StyledPanel);
        atlases->addWidget(atlas->view);
    }
    _albedo.view->setToolTip("The albedo atlas: the six views' colours, the coverage in alpha");
    _normal.view->setToolTip("The normal atlas: the six views' normals, the depth through the sphere in alpha");
    _material.view->setToolTip("The material atlas: roughness in red, metallic in green");
    atlases->addStretch();
    layout->addLayout(atlases);

    auto actions = new QHBoxLayout();
    actions->setSpacing(4);
    _size = new QComboBox(this);
    for (int size : SIZES) {
        _size->addItem(QString("%1 px").arg(size), size);
    }
    _size->setCurrentIndex(DEFAULT_SIZE_INDEX);
    _size->setToolTip("A frame's side in texels: the card's detail (256 does for a prop; 512 for a big one)");
    _bake = new QPushButton("Bake", this);
    _bake->setToolTip("Draws the entity's models from six sides and writes the card's atlases (Build > Bake Impostor)");
    _remove = new QPushButton("Remove", this);
    _remove->setToolTip("Takes the card off the entity (the atlases stay in My Files)");
    actions->addWidget(_size);
    actions->addWidget(_bake);
    actions->addWidget(_remove);
    actions->addStretch();
    layout->addLayout(actions);

    auto angleRow = new QHBoxLayout();
    angleRow->setSpacing(4);
    auto angleLabel = new QLabel("Switch Angle", this);
    angleLabel->setObjectName("EditorInspectorLabel");
    angleLabel->setToolTip("Every card's: an entity whose bounding sphere looks smaller than this draws its card. Automatic is four times the LOD angle. Not saved with the world.");
    _angle = new QDoubleSpinBox(this);
    _angle->setRange(0.0, 179.0);
    _angle->setDecimals(1);
    _angle->setSingleStep(1.0);
    _angle->setSuffix(QStringLiteral("°"));
    _angle->setSpecialValueText("Automatic");
    _angle->setMaximumWidth(130);
    angleRow->addWidget(angleLabel);
    angleRow->addWidget(_angle);
    angleRow->addStretch();
    layout->addLayout(angleRow);
    _tint = new QCheckBox("Tint Cards", this);
    _tint->setToolTip("Draws every card tinted blue, so that a card is told from its model at a glance. Not saved.");
    layout->addWidget(_tint);

    auto lodManager = DependencyManager::get<LODManager>();
    if (lodManager) {
        _angle->setValue(lodManager->getImpostorAngleSetting());
    }
    connect(_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value) {
        if (auto lodManager = DependencyManager::get<LODManager>()) {
            lodManager->setImpostorAngleDeg((float)value);
        }
    });
    if (lodManager) {
        _tint->setChecked(lodManager->getImpostorDebug() == TINT_DEBUG_VIEW);
    }
    connect(_tint, &QCheckBox::toggled, this, [](bool on) {
        if (auto lodManager = DependencyManager::get<LODManager>()) {
            lodManager->setImpostorDebug(on ? TINT_DEBUG_VIEW : 0);
        }
    });
    _stateTimer = new QTimer(this);
    _stateTimer->setInterval(STATE_REFRESH_MS);
    connect(_stateTimer, &QTimer::timeout, this, &ImpostorEditor::showState);
    _stateTimer->start();
    connect(_bake, &QPushButton::clicked, this, &ImpostorEditor::bake);
    connect(_remove, &QPushButton::clicked, this, &ImpostorEditor::remove);
    auto baker = DependencyManager::get<ImpostorBaker>();
    if (baker) {
        connect(baker.data(), &ImpostorBaker::baked, this, [this](const QUuid& id, bool ok, const QString& message) {
            if (_baking.remove(id) && !ok) {
                _summary->setText("Bake failed: " + message);
            }
            _bake->setEnabled(!_ids.isEmpty());
        });
    }
    setValue(QVariant(QString()), false);
}

QWidget* ImpostorEditor::createLabel(QWidget* parent) {
    auto label = new QWidget(parent);
    label->setFixedSize(0, 0);
    return label;
}

void ImpostorEditor::setValue(const QVariant& value, bool mixed) {
    bool changed = value.toString() != _entryText;
    _entryText = value.toString();
    _mixed = mixed;
    QJsonObject entry = QJsonDocument::fromJson(_entryText.toUtf8()).object();
    bool has = !mixed && !entry.isEmpty();
    if (changed && has) {
        // a new entry: the size menu offers its frame size again (the Inspector refreshes the value every moment,
        // so only a new entry moves the menu, never the person's own choice)
        int index = _size->findData(entry.value("size").toInt(256));
        if (index >= 0) {
            _size->setCurrentIndex(index);
        }
    }
    if (mixed) {
        _summary->setText("—");
    } else if (!has) {
        _summary->setText(_baking.isEmpty() ? "No card of its own" : "Baking…");
    } else {
        QString baked = entry.value("baked").toString();
        QDateTime when = QDateTime::fromString(baked, Qt::ISODate);
        QString whenText = when.isValid() ? when.toLocalTime().toString("yyyy-MM-dd hh:mm") : baked;
        QString text = QString("Baked %1: %2 px a frame, radius %3 m").arg(whenText).arg(entry.value("size").toInt(256))
                           .arg(entry.value("radius").toDouble(), 0, 'f', 2);
        if (entry.value("material").toString().isEmpty()) {
            text += " (no material atlas: bake again for roughness and metallic per texel)";
        }
        _summary->setText(text);
    }
    showAtlas(_albedo, has ? entry.value("albedo").toString() : QString());
    showAtlas(_normal, has ? entry.value("normal").toString() : QString());
    showAtlas(_material, has ? entry.value("material").toString() : QString());
    _remove->setEnabled(has);
    _bake->setEnabled(!_ids.isEmpty() && _baking.isEmpty());
}

void ImpostorEditor::showState() {
    if (!isVisible()) {
        return;
    }
    if (_ids.size() != 1 || _mixed) {
        _state->setText(QString());
        return;
    }
    // the entity's own card's switch, or the nearest ancestor's: a model under a group's card follows that card
    auto treeRenderer = DependencyManager::get<EntityTreeRenderer>();
    auto tree = treeRenderer ? treeRenderer->getTree() : nullptr;
    EntityItemPointer entity = tree ? tree->findEntityByID(_ids.first()) : nullptr;
    auto renderer = treeRenderer ? treeRenderer->renderableForEntityId(_ids.first()) : nullptr;
    auto ownSwitch = renderer ? renderer->getImpostorSwitch() : nullptr;
    auto impostorSwitch = ownSwitch ? ownSwitch : (entity ? render::entities::EntityRenderer::findImpostorSwitch(entity) : nullptr);
    if (!impostorSwitch) {
        _state->setText(QString());
        return;
    }
    QString whose = ownSwitch ? QString() : QStringLiteral(" (its parent's card)");
    auto lodManager = DependencyManager::get<LODManager>();
    float switchAngle = lodManager ? lodManager->getImpostorAngleDeg() : 0.0f;
    float share = impostorSwitch->getShownShare();
    float angle = impostorSwitch->getShownAngleDeg();
    QString now = !impostorSwitch->isCardReady() ? QStringLiteral("the model (the card's pictures are loading)")
        : (share >= 1.0f ? QStringLiteral("the card") : (share <= 0.0f ? QStringLiteral("the model") : QStringLiteral("the two, dissolving")));
    QString looks = (angle >= 180.0f) ? QStringLiteral("the camera is inside its sphere")
        : QString("it looks %1° across").arg(angle, 0, 'f', angle < 10.0f ? 2 : 1);
    _state->setText(QString("Showing %1%2: %3, the card shows below %4°").arg(now, whose, looks).arg(switchAngle, 0, 'f', 1));
}

void ImpostorEditor::bake() {
    auto baker = DependencyManager::get<ImpostorBaker>();
    if (!baker || _ids.isEmpty()) {
        return;
    }
    int size = _size->currentData().toInt();
    for (const QUuid& id : _ids) {
        _baking.insert(id);
        baker->bake(id, size);
    }
    _summary->setText("Baking…");
    _bake->setEnabled(false);
}

void ImpostorEditor::remove() {
    // through the property, so that it is one undo step
    emit committed(QVariant(QString()));
}

void ImpostorEditor::showAtlas(Atlas& atlas, const QString& url) {
    if (url == atlas.shown) {
        return;
    }
    atlas.shown = url;
    if (atlas.reply) {
        QPointer<QNetworkReply> previous = atlas.reply;
        atlas.reply = nullptr;
        previous->abort();
    }
    atlas.view->setPixmap(QPixmap());
    atlas.view->setText(QString());
    if (url.isEmpty()) {
        return;
    }
    QUrl address(url);
    if (address.isLocalFile()) {
        QPixmap pixmap(address.toLocalFile());
        if (pixmap.isNull()) {
            atlas.view->setText(QStringLiteral("?"));
        } else {
            atlas.view->setPixmap(pixmap.scaled(ATLAS_WIDTH - 2, ATLAS_HEIGHT - 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        return;
    }
    if (address.scheme() != QStringLiteral("http") && address.scheme() != QStringLiteral("https")) {
        atlas.view->setText(QStringLiteral("?"));
        return;
    }
    atlas.view->setText(QStringLiteral("…"));
    QNetworkRequest request(address);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);   // the files host redirects to the CDN
    atlas.reply = NetworkAccessManager::getInstance().get(request);
    QPointer<QNetworkReply> reply = atlas.reply;
    QLabel* view = atlas.view;
    connect(reply, &QNetworkReply::finished, this, [this, reply, view, &atlas] {
        if (!reply) {
            return;
        }
        reply->deleteLater();
        if (atlas.reply != reply) {
            return;   // a later picture replaced this one
        }
        atlas.reply = nullptr;
        QPixmap pixmap;
        if (reply->error() == QNetworkReply::NoError && pixmap.loadFromData(reply->readAll())) {
            view->setPixmap(pixmap.scaled(ATLAS_WIDTH - 2, ATLAS_HEIGHT - 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            view->setText(QStringLiteral("?"));
        }
    });
}
