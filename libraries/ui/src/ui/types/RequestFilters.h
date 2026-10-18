//
//  RequestFilters.h
//  interface/src/networking
//
//  Created by Kunal Gosar on 2017-03-10.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#ifndef hifi_RequestFilters_h
#define hifi_RequestFilters_h

#include <QtCore/QtGlobal>

#if !defined(Q_OS_ANDROID)
#include <QObject>
#include <QWebEngineUrlRequestInfo>

class QQmlContext;
class QQuickWebEngineProfile;

class RequestFilters : public QObject {
public:
    static void interceptTivoliWebEngineRequest(QWebEngineUrlRequestInfo& info, bool restricted);
    static void interceptFileType(QWebEngineUrlRequestInfo& info);

    // Sites in the profile's web views get no Cache Storage and no service workers (see RequestFilters.cpp):
    // a script removes the APIs from every page and frame, and the service workers and Cache Storage that sites
    // stored in earlier runs are deleted, once per run, before the first page loads.
    static void disableServiceWorkers(QQuickWebEngineProfile* profile);
};
#endif

#endif // hifi_RequestFilters_h
