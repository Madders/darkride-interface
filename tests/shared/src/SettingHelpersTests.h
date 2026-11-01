//
//  SettingHelpersTests.h
//
//  Copyright 2026 Darkride.
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_SettingHelpersTests_h
#define hifi_SettingHelpersTests_h

#include <QtCore/QObject>

class SettingHelpersTests : public QObject {
    Q_OBJECT
private slots:
    void testNoAppDataDir();
    void testAppDataDirAndConfigHome();
    void testAppDataDirOnly();
};

#endif // hifi_SettingHelpersTests_h
