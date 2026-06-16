#pragma once

#include <string>
#include <variant>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <iomanip>

// ─────────────────────────────────────────────────────────────────────────────
// event.h — Strongly typed event definitions
//
// Design philosophy:
//   Events are defined as plain structs and wrapped in a std::variant.
//   This means the compiler enforces that every subscriber handles every
//   event type explicitly — safer than inheritance + dynamic_cast, and
//   avoids heap allocation per event.
//
// Adding a new event type:
//   1. Define a new struct below
//   2. Add it to the Event variant alias at the bottom
//   The compiler will then flag any std::visit that doesn't cover it.
//
// Background:
//   This pattern generalises publish/subscribe notification models used
//   across industrial systems — sensor readings, state changes, alarms,
//   configuration updates, and keep-alive signals — into a reusable,
//   domain-agnostic C++17 library. It is directly inspired by real-world
//   SNMP trap notification patterns used in telecom and industrial platforms.
// ─────────────────────────────────────────────────────────────────────────────


// ── Timestamp ─────────────────────────────────────────────────────────────────
using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

inline Timestamp now() { return std::chrono::steady_clock::now(); }

inline std::string timestampToString(const Timestamp& ts) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()).count();
    return "[t+" + std::to_string(ms) + "ms]";
}


// ── Severity ──────────────────────────────────────────────────────────────────
enum class Severity : uint8_t {
    INFO = 0, WARNING = 1, ERROR = 2, CRITICAL = 3
};

inline std::string severityToString(Severity s) {
    switch (s) {
        case Severity::INFO:     return "INFO";
        case Severity::WARNING:  return "WARNING";
        case Severity::ERROR:    return "ERROR";
        case Severity::CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}


// ── Event type definitions ────────────────────────────────────────────────────

// A named component transitioned between states
// E.g. sensor: idle -> active, device: standby -> running
struct StateChangeEvent {
    std::string component;
    std::string from_state;
    std::string to_state;
    Severity    severity;
    Timestamp   timestamp = now();

    std::string toString() const {
        return timestampToString(timestamp) + " [StateChange] " +
               component + ": " + from_state + " -> " + to_state +
               " (" + severityToString(severity) + ")";
    }
};

// A sensor or instrument produced a measurement
// E.g. temperature=72.3 C, voltage=3.28 V, pressure=101.3 kPa
struct MeasurementEvent {
    std::string source;
    std::string parameter;
    double      value;
    std::string unit;
    Timestamp   timestamp = now();

    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        return timestampToString(timestamp) + " [Measurement] " +
               source + " " + parameter + "=" + oss.str() + " " + unit;
    }
};

// A threshold was exceeded or a fault condition was detected/cleared
// E.g. temperature too high, connection lost, hardware fault
struct AlarmEvent {
    std::string source;
    std::string description;
    Severity    severity;
    bool        active;       // true = raised, false = cleared
    Timestamp   timestamp = now();

    std::string toString() const {
        return timestampToString(timestamp) +
               " [Alarm][" + severityToString(severity) + "] " +
               source + ": " + description +
               (active ? " (ACTIVE)" : " (CLEARED)");
    }
};

// A configuration parameter was updated
// E.g. threshold changed, mode switched — analogous to SNMP config change traps
struct ConfigChangeEvent {
    std::string component;
    std::string parameter;
    std::string old_value;
    std::string new_value;
    Timestamp   timestamp = now();

    std::string toString() const {
        return timestampToString(timestamp) + " [ConfigChange] " +
               component + "." + parameter +
               ": \"" + old_value + "\" -> \"" + new_value + "\"";
    }
};

// Periodic keep-alive from a component — gaps in sequence indicate missed beats
struct HeartbeatEvent {
    std::string source;
    uint64_t    sequence;
    Timestamp   timestamp = now();

    std::string toString() const {
        return timestampToString(timestamp) + " [Heartbeat] " +
               source + " seq=" + std::to_string(sequence);
    }
};


// ── Event variant — the single type passed through the system ─────────────────
// std::variant is a type-safe tagged union. Subscribers use std::visit
// to handle each event type. The compiler catches unhandled types.
using Event = std::variant<
    StateChangeEvent,
    MeasurementEvent,
    AlarmEvent,
    ConfigChangeEvent,
    HeartbeatEvent
>;

// Convenience: get a printable description of any Event
inline std::string eventToString(const Event& e) {
    return std::visit([](const auto& ev) { return ev.toString(); }, e);
}
