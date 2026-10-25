//
//  main.cpp
//  interface/src
//
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <thread>

#include <QDebug>
#include <QDir>
#include <QThreadPool>
#include <QTranslator>
#include <QtNetwork/QSslConfiguration>

#include <BuildInfo.h>
#include <SharedUtil.h>
#include <StartupProfiler.h>
#include <NetworkAccessManager.h>
#include <gl/GLHelpers.h>

#include "AddressManager.h"
#include "Application.h"
#include "ClientOptions.h"
#include "InterfaceLogging.h"
#include "MainWindow.h"
#include "SingleInstance.h"

#include "Profile.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_MACOS
#include "MacAutoFill.h"
#endif

int main(int argc, const char* argv[]) {
    StartupProfiler::start();
    StartupProfiler::phase("main: surface format, application name, command line");
#ifdef Q_OS_MACOS
    // before anything of AppKit's runs: AutoFill is set up when the app first becomes active
    disableMacAutoFill();

    auto format = getDefaultOpenGLSurfaceFormat();
    // Deal with some weirdness in the chromium context sharing on Mac.
    // The primary share context needs to be 3.2, so that the Chromium will
    // succeed in it's creation of it's command stub contexts.  
    format.setVersion(3, 2);
    // This appears to resolve the issues with corrupted fonts on OSX.  No
    // idea why.
    qputenv("QT_ENABLE_GLYPH_CACHE_WORKAROUND", "true");
	// https://i.kym-cdn.com/entries/icons/original/000/008/342/ihave.jpg
    QSurfaceFormat::setDefaultFormat(format);
#endif

#if defined(Q_OS_WIN) 
    // Check the minimum version of 
    if (gl::getAvailableVersion() < gl::getRequiredVersion()) {
        MessageBoxA(nullptr, "Darkride requires OpenGL 4.1 or higher", "Unsupported", MB_OK);
        return -1;
    }
#endif

    setupHifiApplication(BuildInfo::INTERFACE_NAME);

    ClientOptions::parse(argc, argv);
    const ClientOptions& options = ClientOptions::get();

    StartupProfiler::phase("main: tracer");
    auto tracer = DependencyManager::set<tracing::Tracer>();
    if (!options.traceFile.isEmpty()) {
        tracer->startTracing();
    }
   
    PROFILE_SYNC_BEGIN(startup, "main startup", "");

    QElapsedTimer startupTime;
    startupTime.start();

    StartupProfiler::phase("main: settings (Setting::init)");
    Setting::init();

    // The SSL backend's first use loads OpenSSL and the system's root certificates, about 130 ms on this Mac, and it
    // used to fall on the main thread at start-up: inside the keypair request in a fresh home, inside the profile
    // request in a signed-in one. Warmed on a pool thread here, beside the rest of the start; a first use that
    // comes sooner waits on the backend's own lock.
    QThreadPool::globalInstance()->start([] { QSslConfiguration::defaultConfiguration(); });



    StartupProfiler::phase("main: single-instance check, scripts path");
    SingleInstance singleInstance(options.allowMultipleInstances, options.hasUrl, options.url);
    if (singleInstance.anotherIsRunning()) {
        return EXIT_SUCCESS;
    }
    // this needs to be done here in main, as the mechanism for setting the
    // scripts directory appears not to work.  See the bug report
    // https://highfidelity.fogbugz.com/f/cases/5759/Issues-changing-scripts-directory-in-ScriptsEngine
    if (!options.scriptsPath.isEmpty()) {
        QDir scriptsPath(options.scriptsPath);
        if (scriptsPath.exists()) {
            PathUtils::defaultScriptsLocation(scriptsPath.path());
        }
    }


    StartupProfiler::phase("main: Application::initPlugins");
    // the preferred and disabled plug-ins, before the Application constructor creates the GL context and activates them
    Application::initPlugins(options);

    int exitCode;
    {
        StartupProfiler::phase("main: running marker, arguments");
        RunningMarker runningMarker(RUNNING_MARKER_FILENAME);
        bool runningMarkerExisted = runningMarker.fileExists();
        runningMarker.writeRunningMarkerFile();

        // Extend argv to enable WebGL rendering
        std::vector<const char*> argvExtended(&argv[0], &argv[argc]);
        argvExtended.push_back("--ignore-gpu-blacklist");
        int argcExtended = (int)argvExtended.size();

        PROFILE_SYNC_END(startup, "main startup", "");
        PROFILE_SYNC_BEGIN(startup, "app full ctor", "");
        StartupProfiler::phase("Application: QApplication, main window, file logger");
        Application app(argcExtended, const_cast<char**>(argvExtended.data()), startupTime, runningMarkerExisted);
        PROFILE_SYNC_END(startup, "app full ctor", "");

        StartupProfiler::phase("main: local server, system information, translator");

        QTimer exitTimer;
        if (options.traceDuration > 0.0f) {
            exitTimer.setSingleShot(true);
            QObject::connect(&exitTimer, &QTimer::timeout, &app, &Application::quit);
            exitTimer.start(int(1000 * options.traceDuration));
        }


        // later launches hand their address to this client (SingleInstance)
        QObject::connect(&singleInstance, &SingleInstance::connected, &app.getDisplay(), &Display::raiseWindow);
        QObject::connect(&singleInstance, &SingleInstance::urlHandedOff, &app.getDomainSession(), &DomainSession::goToHandedOffUrl);
        singleInstance.listen();

        printSystemInformation();

        if (options.hasUrl) {
            app.overrideEntry();
        }
        if (!options.displayName.isEmpty()) {
            app.forceDisplayName(options.displayName);
        }
        if (options.hasTokens) {
            app.forceLoginWithTokens(options.tokens);
        }

        QTranslator translator;
        translator.load("i18n/interface_en");
        app.installTranslator(&translator);
        qCDebug(interfaceapp, "Created QT Application.");
        StartupProfiler::logReport();
        exitCode = app.exec();
        singleInstance.close();

        if (!options.traceFile.isEmpty()) {
            tracer->stopTracing();
            // a relative name lands in Documents (FileUtils::computeDocumentPath), an absolute path where it says
            tracer->serialize(options.traceFile);
        }
    }

    qCDebug(interfaceapp, "Normal exit.");
#if !defined(DEBUG) && !defined(Q_OS_LINUX)
    // HACK: exit immediately (don't handle shutdown callbacks) for Release build
    _exit(exitCode);
#endif
    return exitCode;
}
