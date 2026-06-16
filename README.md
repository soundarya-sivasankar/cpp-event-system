# cpp-event-system
# C++ Real-Time Event Notification System

![Build and Test](https://github.com/soundarya-sivasankar/cpp-event-system/actions/workflows/build.yml/badge.svg)

A thread-safe, general-purpose publish/subscribe event notification library in C++17, demonstrating the Observer design pattern with both synchronous and asynchronous delivery modes.

---

## Overview

Modern high-tech systems — from semiconductor equipment to medical devices to industrial automation — need reliable ways for components to notify each other about state changes, measurements, alarms, and configuration updates without tight coupling between producers and consumers.

This library provides a clean, reusable foundation for that pattern:

- **Publishers** call `EventBus::publish()` — they never know who is listening
- **Subscribers** implement `ISubscriber::onEvent()` — they never know who is publishing
- **EventBus** connects them — decoupled, thread-safe, configurable delivery

---

## Event Types

| Event | Use case |
|---|---|
| `StateChangeEvent` | Component transitions between states (e.g. idle → running) |
| `MeasurementEvent` | Sensor or instrument produces a reading (e.g. temperature=72.3°C) |
| `AlarmEvent` | Threshold exceeded or fault detected / cleared |
| `ConfigChangeEvent` | Parameter reconfigured (analogous to SNMP config change traps) |
| `HeartbeatEvent` | Periodic keep-alive signal — gaps indicate missed beats |

---

## Design Patterns & C++ Features

| Feature | Where |
|---|---|
| **Observer pattern** | `EventBus` (subject) + `ISubscriber` (observer) |
| **`std::variant`** | Type-safe `Event` union — compiler enforces all types are handled |
| **`std::visit`** | Selective event handling in subscribers |
| **RAII** | `std::lock_guard`, automatic thread lifecycle management |
| **`std::thread`** | Background worker thread for async delivery |
| **`std::condition_variable`** | Efficient async queue drain — no busy-waiting |
| **`std::atomic`** | Lock-free statistics counters |
| **`std::function`** | Extensible subscriber callbacks |
| **Google Test** | 25 unit tests — sync, async, concurrency, edge cases |
| **CMake + FetchContent** | Modern build system |
| **GitHub Actions** | CI/CD pipeline |

---

## Delivery Modes

### Synchronous (default)
`publish()` notifies all subscribers in the calling thread before returning. Simple, predictable, zero latency overhead. Use when strict event ordering is required.

### Asynchronous
`publish()` queues the event and returns immediately. A background thread drains the queue. Use when publishers must not block (e.g. real-time control loops, hardware interrupt handlers).

```cpp
EventBus sync_bus;                                    // Synchronous
EventBus async_bus(DeliveryMode::ASYNCHRONOUS);       // Asynchronous
```

---

## Build & Run

**Requirements:** CMake 3.14+, C++17 compiler (GCC 9+ or Clang 9+)

```bash
git clone https://github.com/soundarya-sivasankar/cpp-event-system.git
cd cpp-event-system

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Run the demo
./build/main

# Run all 25 unit tests
ctest --test-dir build --output-on-failure
```

---

## Quick Start

```cpp
#include "event.h"
#include "event_bus.h"
#include "subscriber.h"

// 1. Define a subscriber
class MyLogger : public ISubscriber {
public:
    void onEvent(const Event& event) override {
        std::cout << eventToString(event) << "\n";
    }
    std::string name() const override { return "MyLogger"; }
};

int main() {
    EventBus bus;
    MyLogger logger;

    bus.subscribe(&logger);

    // Publish any event type — subscriber receives all of them
    bus.publish(StateChangeEvent{"motor", "idle", "running", Severity::INFO});
    bus.publish(MeasurementEvent{"sensor_1", "temperature", 72.3, "C"});
    bus.publish(AlarmEvent{"pump", "pressure high", Severity::WARNING, true});
}
```

---

## Project Structure

```
cpp-event-system/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── event.h          # All event types + Event variant + eventToString()
│   ├── subscriber.h     # ISubscriber interface (Observer)
│   └── event_bus.h      # EventBus — subscribe, publish, async worker thread
├── src/
│   └── main.cpp         # Demo: 2 scenarios, 3 subscriber types
├── tests/
│   └── test_event_bus.cpp  # 25 Google Tests
└── .github/workflows/
    └── build.yml           # CI: build + test on every push
```

---

## About

**Soundarya Sivasankar** — Senior Software Engineer  
6+ years of C/C++ experience in mission-critical embedded systems including direct work on event notification systems (SNMP trap implementation), CLI management software, and OLS/OTDR module development for Nokia's 1830 PSS optical networking platform.  
Based in Helmond, Netherlands. Open to C++ software and embedded engineering roles.

[LinkedIn](https://www.linkedin.com/in/soundarya-sivasankar) · [GitHub](https://github.com/soundarya-sivasankar)
