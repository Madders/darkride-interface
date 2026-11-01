//
//  RightsLine.cpp
//  interface/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "RightsLine.h"

#include <QtWidgets/QLabel>

#include <NetworkingConstants.h>

namespace rights {

QString sentence() {
    return QStringLiteral("Only share things you made or have the rights to.");
}

QUrl termsUrl() {
    return QUrl(NetworkingConstants::METAVERSE_SERVER_URL().toString() + "/terms#your-content");
}

QString richText(const QColor& link) {
    return sentence().toHtmlEscaped() + QString(" <a href=\"%1\" style=\"color: %2; text-decoration: none;\">Terms</a>")
        .arg(termsUrl().toString().toHtmlEscaped(), link.name());
}

QLabel* label(const QString& objectName, const QColor& link, QWidget* parent) {
    auto label = new QLabel(richText(link), parent);
    label->setObjectName(objectName);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    label->setOpenExternalLinks(true);
    label->setFocusPolicy(Qt::NoFocus);
    return label;
}

}
