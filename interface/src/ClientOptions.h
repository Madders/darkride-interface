//
//  ClientOptions.h
//  interface/src
//
//  The client's command line, parsed once at the top of main() and read anywhere after: every option the client
//  takes is declared here, so --help lists them all and no other code walks the arguments by hand (before, main
//  parsed eight of them, Application's start-up read thirteen more with getCmdOption, three places searched
//  arguments() for --tokens, and initPlugins ran a parser of its own). The meaning of each option is unchanged.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_ClientOptions_h
#define hifi_ClientOptions_h

#include <QtCore/QString>
#include <QtCore/QStringList>

class ClientOptions {
public:
    // Parses the client's arguments (argv as main() has them). An unknown option is printed and ignored, as it
    // always was, so a launcher that passes more than this client knows still starts it; --help and --version
    // print and exit here. Call once, before anything reads get().
    static void parse(int argc, const char* argv[]);
    static const ClientOptions& get();

    bool hasUrl { false };                 // --url <address>: where to go on start; handed to a running client when there is one
    QString url;
    bool allowMultipleInstances { false }; // --allowMultipleInstances, or HIFI_ALLOW_MULTIPLE_INSTANCES in the environment
    QString cacheDirectory;                // --cache <dir>: the local data path (caches) for this run
    QString scriptsPath;                   // --scripts <path>: the default scripts location
    bool hasTokens { false };              // --tokens <json>: a launcher's access tokens; the client starts signed in, with no sign-in screen
    QString tokens;
    QString displayName;                   // --displayName <name>
    QString setBookmark;                   // --setBookmark name=url: a location bookmark to add before the start
    QString defaultScriptsOverride;        // --defaultScriptsOverride <path>: another defaultScripts.js
    QString clientLabel;                   // --clientLabel <text>: first in the window title (see Application's clientLabel)
    QString traceFile;                     // --traceFile <file> [<seconds>]: trace from the start, written to Documents on exit...
    float traceDuration { 0.0f };          //   ...and quit after this many seconds, when a number follows the file
    QString protocolVersionFile;           // --protocolVersion <file>: write the protocol signature there and exit at once
    int listenPort { -1 };                 // --listenPort <port> for the node list; -1 = unset (the node list picks)
    bool standalone { false };             // --standalone: emulate a standalone device
    QString testScript;                    // --testScript <path or URL> [quitWhenFinished]: a test to run on start...
    bool testQuitWhenFinished { false };   //   ...quitting when it finishes, when that word follows the script
    QString testResultsLocation;           // --testResultsLocation <dir>: where the test writes its results (a writable directory)
    bool disableWatchdog { false };        // --disableWatchdog: no deadlock watchdog thread
    int concurrentDownloads { -1 };        // --concurrent-downloads <n>: the resource request limit; -1 = unset (the default)
    QString avatarUrl;                     // --avatarURL <url>: the avatar for this run
    QString replaceAvatarUrl;              // --replaceAvatarURL <url>: the avatar for this run, saved as the avatar
    bool systemCursor { false };           // --system-cursor: the system's cursor instead of the client's
    QStringList displays;                  // --display a,b: preferred display plug-ins, in order
    QStringList disabledDisplays;          // --disable-displays a,b
    QStringList disabledInputs;            // --disable-inputs a,b
};

#endif // hifi_ClientOptions_h
