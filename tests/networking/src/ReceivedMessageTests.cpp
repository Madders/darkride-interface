//
//  ReceivedMessageTests.cpp
//  tests/networking/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "ReceivedMessageTests.h"

#include <ReceivedMessage.h>

QTEST_MAIN(ReceivedMessageTests)

namespace {

// a message as a server receives it: just its payload
std::unique_ptr<ReceivedMessage> messageOf(const QByteArray& payload) {
    return std::make_unique<ReceivedMessage>(payload, PacketType::EntityScriptCallMethod, 0, HifiSockAddr());
}

// a string as packets carry it: a 32-bit length, then the UTF-8 bytes
QByteArray stringField(quint32 length, const QByteArray& bytes) {
    QByteArray field(reinterpret_cast<const char*>(&length), sizeof(length));
    return field + bytes;
}

}

void ReceivedMessageTests::readStringTest() {
    auto message = messageOf(stringField(3, "rez") + stringField(5, "hello"));
    QCOMPARE(message->readString(), QString("rez"));
    QCOMPARE(message->readString(), QString("hello"));
    QCOMPARE(message->getBytesLeftToRead(), 0);
}

void ReceivedMessageTests::readStringLongerThanMessageTest() {
    // a length far past the end: what is there, and the position stops at the end
    auto message = messageOf(stringField(0xFFFFFFF0, "rez"));
    QCOMPARE(message->readString(), QString("rez"));
    QCOMPARE(message->getBytesLeftToRead(), 0);
    QCOMPARE(message->readString(), QString());
    QCOMPARE(message->getBytesLeftToRead(), 0);
}

void ReceivedMessageTests::readPastEndTest() {
    auto message = messageOf("abcdef");
    QCOMPARE(message->read(4), QByteArray("abcd"));
    QCOMPARE(message->read(100), QByteArray("ef"));
    QCOMPARE(message->getBytesLeftToRead(), 0);
    QCOMPARE(message->readWithoutCopy(100), QByteArray());
    quint32 value = 7;
    QCOMPARE(message->readPrimitive(&value), 0);
    QCOMPARE(value, quint32(7));
}

void ReceivedMessageTests::readAfterSeekPastEndTest() {
    // a seek computed from the message can land past its end: nothing more is read from there
    auto message = messageOf("abcdef");
    message->seek(1000);
    char buffer[8] = { 0 };
    QCOMPARE(message->read(buffer, sizeof(buffer)), 0);
    QCOMPARE(message->peek(buffer, sizeof(buffer)), 0);
    QCOMPARE(message->read(4), QByteArray());
    QCOMPARE(message->readWithoutCopy(4), QByteArray());
    QCOMPARE(message->readString(), QString());
    QCOMPARE(message->readAll(), QByteArray());
}

void ReceivedMessageTests::negativeSizesTest() {
    auto message = messageOf("abcdef");
    char buffer[8] = { 0 };
    QCOMPARE(message->read(buffer, -1), 0);
    QCOMPARE(message->read(-1), QByteArray());
    QCOMPARE(message->readWithoutCopy(-5), QByteArray());
    QCOMPARE(message->getPosition(), 0);
    QCOMPARE(message->read(6), QByteArray("abcdef"));
}
