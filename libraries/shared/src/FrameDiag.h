//
//  FrameDiag.h
//  libraries/shared/src
//
//  Per-frame diagnostics for the black-frame hunt of 2026-10-19 (KNOWN-ISSUES.md): with TIVOLI_FRAME_DIAG=<file> in
//  the environment, the render thread appends an R line per frame it builds and the present thread a P line per
//  frame it shows and a C line per frame it executes, each with the time in microseconds, so a screen recording's
//  black frames can be matched to what the engine did in them. Without the variable nothing is opened or written.
//  Temporary, to go when the cause is known.
//
#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace FrameDiag {
    // what the main view saw this frame; -1 until the job that sets it has run
    inline std::atomic<int>& mainOpaques() { static std::atomic<int> value { -1 }; return value; }
    inline std::atomic<int>& zones() { static std::atomic<int> value { -1 }; return value; }
    inline std::atomic<int>& sunLights() { static std::atomic<int> value { -1 }; return value; }
    inline std::atomic<int>& ambientLights() { static std::atomic<int> value { -1 }; return value; }
    inline std::atomic<int>& bake() { static std::atomic<int> value { -1 }; return value; }
    // the main view's lighting model switches, packed (MakeLightingModel::run)
    inline std::atomic<unsigned>& lightingFlags() { static std::atomic<unsigned> value { 0 }; return value; }

    inline FILE* file() {
        static FILE* handle = [] {
            const char* path = getenv("TIVOLI_FRAME_DIAG");
            if (!path || !*path) {
                return (FILE*)nullptr;
            }
            FILE* opened = fopen(path, "a");
            if (opened) {
                setvbuf(opened, nullptr, _IOLBF, 0);
            }
            return opened;
        }();
        return handle;
    }

    inline bool enabled() { return file() != nullptr; }

    inline void log(const char* format, ...) {
        FILE* handle = file();
        if (!handle) {
            return;
        }
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        va_list args;
        va_start(args, format);
        vfprintf(handle, format, args);
        va_end(args);
        fputc('\n', handle);
    }

    inline void beginFrame() {
        mainOpaques() = -1;
        zones() = -1;
        sunLights() = -1;
        ambientLights() = -1;
        bake() = -1;
    }

    // The present thread's record of the pipelines the backend switched to while executing a frame, as
    // "program:draws" pairs, the draws being those made with the pipeline before it; takePipelines ends the list with
    // the draws made after the last switch. Present thread only.
    inline std::string& pipelines() { static std::string value; return value; }
    inline unsigned& lastDraws() { static unsigned value = 0; return value; }
    inline void notePipeline(unsigned program, unsigned drawsSoFar) {
        if (!enabled()) {
            return;
        }
        char buffer[48];
        snprintf(buffer, sizeof(buffer), "%s%u:%u", pipelines().empty() ? "" : ",", program, drawsSoFar - lastDraws());
        lastDraws() = drawsSoFar;
        pipelines() += buffer;
    }
    inline std::string takePipelines(unsigned drawsNow) {
        std::string out = pipelines();
        char buffer[24];
        snprintf(buffer, sizeof(buffer), ":%u", drawsNow - lastDraws());
        out += buffer;
        pipelines().clear();
        lastDraws() = drawsNow;
        return out;
    }
}
