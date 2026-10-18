//
//  AAudioOutput.cpp
//  libraries/audio-client/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AAudioOutput.h"

#if defined(Q_OS_ANDROID)

#include <aaudio/AAudio.h>
#include <cstring>

#include "AudioClientLogging.h"

// Each read: 10 ms, like a network frame
static const int CALLBACK_MSECS = 10;
// Buffered ahead to start with, in bursts, and the most it grows to
static const int START_BUFFER_BURSTS = 4;
static const int MAX_BUFFER_MSECS = 120;
// How often (in callbacks) to check for underruns
static const int XRUN_CHECK_CALLBACKS = 50;

AAudioOutput::~AAudioOutput() {
    stop();
}

bool AAudioOutput::start(int sampleRate, int channelCount, QIODevice* source, ErrorHandler onError) {
    stop();
    _source = source;
    _onError = onError;

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        qCWarning(audioclient) << "AAudio: couldn't make a stream builder";
        return false;
    }
    _framesPerCallback = sampleRate * CALLBACK_MSECS / 1000;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, channelCount);
    AAudioStreamBuilder_setSampleRate(builder, sampleRate);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFramesPerDataCallback(builder, _framesPerCallback);
    AAudioStreamBuilder_setDataCallback(builder,
        [](AAudioStream* stream, void* userData, void* audioData, int32_t numFrames) -> aaudio_data_callback_result_t {
            return AAudioOutput::dataCallback(stream, userData, audioData, numFrames);
        }, this);
    AAudioStreamBuilder_setErrorCallback(builder,
        [](AAudioStream* stream, void* userData, aaudio_result_t error) {
            AAudioOutput::errorCallback(stream, userData, error);
        }, this);

    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &_stream);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK) {
        qCWarning(audioclient) << "AAudio: couldn't open a stream:" << AAudio_convertResultToText(result);
        _stream = nullptr;
        return false;
    }
    _sampleRate = AAudioStream_getSampleRate(_stream);
    _channelCount = AAudioStream_getChannelCount(_stream);
    _burstFrames = AAudioStream_getFramesPerBurst(_stream);
    AAudioStream_setBufferSizeInFrames(_stream, _burstFrames * START_BUFFER_BURSTS);
    _lastXruns = 0;
    _callbacksSinceCheck = 0;

    result = AAudioStream_requestStart(_stream);
    if (result != AAUDIO_OK) {
        qCWarning(audioclient) << "AAudio: couldn't start the stream:" << AAudio_convertResultToText(result);
        stop();
        return false;
    }
    qCDebug(audioclient) << "AAudio output:" << _sampleRate << "Hz," << _channelCount << "channels, burst" << _burstFrames
                         << "frames, buffer" << AAudioStream_getBufferSizeInFrames(_stream) << "of"
                         << AAudioStream_getBufferCapacityInFrames(_stream) << "frames, reads of" << _framesPerCallback
                         << "frames, performance mode" << AAudioStream_getPerformanceMode(_stream);
    return true;
}

void AAudioOutput::stop() {
    if (_stream) {
        // (stopping waits for a callback in progress)
        AAudioStream_requestStop(_stream);
        AAudioStream_close(_stream);
        _stream = nullptr;
    }
}

int AAudioOutput::bufferFrames() const {
    return _stream ? AAudioStream_getBufferSizeInFrames(_stream) : 0;
}

int AAudioOutput::xrunCount() const {
    return _stream ? AAudioStream_getXRunCount(_stream) : 0;
}

int AAudioOutput::dataCallback(AAudioStreamStruct* stream, void* userData, void* audioData, int32_t numFrames) {
    auto output = static_cast<AAudioOutput*>(userData);
    qint64 bytes = (qint64)numFrames * output->_channelCount * (qint64)sizeof(int16_t);
    char* data = static_cast<char*>(audioData);
    qint64 filled = 0;
    // (readData may give a little less than asked: it's asked again for the rest, and silence makes up anything left)
    for (int attempt = 0; attempt < 4 && filled < bytes; attempt++) {
        qint64 read = output->_source->read(data + filled, bytes - filled);
        if (read <= 0) {
            break;
        }
        filled += read;
    }
    if (filled < bytes) {
        memset(data + filled, 0, bytes - filled);
    }
    if (++output->_callbacksSinceCheck >= XRUN_CHECK_CALLBACKS) {
        output->_callbacksSinceCheck = 0;
        output->growBufferIfUnderrunning();
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// Google's advice for low-latency output: start with a small buffer, and add a burst whenever it underruns
void AAudioOutput::growBufferIfUnderrunning() {
    int xruns = AAudioStream_getXRunCount(_stream);
    if (xruns > _lastXruns) {
        _lastXruns = xruns;
        int size = AAudioStream_getBufferSizeInFrames(_stream);
        int max = std::min(AAudioStream_getBufferCapacityInFrames(_stream), _sampleRate * MAX_BUFFER_MSECS / 1000);
        if (size + _burstFrames <= max) {
            AAudioStream_setBufferSizeInFrames(_stream, size + _burstFrames);
        }
    }
}

void AAudioOutput::errorCallback(AAudioStreamStruct* stream, void* userData, int32_t error) {
    auto output = static_cast<AAudioOutput*>(userData);
    qCWarning(audioclient) << "AAudio: the output stopped:" << AAudio_convertResultToText(error);
    if (output->_onError) {
        output->_onError();
    }
}

#endif
