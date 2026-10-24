//
//  OpusCodec.cpp
//  plugins/opus-codec-plugin/src
//
//  See OpusCodec.h.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "OpusCodec.h"

#include <atomic>
#include <cstring>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>

#include <opus.h>

#include <AudioConstants.h>

Q_LOGGING_CATEGORY(opuscodec, "hifi.codec.opus")

const char* OpusCodec::NAME { "opus" };

namespace {

// a voice from a microphone, and a listener's mix (see OpusCodec.h for the measurements behind these)
const int MONO_BITRATE = 32000;
const int MONO_COMPLEXITY = 10;
const int STEREO_BITRATE = 64000;
const int STEREO_COMPLEXITY = 5;
const int MAX_PACKET_BYTES = 1275;   // the largest Opus packet for one frame (RFC 6716 3.2.1)

int frameSamples() {
    return AudioConstants::NETWORK_FRAME_SAMPLES_PER_CHANNEL;
}

// A warning per kind of failure, the first few times only: a stream that keeps failing would fill the log every 10 ms.
void warnSometimes(std::atomic<int>& count, const char* what, int error) {
    const int MAX_WARNINGS = 5;
    if (count.fetch_add(1) < MAX_WARNINGS) {
        qCWarning(opuscodec) << what << opus_strerror(error);
    }
}
std::atomic<int> encodeFailures { 0 };
std::atomic<int> decodeFailures { 0 };

class OpusEncoderStream : public Encoder {
public:
    OpusEncoderStream(int sampleRate, int numChannels) : _numChannels(numChannels) {
        int error = OPUS_OK;
        _encoder = opus_encoder_create(sampleRate, numChannels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
        if (error != OPUS_OK || !_encoder) {
            qCWarning(opuscodec) << "encoder not created for" << sampleRate << "Hz," << numChannels << "channels:"
                << opus_strerror(error);
            _encoder = nullptr;
            return;
        }
        bool mono = numChannels == 1;
        opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(mono ? MONO_BITRATE : STEREO_BITRATE));
        opus_encoder_ctl(_encoder, OPUS_SET_COMPLEXITY(mono ? MONO_COMPLEXITY : STEREO_COMPLEXITY));
        opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(mono ? OPUS_SIGNAL_VOICE : OPUS_AUTO));
    }
    ~OpusEncoderStream() override {
        if (_encoder) {
            opus_encoder_destroy(_encoder);
        }
    }

    void encode(const QByteArray& decodedBuffer, QByteArray& encodedBuffer) override {
        const int samples = decodedBuffer.size() / (int)(sizeof(opus_int16) * _numChannels);
        if (!_encoder) {
            encodedBuffer.clear();
            return;
        }
        encodedBuffer.resize(MAX_PACKET_BYTES);
        int bytes = opus_encode(_encoder, reinterpret_cast<const opus_int16*>(decodedBuffer.constData()), samples,
                                reinterpret_cast<unsigned char*>(encodedBuffer.data()), MAX_PACKET_BYTES);
        if (bytes < 0) {
            warnSometimes(encodeFailures, "encode failed:", bytes);
            encodedBuffer.clear();
            return;
        }
        encodedBuffer.resize(bytes);
    }

private:
    OpusEncoder* _encoder { nullptr };
    const int _numChannels;
};

class OpusDecoderStream : public Decoder {
public:
    OpusDecoderStream(int sampleRate, int numChannels) :
        _numChannels(numChannels), _decodedSize(frameSamples() * numChannels * (int)sizeof(opus_int16)) {
        int error = OPUS_OK;
        _decoder = opus_decoder_create(sampleRate, numChannels, &error);
        if (error != OPUS_OK || !_decoder) {
            qCWarning(opuscodec) << "decoder not created for" << sampleRate << "Hz," << numChannels << "channels:"
                << opus_strerror(error);
            _decoder = nullptr;
        }
    }
    ~OpusDecoderStream() override {
        if (_decoder) {
            opus_decoder_destroy(_decoder);
        }
    }

    void decode(const QByteArray& encodedBuffer, QByteArray& decodedBuffer) override {
        decodedBuffer.resize(_decodedSize);
        int samples = _decoder ? opus_decode(_decoder, reinterpret_cast<const unsigned char*>(encodedBuffer.constData()),
                                             encodedBuffer.size(), reinterpret_cast<opus_int16*>(decodedBuffer.data()),
                                             frameSamples(), 0)
                               : OPUS_INVALID_STATE;
        finish(samples, decodedBuffer);
    }

    // the frame that did not arrive, as Opus conceals it from the frames before (the caller may hand an empty buffer)
    void lostFrame(QByteArray& decodedBuffer) override {
        decodedBuffer.resize(_decodedSize);
        int samples = _decoder ? opus_decode(_decoder, nullptr, 0, reinterpret_cast<opus_int16*>(decodedBuffer.data()),
                                             frameSamples(), 0)
                               : OPUS_INVALID_STATE;
        finish(samples, decodedBuffer);
    }

private:
    // a failed or short decode leaves silence where it fell short, never stale memory
    void finish(int samples, QByteArray& decodedBuffer) {
        if (samples < 0) {
            if (_decoder) {
                warnSometimes(decodeFailures, "decode failed:", samples);
            }
            samples = 0;
        }
        const int written = samples * _numChannels * (int)sizeof(opus_int16);
        if (written < _decodedSize) {
            memset(decodedBuffer.data() + written, 0, _decodedSize - written);
        }
    }

    OpusDecoder* _decoder { nullptr };
    const int _numChannels;
    const int _decodedSize;
};

// More than two channels (Opus's own limit without multistream): PCM, as the PCM codec sends it
class PassthroughStream : public Encoder, public Decoder {
public:
    explicit PassthroughStream(int numChannels) :
        _decodedSize(frameSamples() * numChannels * (int)sizeof(opus_int16)) {}
    void encode(const QByteArray& decodedBuffer, QByteArray& encodedBuffer) override { encodedBuffer = decodedBuffer; }
    void decode(const QByteArray& encodedBuffer, QByteArray& decodedBuffer) override { decodedBuffer = encodedBuffer; }
    void lostFrame(QByteArray& decodedBuffer) override {
        decodedBuffer.resize(_decodedSize);
        memset(decodedBuffer.data(), 0, _decodedSize);
    }
private:
    const int _decodedSize;
};

bool opusTakes(int numChannels) {
    return numChannels == 1 || numChannels == 2;
}

}  // namespace

bool OpusCodec::activate() {
    CodecPlugin::activate();
    return true;
}

void OpusCodec::deactivate() {
    CodecPlugin::deactivate();
}

Encoder* OpusCodec::createEncoder(int sampleRate, int numChannels) {
    if (!opusTakes(numChannels)) {
        return new PassthroughStream(numChannels);
    }
    return new OpusEncoderStream(sampleRate, numChannels);
}

Decoder* OpusCodec::createDecoder(int sampleRate, int numChannels) {
    if (!opusTakes(numChannels)) {
        return new PassthroughStream(numChannels);
    }
    return new OpusDecoderStream(sampleRate, numChannels);
}

void OpusCodec::releaseEncoder(Encoder* encoder) {
    delete encoder;
}

void OpusCodec::releaseDecoder(Decoder* decoder) {
    delete decoder;
}
