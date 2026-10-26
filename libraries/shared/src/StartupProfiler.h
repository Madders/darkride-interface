//
//  StartupProfiler.h
//  libraries/shared/src
//
//  A record of where the start-up time goes, on one clock that starts at the top of main(): every service the
//  DependencyManager constructs (measured inside set<T>, so the registration list needs no lines of its own), each
//  step of main() and of the Application constructor, and the moments after them (the sign-in dialog, the first
//  frame, the domain connection, the world). The client prints the record as one table before it enters its event
//  loop, beside the old "Startup time" line (ARCHITECTURE.md, section 5.4, step 1); a moment that comes after the
//  table has been printed is logged as a line of its own, so a start-up's log carries the whole timeline. Nothing
//  is recorded in a process that never calls start() (the servers), and nothing after the table is printed: a
//  DependencyManager::set at run time costs one atomic load.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_StartupProfiler_h
#define hifi_StartupProfiler_h

#include <typeinfo>

#include <QtCore/QLoggingCategory>
#include <QtCore/QString>

Q_DECLARE_LOGGING_CATEGORY(startup_profile)

class StartupProfiler {
public:
    // Starts the clock and the recording. The first line of main(); everything is measured from here.
    static void start();
    // Whether steps are being recorded: after start() and before logReport().
    static bool recording();
    // Microseconds since start(); 0 before it.
    static qint64 elapsedUsecs();

    // A step with a duration: begin() returns the handle that end() takes. Steps nest, per thread.
    static int begin(const char* name);
    static int begin(const std::type_info& type);   // named after the type, demangled (DependencyManager::set)
    static void end(int handle);

    // The consecutive steps of one long function, on the main thread: phase() ends the phase that is open and
    // begins the next at the top level, so a block of a constructor needs one line, not a scope and two braces.
    // endPhases() closes the last one; logReport() does that too.
    static void phase(const char* name);
    static void endPhases();

    // A moment without a duration. A row of the table while recording; logged at once after the table.
    static void instant(const char* name);

    // Prints the table (one log line per row, category hifi.startup) and stops the recording.
    static void logReport();

    // A step as a scope: for a block, or around one call.
    class Scope {
    public:
        explicit Scope(const char* name) : _handle(begin(name)) {}
        explicit Scope(const std::type_info& type) : _handle(begin(type)) {}
        ~Scope() { end(_handle); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        int _handle;
    };
};

#endif // hifi_StartupProfiler_h
