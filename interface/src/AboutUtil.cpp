//
//  AboutUtil.cpp
//  interface/src
//
//  Created by Vlad Stelmahovsky on 15/5/2018.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AboutUtil.h"
#include <QDate>
#include <QLocale>

#include <ui/TabletScriptingInterface.h>
#include <OffscreenQmlDialog.h>

#include "BuildInfo.h"
#include "DependencyManager.h"
#include "scripting/HMDScriptingInterface.h"
#include "Application.h"

AboutUtil::AboutUtil(QObject *parent) : QObject(parent) {
    QLocale locale;
    _dateConverted = QDate::fromString(BuildInfo::BUILD_TIME, "dd/MM/yyyy").
            toString(locale.dateFormat(QLocale::ShortFormat));
}

AboutUtil *AboutUtil::getInstance() {
    static AboutUtil instance;
    return &instance;
}

QString AboutUtil::getBuildDate() const {
    return _dateConverted;
}

QString AboutUtil::getBuildVersion() const {
    return BuildInfo::VERSION;
}

QString AboutUtil::getBuildCommit() const {
    return BuildInfo::BUILD_COMMIT;
}

QString AboutUtil::getBuildLine() const {
    // the commit names a build exactly; a build number where there is no commit (a tree outside git)
    const QString build = !BuildInfo::BUILD_COMMIT.isEmpty() ? BuildInfo::BUILD_COMMIT : BuildInfo::BUILD_NUMBER;
    // PR and nightly builds append "-<commit>" to the version (cmake/macros/SetPackagingParameters.cmake), which this
    // line shows as the build already, or a bare "-" when the packager passes no commit ("PR1-"); a suffix of the
    // version's own ("-beta") stays
    QString version = BuildInfo::VERSION;
    const int dash = version.lastIndexOf('-');
    if (dash > 0) {
        const QString tail = version.mid(dash + 1);
        if (tail.isEmpty() || (!build.isEmpty() && (build.startsWith(tail) || tail.startsWith(build)))) {
            version.truncate(dash);
        }
    }
    return "Darkride " + version + (build.isEmpty() ? QString() : QString::fromUtf8(" \u00b7 build ") + build);
}

QString AboutUtil::getQtVersion() const {
    return qVersion();
}


