//
//  OpusCodec.h
//  plugins/opus-codec-plugin/src
//
//  The Opus audio codec (RFC 6716, libopus) as a codec plug-in: the stream between a client and the audio mixer, a
//  client's microphone up and the mixer's mix down, encoded with Opus instead of sent as PCM. The engine's frame is
//  10 ms at 24 kHz, an Opus frame size, so a frame is one Opus packet. Low-delay mode (CELT only, 2.5 ms look-ahead):
//  the mix is spatial and a listener hears it with their head's motion, so the codec adds as little delay as Opus can.
//  A mono stream (a microphone) at 32 kbit/s, complexity 10; a stereo one (a mix) at 64 kbit/s, complexity 5, since
//  the mixer encodes one per listener. Measured on an M3 Ultra at the engine's frame: a mono encode 25 µs, a stereo
//  one 30 µs, a decode 6-11 µs; 41 bytes a frame for a voice and 81 for a mix, against PCM's 480 and 960. A lost
//  frame is Opus's own concealment. Streams of more than two channels pass through as PCM, on both ends alike.
//  The audio mixer picks a client's codec from its "codec_preference_order" setting among the codecs the client
//  offers, so a client without this plug-in keeps the codec it had.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_OpusCodec_h
#define hifi_OpusCodec_h

#include <plugins/CodecPlugin.h>

class OpusCodec : public CodecPlugin {
    Q_OBJECT

public:
    bool isSupported() const override { return true; }
    const QString getName() const override { return NAME; }

    void init() override {}
    void deinit() override {}
    bool activate() override;
    void deactivate() override;

    Encoder* createEncoder(int sampleRate, int numChannels) override;
    Decoder* createDecoder(int sampleRate, int numChannels) override;
    void releaseEncoder(Encoder* encoder) override;
    void releaseDecoder(Decoder* decoder) override;

    static const char* NAME;
};

#endif // hifi_OpusCodec_h
