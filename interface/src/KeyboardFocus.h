//
//  KeyboardFocus.h
//  interface/src
//
//  Which entity has the keyboard, and the highlight drawn around it. setFocusedEntity() records the entity, gives
//  its renderable the window, takes the keyboard-mouse device's focus and starts the idle clock (while the sign-in
//  dialog is up, only that dialog may take the focus; an entity without a renderable yet is waited for);
//  deliverKeyEvent() sends a key event to the focused entity's handler and restarts the clock when it is accepted;
//  update(), once a frame, drops the focus after thirty idle seconds, moves the highlight with the entity, and
//  retries a focus that waited for its renderable; highlight() makes the highlight on demand, a local line-box
//  entity, and moves it; deleteHighlight() removes it at shutdown. Application owns one, reached by
//  getKeyboardFocus(); these were Application's getKeyboardFocusEntity, setKeyboardFocusEntity,
//  setKeyboardFocusHighlight and handleKeyEventForFocusedEntity, the frame loop's follow, the shutdown's deletion
//  and the globals of KeyboardFocusHighlight.h (ARCHITECTURE.md §5.4 step 4, the third owner class).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_KeyboardFocus_h
#define hifi_KeyboardFocus_h

#include <QtCore/QUuid>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <EntityItemID.h>
#include <ThreadSafeValueCache.h>

class QEvent;

class KeyboardFocus {
public:
    QUuid focusedEntity() const { return _focused.get(); }   // thread-safe
    void setFocusedEntity(const QUuid& id);
    void highlight(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& dimensions);
    bool deliverKeyEvent(QEvent* event);   // to the focused entity's handler; true when it accepted the event
    void update();                          // once a frame
    void deleteHighlight();                 // at shutdown

private:
    ThreadSafeValueCache<EntityItemID> _focused;
    quint64 _lastAcceptedKeyPress{ 0 };
    bool _waitingOnRenderable{ false };
    QUuid _highlightID;   // the highlight entity; UNKNOWN_ENTITY_ID when there is none
};

#endif // hifi_KeyboardFocus_h
