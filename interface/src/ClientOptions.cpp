//
//  ClientOptions.cpp
//  interface/src
//
//  See ClientOptions.h. One QCommandLineParser, in parse() rather than process(), so that an unknown option is
//  printed and tolerated as before. Two options keep their old two-word forms (--traceFile <file> <seconds>,
//  --testScript <script> quitWhenFinished): the parser takes the first word as the value and the second lands among
//  the positional arguments, so those are read from the raw list, exactly where they stood.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "ClientOptions.h"

#include <iostream>

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QProcessEnvironment>

namespace {

ClientOptions parsedOptions;
bool parsed = false;

QStringList commaSeparated(const QString& value) {
    return value.split(',', Qt::SkipEmptyParts);
}

}   // namespace

const ClientOptions& ClientOptions::get() {
    Q_ASSERT_X(parsed, "ClientOptions::get", "the command line is read before main() parsed it");
    return parsedOptions;
}

void ClientOptions::parse(int argc, const char* argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << argv[i];
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Darkride");
    QCommandLineOption versionOption = parser.addVersionOption();
    QCommandLineOption helpOption = parser.addHelpOption();

    QCommandLineOption urlOption("url", "go to this address on start (a running client is sent there instead)", "address");
    QCommandLineOption allowMultipleInstancesOption("allowMultipleInstances", "start even when a client is running");
    QCommandLineOption cacheOption("cache", "the local data path (caches) for this run", "dir");
    QCommandLineOption scriptsOption("scripts", "the default scripts location", "path");
    QCommandLineOption tokensOption("tokens", "a launcher's access tokens: start signed in, with no sign-in screen", "json");
    QCommandLineOption displayNameOption("displayName", "the avatar's display name", "name");
    QCommandLineOption setBookmarkOption("setBookmark", "add a location bookmark before the start", "name=url");
    QCommandLineOption defaultScriptsOverrideOption("defaultScriptsOverride", "another defaultScripts.js", "path");
    QCommandLineOption clientLabelOption("clientLabel", "a label first in the window title", "text");
    QCommandLineOption traceFileOption("traceFile", "trace from the start into this file in Documents; a number of seconds may follow, to quit after", "file");
    QCommandLineOption protocolVersionOption("protocolVersion", "write the protocol signature to this file and exit", "file");
    QCommandLineOption listenPortOption("listenPort", "the node list's port", "port");
    QCommandLineOption standaloneOption("standalone", "emulate a standalone device");
    QCommandLineOption testScriptOption("testScript", "run this test script on start; the word quitWhenFinished may follow", "script");
    QCommandLineOption testResultsLocationOption("testResultsLocation", "where the test script writes its results", "dir");
    QCommandLineOption disableWatchdogOption("disableWatchdog", "no deadlock watchdog thread");
    QCommandLineOption concurrentDownloadsOption("concurrent-downloads", "the resource request limit", "n");
    QCommandLineOption avatarUrlOption("avatarURL", "the avatar for this run", "url");
    QCommandLineOption replaceAvatarUrlOption("replaceAvatarURL", "the avatar for this run, saved as the avatar", "url");
    QCommandLineOption systemCursorOption("system-cursor", "the system's cursor instead of the client's");
    QCommandLineOption displayOption("display", "preferred display plug-ins, in order", "a,b");
    QCommandLineOption disableDisplaysOption("disable-displays", "display plug-ins to leave out", "a,b");
    QCommandLineOption disableInputsOption("disable-inputs", "input plug-ins to leave out", "a,b");

    for (const auto& option : { urlOption, allowMultipleInstancesOption, cacheOption, scriptsOption, tokensOption,
                                displayNameOption, setBookmarkOption, defaultScriptsOverrideOption, clientLabelOption,
                                traceFileOption, protocolVersionOption, listenPortOption, standaloneOption,
                                testScriptOption, testResultsLocationOption, disableWatchdogOption,
                                concurrentDownloadsOption, avatarUrlOption, replaceAvatarUrlOption, systemCursorOption,
                                displayOption, disableDisplaysOption, disableInputsOption }) {
        parser.addOption(option);
    }

    if (!parser.parse(arguments)) {
        std::cout << parser.errorText().toStdString() << std::endl;   // as before: said, and the start goes on
    }
    if (parser.isSet(versionOption)) {
        parser.showVersion();
        Q_UNREACHABLE();
    }
    if (parser.isSet(helpOption)) {
        QCoreApplication mockApp(argc, const_cast<char**>(argv));   // showHelp() wants the application's name
        parser.showHelp();
        Q_UNREACHABLE();
    }

    ClientOptions options;
    options.hasUrl = parser.isSet(urlOption);
    options.url = parser.value(urlOption);
    options.allowMultipleInstances = parser.isSet(allowMultipleInstancesOption) ||
        QProcessEnvironment::systemEnvironment().contains("HIFI_ALLOW_MULTIPLE_INSTANCES");
    options.cacheDirectory = parser.value(cacheOption);
    options.scriptsPath = parser.value(scriptsOption);
    options.hasTokens = parser.isSet(tokensOption);
    options.tokens = parser.value(tokensOption);
    options.displayName = parser.value(displayNameOption);
    options.setBookmark = parser.value(setBookmarkOption);
    options.defaultScriptsOverride = parser.value(defaultScriptsOverrideOption);
    options.clientLabel = parser.value(clientLabelOption).trimmed();
    options.traceFile = parser.value(traceFileOption);
    options.protocolVersionFile = parser.value(protocolVersionOption);
    if (parser.isSet(listenPortOption)) {
        options.listenPort = parser.value(listenPortOption).toInt();   // atoi before: a bad value is 0, as it was
    }
    options.standalone = parser.isSet(standaloneOption);
    options.testScript = parser.value(testScriptOption);
    options.testResultsLocation = parser.value(testResultsLocationOption);
    options.disableWatchdog = parser.isSet(disableWatchdogOption);
    if (parser.isSet(concurrentDownloadsOption)) {
        bool ok = false;
        uint32_t limit = parser.value(concurrentDownloadsOption).toUInt(&ok);
        if (ok) {
            options.concurrentDownloads = (int)limit;
        }
    }
    options.avatarUrl = parser.value(avatarUrlOption);
    options.replaceAvatarUrl = parser.value(replaceAvatarUrlOption);
    options.systemCursor = parser.isSet(systemCursorOption);
    options.displays = commaSeparated(parser.value(displayOption));
    options.disabledDisplays = commaSeparated(parser.value(disableDisplaysOption));
    options.disabledInputs = commaSeparated(parser.value(disableInputsOption));

    // the two-word forms: the word after the option's value, where it has always stood
    int at = arguments.indexOf("--traceFile");
    if (at >= 0 && at + 2 < arguments.size()) {
        bool ok = false;
        float seconds = arguments.at(at + 2).toFloat(&ok);
        if (ok) {
            options.traceDuration = seconds;
        }
    }
    at = arguments.indexOf("--testScript");
    if (at >= 0 && at + 2 < arguments.size() && arguments.at(at + 2) == "quitWhenFinished") {
        options.testQuitWhenFinished = true;
    }

    parsedOptions = options;
    parsed = true;
}
