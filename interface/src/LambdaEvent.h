//
//  LambdaEvent.h
//  interface/src
//
//  An event that carries a function to run on the thread it is sent to: Application's postLambdaEvent and
//  sendLambdaEvent make one, its event() runs it. Out of Application.cpp, where it was defined inline.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_LambdaEvent_h
#define hifi_LambdaEvent_h

#include <functional>

#include <QtCore/QEvent>

#include "graphics/RenderEventHandler.h"   // ApplicationEvent

class LambdaEvent : public QEvent {
    std::function<void()> _fun;
public:
    LambdaEvent(const std::function<void()> & fun) :
    QEvent(static_cast<QEvent::Type>(ApplicationEvent::Lambda)), _fun(fun) {
    }
    LambdaEvent(std::function<void()> && fun) :
    QEvent(static_cast<QEvent::Type>(ApplicationEvent::Lambda)), _fun(fun) {
    }
    void call() const { _fun(); }
};

#endif // hifi_LambdaEvent_h
