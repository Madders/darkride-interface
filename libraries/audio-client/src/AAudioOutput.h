//
//  AAudioOutput.h
//  libraries/audio-client/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_ANDROID)

#include <atomic>
#include <functional>

#include <QtCore/QIODevice>

struct AAudioStreamStruct;

// Android's sound output through AAudio, for AudioClient instead of Qt's OpenSL ES output: that output feeds a
// low-latency track with an 8 ms buffer in 40 ms lumps from two buffers, which underruns (clicks) whenever the phone is
// a little late. AAudio asks for small, regular amounts, at the device's own rate, from a buffer this sizes from the
// device's burst and grows if it underruns.
class AAudioOutput {
public:
    // The device lost (headphones unplugged, Bluetooth gone...): called on AAudio's thread, to restart the output
    using ErrorHandler = std::function<void()>;

    AAudioOutput() {}
    ~AAudioOutput();

    // Starts playing 16-bit samples read from source; false if AAudio won't open a stream
    bool start(int sampleRate, int channelCount, QIODevice* source, ErrorHandler onError);
    void stop();

    int sampleRate() const { return _sampleRate; }
    int channelCount() const { return _channelCount; }
    // the most frames a read asks for (AAudio's callback size)
    int framesPerCallback() const { return _framesPerCallback; }
    int bufferFrames() const;
    // underruns since it started
    int xrunCount() const;

private:
    static int dataCallback(AAudioStreamStruct* stream, void* userData, void* audioData, int32_t numFrames);
    static void errorCallback(AAudioStreamStruct* stream, void* userData, int32_t error);
    void growBufferIfUnderrunning();

    AAudioStreamStruct* _stream { nullptr };
    QIODevice* _source { nullptr };
    ErrorHandler _onError;
    int _sampleRate { 0 };
    int _channelCount { 0 };
    int _framesPerCallback { 0 };
    int _burstFrames { 0 };
    int _lastXruns { 0 };
    int _callbacksSinceCheck { 0 };
};

#endif
