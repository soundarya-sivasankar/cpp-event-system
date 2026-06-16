#pragma once

#include "event.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// subscriber.h — Subscriber interface (Observer pattern)
//
// Design:
//   ISubscriber is a pure abstract base class. Any component that wants to
//   receive events implements this interface. The EventBus holds pointers
//   to ISubscriber and calls onEvent() for each published event.
//
//   This is the classic Observer pattern — the subscriber doesn't know
//   about the publisher, and the publisher doesn't know about subscribers.
//   They communicate only through the EventBus (the subject).
//
// Thread safety:
//   onEvent() may be called from any thread if async delivery is enabled.
//   Implementations should be thread-safe or use synchronisation.
// ─────────────────────────────────────────────────────────────────────────────

class ISubscriber {
public:
    virtual ~ISubscriber() = default;

    // Called by the EventBus when an event is published.
    // Implement this to handle incoming events.
    // Use std::visit inside to handle each event type explicitly.
    virtual void onEvent(const Event& event) = 0;

    // Optional: a name for logging and debugging
    virtual std::string name() const = 0;
};
