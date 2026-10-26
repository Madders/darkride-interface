//
//  TestClientIcon.cpp
//  interface/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "TestClientIcon.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <PathUtils.h>

#include "InterfaceLogging.h"

namespace {

const int ICON_SIZE = 1024;   // the Dock's largest

// The icon the app shows now: on macOS the bundle's .icns (Info.plist's CFBundleIconFile), else the DR logo
QIcon appIcon() {
#ifdef Q_OS_MACOS
    const QDir contents(QCoreApplication::applicationDirPath() + "/..");
    QSettings info(contents.absoluteFilePath("Info.plist"), QSettings::NativeFormat);
    QString file = info.value("CFBundleIconFile").toString();
    if (!file.isEmpty()) {
        if (!file.endsWith(".icns")) {
            file += ".icns";
        }
        QIcon bundleIcon(contents.absoluteFilePath("Resources/" + file));
        if (!bundleIcon.availableSizes().isEmpty()) {
            return bundleIcon;
        }
    }
#endif
    const QIcon windowIcon = QApplication::windowIcon();
    if (!windowIcon.availableSizes().isEmpty()) {
        return windowIcon;
    }
    return QIcon(PathUtils::resourcesPath() + "images/darkride-logo.png");
}

// skunkworks' mock-up (team/skunkworks/prototypes/test-client-icon/make-mockup.py), in its proportions: the icon dimmed
// by 35 %, a red ring and slash 8.5 % wide, and a rounded tag along the bottom with the word in it
QPixmap drawTestIcon(const QIcon& base, const QString& tag, const QColor& tagFill, const QColor& tagInk) {
    const qreal size = ICON_SIZE;
    QPixmap pixmap(ICON_SIZE, ICON_SIZE);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRectF(0, 0, size, size), base.pixmap(ICON_SIZE, ICON_SIZE), QRectF());

    // dimmed only where the icon is (its transparent corners stay transparent)
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(QRectF(0, 0, size, size), QColor(0, 0, 0, 89));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QColor red(224, 32, 32);
    const qreal width = size * 0.085;
    const qreal margin = size * 0.12 + width / 2;
    painter.setPen(QPen(red, width, Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(margin, margin, size - 2 * margin, size - 2 * margin));
    const qreal radius = (size - 2 * margin) / 2;
    const qreal reach = radius * 0.70710678;   // cos 45 degrees: the slash from ring to ring, top left to bottom right
    const QPointF centre(size / 2, size / 2);
    painter.drawLine(centre - QPointF(reach, reach), centre + QPointF(reach, reach));

    if (!tag.isEmpty()) {
        const qreal height = size * 0.2;
        const QRectF strip(size * 0.08, size - size * 0.1 - height, size * 0.84, height);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tagFill);
        painter.drawRoundedRect(strip, height * 0.3, height * 0.3);
        QFont font;
        font.setBold(true);
        qreal pixelSize = height * 0.62;
        font.setPixelSize(int(pixelSize));
        while (QFontMetricsF(font).horizontalAdvance(tag) > size * 0.78 && pixelSize > 10) {
            pixelSize -= 4;
            font.setPixelSize(int(pixelSize));
        }
        painter.setFont(font);
        painter.setPen(tagInk);
        painter.drawText(strip, Qt::AlignCenter, tag);
    }
    painter.end();
    return pixmap;
}

}  // namespace

void setTestClientIcon(const QString& clientLabel) {
    static const QRegularExpression QUIET_WORD("\\bQUIET\\b");
    static const QRegularExpression TEST_WORD("\\bTEST\\b");
    const bool quiet = QUIET_WORD.match(clientLabel).hasMatch();
    if (!quiet && !TEST_WORD.match(clientLabel).hasMatch()) {
        return;
    }
    QString tag;
    QColor tagFill(224, 32, 32);
    QColor tagInk(Qt::white);
    if (quiet) {
        tag = "QUIET";
        tagFill = QColor(255, 204, 0);
        tagInk = Qt::black;
    } else {
        // "Darkride - <session> - TEST, not yours": the middle part names the session
        const QStringList parts = clientLabel.split(" - ");
        if (parts.size() >= 3) {
            tag = parts.at(1).trimmed().toUpper();
        }
    }
    const QPixmap icon = drawTestIcon(appIcon(), tag, tagFill, tagInk);
    QApplication::setWindowIcon(QIcon(icon));
    qCDebug(interfaceapp) << "Test client icon:" << (tag.isEmpty() ? QString("no tag") : tag);
    // DARKRIDE_TEST_ICON_FILE=<file.png> also writes it out, to look at without a screenshot of the Dock
    const QString iconFile = qEnvironmentVariable("DARKRIDE_TEST_ICON_FILE");
    if (!iconFile.isEmpty()) {
        icon.save(iconFile);
    }
}
