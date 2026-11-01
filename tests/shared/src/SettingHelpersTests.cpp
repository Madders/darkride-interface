//
//  SettingHelpersTests.cpp
//
//  Copyright 2026 Darkride.
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "SettingHelpersTests.h"

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <SettingHelpers.h>

QTEST_MAIN(SettingHelpersTests)

// QSettings::setPath holds for the rest of the process, so the case that sets it comes last.

void SettingHelpersTests::testNoAppDataDir() {
    qunsetenv("TIVOLI_APP_DATA_DIR");
    qputenv("XDG_CONFIG_HOME", "/tmp/darkride-settings-test-unused");
    QCOMPARE(setSettingsPathFromEnvironment(), QString());
}

void SettingHelpersTests::testAppDataDirAndConfigHome() {
    qputenv("TIVOLI_APP_DATA_DIR", "/tmp/darkride-settings-test-data");
    qputenv("XDG_CONFIG_HOME", "/tmp/darkride-settings-test-config");
#ifdef Q_OS_WIN
    // Qt doesn't read XDG_CONFIG_HOME on Windows, so it is set here, as macOS and Linux read it
    QCOMPARE(setSettingsPathFromEnvironment(), QString("/tmp/darkride-settings-test-config"));
#else
    QCOMPARE(setSettingsPathFromEnvironment(), QString());
#endif
}

void SettingHelpersTests::testAppDataDirOnly() {
    QTemporaryDir appData;
    QVERIFY(appData.isValid());
    qputenv("TIVOLI_APP_DATA_DIR", appData.path().toLocal8Bit());
    qunsetenv("XDG_CONFIG_HOME");
    QCOMPARE(setSettingsPathFromEnvironment(), appData.path());

    // the settings file and the old INI one both land inside it, and a write makes the file there
    QSettings json(JSON_FORMAT, QSettings::UserScope, "Darkride Settings Test", "Interface");
    QSettings ini(QSettings::IniFormat, QSettings::UserScope, "Darkride Settings Test", "Interface");
    QVERIFY2(json.fileName().startsWith(appData.path() + "/"), qPrintable(json.fileName()));
    QVERIFY2(ini.fileName().startsWith(appData.path() + "/"), qPrintable(ini.fileName()));
    QVERIFY(json.fileName().endsWith("Darkride Settings Test/Interface.json"));
    json.setValue("walled", true);
    json.sync();
    QCOMPARE(json.status(), QSettings::NoError);
    QVERIFY(QFile::exists(json.fileName()));
}
