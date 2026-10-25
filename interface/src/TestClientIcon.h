//
//  TestClientIcon.h
//  interface/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_TestClientIcon_h
#define hifi_TestClientIcon_h

#include <QtCore/QString>

// A test client's Dock (macOS) and taskbar (Windows) icon, so a glance tells which Darkride windows are other
// sessions' test clients and when the machine must stay quiet (Caity, 2026-10-25). The trigger is the client label,
// as the team's rule titles test clients "Darkride - <session> - TEST, not yours":
//   - the word QUIET in it: the app's icon dimmed, a red slashed circle over it and a yellow "QUIET" tag
//     (a quiet timing run: nothing else on the GPU);
//   - else the word TEST: the same with a red tag naming the session (the label's middle part), when there is one;
//   - else nothing changes: hand tests and releases keep the normal icon.
// The words are matched in capitals only, so a label that just says "test" changes nothing.
void setTestClientIcon(const QString& clientLabel);

#endif // hifi_TestClientIcon_h
