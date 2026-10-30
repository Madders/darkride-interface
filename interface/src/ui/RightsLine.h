//
//  RightsLine.h
//  interface/src/ui
//
//  The rights line (Caity, 10-26): one sentence, the same wherever something leaves this computer to be seen by others
//  (an upload, an avatar put in My Files, a share), in small grey text right under the button or where the files are
//  dropped, with a link to the "Your content" paragraph of the Terms, which says the same. Fine print, never a
//  question: no tick box, no extra click. The website shows the same sentence (metaverse's shared component); QML
//  panels read it through MetaverseRequests.rightsLine() and termsUrl(), so it is written once, here.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_RightsLine_h
#define hifi_RightsLine_h

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtGui/QColor>

class QLabel;
class QWidget;

namespace rights {

// the sentence, without its link
QString sentence();
// the Terms' "Your content" paragraph, on the metaverse this client signs in to
QUrl termsUrl();
// the sentence and its "Terms" link, as rich text, the link in the given colour
QString richText(const QColor& link);
// a word-wrapped label of it that opens the Terms in the browser; styled by its object name
QLabel* label(const QString& objectName, const QColor& link, QWidget* parent = nullptr);

}

#endif // hifi_RightsLine_h
