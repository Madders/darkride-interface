//
//  StartupProfiler.cpp
//  libraries/shared/src
//
//  See StartupProfiler.h. The entries live in one vector behind a mutex; each thread keeps its own nesting depth,
//  and the phases (the top-level rows of the long constructors) are the main thread's. The table lists the rows in
//  the order they began, indented by depth, then the ten steps whose own time (their duration less their direct
//  children's) was longest, which is where a later removal will show.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "StartupProfiler.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <vector>

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>

#if defined(__GNUG__) || defined(__clang__)
#include <cxxabi.h>
#endif
#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <Windows.h>
#endif

Q_LOGGING_CATEGORY(startup_profile, "hifi.startup")

namespace {

struct Entry {
    QString name;
    qint64 startUsecs;
    qint64 durationUsecs;   // -1 while the step is open
    int depth;
    bool instant;
};

// A record that kept growing would be a leak in a process that registers services all its life, so a start-up
// that somehow never prints its table stops recording at this many rows.
const size_t MAX_ENTRIES = 4096;

std::mutex entriesMutex;
std::vector<Entry> entries;
QElapsedTimer startClock;
std::atomic<bool> started { false };
std::atomic<bool> recordingFlag { false };
int openPhase = -1;          // the index of the open top-level phase; main thread only
qint64 processStartToMainMsecs = -1;
thread_local int nesting = 0;

QString typeName(const std::type_info& type) {
#if defined(__GNUG__) || defined(__clang__)
    int status = 0;
    char* demangled = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    QString name = (status == 0 && demangled) ? QString::fromLatin1(demangled) : QString::fromLatin1(type.name());
    std::free(demangled);
    return name;
#else
    QString name = QString::fromLatin1(type.name());   // MSVC: "class Foo" or "struct Foo"
    if (name.startsWith(QStringLiteral("class "))) {
        return name.mid(6);
    }
    if (name.startsWith(QStringLiteral("struct "))) {
        return name.mid(7);
    }
    return name;
#endif
}

// How long the process had lived when start() ran: the dynamic linker's and the runtime's share of a launch, which
// no timer inside main() sees. -1 where the platform does not say.
qint64 msecsFromProcessStart() {
#if defined(Q_OS_MACOS)
    struct proc_bsdinfo info;
    if (proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info)) == (int)sizeof(info)) {
        struct timeval now;
        gettimeofday(&now, nullptr);
        qint64 nowUsecs = (qint64)now.tv_sec * 1000000 + now.tv_usec;
        qint64 startUsecs = (qint64)info.pbi_start_tvsec * 1000000 + (qint64)info.pbi_start_tvusec;
        return (nowUsecs - startUsecs) / 1000;
    }
#elif defined(Q_OS_WIN)
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        FILETIME nowFileTime;
        GetSystemTimeAsFileTime(&nowFileTime);
        ULARGE_INTEGER createdTicks, nowTicks;   // 100 ns ticks
        createdTicks.LowPart = created.dwLowDateTime;
        createdTicks.HighPart = created.dwHighDateTime;
        nowTicks.LowPart = nowFileTime.dwLowDateTime;
        nowTicks.HighPart = nowFileTime.dwHighDateTime;
        return (qint64)((nowTicks.QuadPart - createdTicks.QuadPart) / 10000);
    }
#endif
    return -1;
}

int addEntry(const QString& name, int depth, bool instant) {
    std::lock_guard<std::mutex> lock(entriesMutex);
    if (entries.size() >= MAX_ENTRIES) {
        recordingFlag = false;
        return -1;
    }
    Entry entry;
    entry.name = name;
    entry.startUsecs = startClock.nsecsElapsed() / 1000;
    entry.durationUsecs = instant ? 0 : -1;
    entry.depth = depth;
    entry.instant = instant;
    entries.push_back(entry);
    return (int)entries.size() - 1;
}

void closeEntry(int handle) {
    std::lock_guard<std::mutex> lock(entriesMutex);
    if (handle >= 0 && handle < (int)entries.size() && entries[handle].durationUsecs < 0) {
        entries[handle].durationUsecs = startClock.nsecsElapsed() / 1000 - entries[handle].startUsecs;
    }
}

}   // namespace

void StartupProfiler::start() {
    if (started.exchange(true)) {
        return;
    }
    startClock.start();
    processStartToMainMsecs = msecsFromProcessStart();
    recordingFlag = true;
}

bool StartupProfiler::recording() {
    return recordingFlag.load(std::memory_order_relaxed);
}

qint64 StartupProfiler::elapsedUsecs() {
    return started ? startClock.nsecsElapsed() / 1000 : 0;
}

int StartupProfiler::begin(const char* name) {
    if (!recording()) {
        return -1;
    }
    int handle = addEntry(QString::fromLatin1(name), (openPhase >= 0 ? 1 : 0) + nesting, false);
    if (handle >= 0) {
        ++nesting;
    }
    return handle;
}

int StartupProfiler::begin(const std::type_info& type) {
    if (!recording()) {
        return -1;
    }
    int handle = addEntry(typeName(type), (openPhase >= 0 ? 1 : 0) + nesting, false);
    if (handle >= 0) {
        ++nesting;
    }
    return handle;
}

void StartupProfiler::end(int handle) {
    if (handle < 0) {
        return;
    }
    if (nesting > 0) {
        --nesting;
    }
    closeEntry(handle);
}

void StartupProfiler::phase(const char* name) {
    if (!recording()) {
        return;
    }
    endPhases();
    openPhase = addEntry(QString::fromLatin1(name), 0, false);
}

void StartupProfiler::endPhases() {
    if (openPhase >= 0) {
        closeEntry(openPhase);
        openPhase = -1;
    }
}

void StartupProfiler::instant(const char* name) {
    if (!started) {
        return;
    }
    if (recording()) {
        addEntry(QString::fromLatin1(name), (openPhase >= 0 ? 1 : 0) + nesting, true);
        return;
    }
    qCDebug(startup_profile).noquote() << QStringLiteral("Startup profile: %1 at %2 s")
        .arg(QString::fromLatin1(name)).arg(elapsedUsecs() / 1.0e6, 0, 'f', 3);
}

void StartupProfiler::logReport() {
    if (!started) {
        return;
    }
    endPhases();
    recordingFlag = false;
    std::vector<Entry> rows;
    {
        std::lock_guard<std::mutex> lock(entriesMutex);
        rows = entries;
    }
    qint64 nowUsecs = elapsedUsecs();
    // own time: a step's duration less its direct children's (the rows one level deeper that began inside it)
    std::vector<qint64> ownUsecs(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        const Entry& row = rows[i];
        qint64 duration = row.durationUsecs < 0 ? nowUsecs - row.startUsecs : row.durationUsecs;
        qint64 own = duration;
        for (size_t j = i + 1; j < rows.size() && rows[j].startUsecs < row.startUsecs + duration; ++j) {
            if (rows[j].depth == row.depth + 1 && rows[j].durationUsecs > 0) {
                own -= rows[j].durationUsecs;
            }
        }
        ownUsecs[i] = own < 0 ? 0 : own;
    }

    auto log = [](const QString& line) { qCDebug(startup_profile).noquote() << line; };
    QString beforeMain = processStartToMainMsecs >= 0
        ? QStringLiteral("the process had lived %1 ms before main()").arg(processStartToMainMsecs)
        : QStringLiteral("the time before main() is unknown on this platform");
    log(QStringLiteral("Startup profile: %1 rows, %2 s from main() to the event loop; %3. Columns: at (ms), took (ms), "
                       "step; a step is indented under the one it ran inside; a registration row is that service's "
                       "constructor.")
            .arg((int)rows.size()).arg(nowUsecs / 1.0e6, 0, 'f', 3).arg(beforeMain));
    log(QStringLiteral("%1 %2  %3").arg(QStringLiteral("at"), 9).arg(QStringLiteral("took"), 9).arg(QStringLiteral("step")));
    for (size_t i = 0; i < rows.size(); ++i) {
        const Entry& row = rows[i];
        QString took;
        if (row.instant) {
            took = QStringLiteral("%1").arg(QStringLiteral("-"), 9);
        } else if (row.durationUsecs < 0) {
            took = QStringLiteral("%1").arg(QStringLiteral("open"), 9);
        } else {
            took = QStringLiteral("%1").arg(row.durationUsecs / 1000.0, 9, 'f', 1);
        }
        log(QStringLiteral("%1 %2  %3%4").arg(row.startUsecs / 1000.0, 9, 'f', 1).arg(took)
                .arg(QString(row.depth * 2, QChar(' '))).arg(row.name));
    }

    std::vector<size_t> order;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (!rows[i].instant) {
            order.push_back(i);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return ownUsecs[a] > ownUsecs[b]; });
    if (order.size() > 10) {
        order.resize(10);
    }
    log(QStringLiteral("Startup profile, the ten steps with the most time of their own (ms):"));
    for (size_t i : order) {
        log(QStringLiteral("%1  %2").arg(ownUsecs[i] / 1000.0, 9, 'f', 1).arg(rows[i].name));
    }
}
