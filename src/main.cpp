#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "event.h"
#include "event_bus.h"
#include "subscriber.h"

// ─────────────────────────────────────────────────────────────────────────────
// Demo: A general-purpose event-driven system
//
// Three subscriber types demonstrate typical use cases:
//
//   EventLogger    — logs every event to console (audit trail)
//   AlarmMonitor   — reacts only to alarms and critical state changes
//   DataRecorder   — records measurements for analysis
//
// Two scenarios:
//   1. Synchronous delivery — simple, predictable, single-threaded
//   2. Asynchronous delivery — publisher never blocks, background delivery
// ─────────────────────────────────────────────────────────────────────────────


// ── Subscriber implementations ────────────────────────────────────────────────

// EventLogger: logs every event — useful for audit trails and debugging
class EventLogger : public ISubscriber {
public:
    explicit EventLogger(const std::string& label) : label_(label) {}

    void onEvent(const Event& event) override {
        std::cout << "[" << label_ << "] " << eventToString(event) << "\n";
        ++event_count_;
    }

    std::string name() const override { return label_; }
    int eventCount() const { return event_count_; }

private:
    std::string label_;
    int event_count_ = 0;
};


// AlarmMonitor: only reacts to alarms and critical/error state changes
// Demonstrates selective handling with std::visit
class AlarmMonitor : public ISubscriber {
public:
    void onEvent(const Event& event) override {
        std::visit([this](const auto& e) { handle(e); }, event);
    }

    std::string name() const override { return "AlarmMonitor"; }
    int activeAlarms() const { return active_alarms_; }

private:
    int active_alarms_ = 0;

    // Handle alarm events — track active vs cleared
    void handle(const AlarmEvent& e) {
        if (e.active) {
            ++active_alarms_;
            std::cout << "  *** ALARM MONITOR: Active alarm from "
                      << e.source << " — " << e.description
                      << " [" << severityToString(e.severity) << "] ***\n";
        } else {
            --active_alarms_;
            std::cout << "  *** ALARM MONITOR: Alarm cleared from "
                      << e.source << " ***\n";
        }
    }

    // Handle state changes — only react to ERROR or CRITICAL
    void handle(const StateChangeEvent& e) {
        if (e.severity >= Severity::ERROR) {
            std::cout << "  *** ALARM MONITOR: Critical state change on "
                      << e.component << ": " << e.from_state
                      << " -> " << e.to_state << " ***\n";
        }
    }

    // Ignore all other event types
    void handle(const MeasurementEvent&)  {}
    void handle(const ConfigChangeEvent&) {}
    void handle(const HeartbeatEvent&)    {}
};


// DataRecorder: captures measurement events for analysis
// Demonstrates filtering to a single event type
class DataRecorder : public ISubscriber {
public:
    void onEvent(const Event& event) override {
        std::visit([this](const auto& e) { handle(e); }, event);
    }

    std::string name() const override { return "DataRecorder"; }

    int recordingCount() const { return recordings_.size(); }

    void printSummary() const {
        std::cout << "\n  [DataRecorder] Recorded " << recordings_.size()
                  << " measurements:\n";
        for (const auto& r : recordings_) {
            std::cout << "    " << r << "\n";
        }
    }

private:
    std::vector<std::string> recordings_;

    void handle(const MeasurementEvent& e) {
        recordings_.push_back(e.toString());
    }

    // Ignore all other event types
    void handle(const StateChangeEvent&)  {}
    void handle(const AlarmEvent&)        {}
    void handle(const ConfigChangeEvent&) {}
    void handle(const HeartbeatEvent&)    {}
};


// ── Scenario 1: Synchronous delivery ─────────────────────────────────────────
void runSynchronousDemo() {
    std::cout << "\n================================================\n";
    std::cout << " Scenario 1: Synchronous Event Delivery\n";
    std::cout << "================================================\n\n";

    EventBus bus(DeliveryMode::SYNCHRONOUS);

    EventLogger  logger("AuditLog");
    AlarmMonitor monitor;
    DataRecorder recorder;

    bus.subscribe(&logger);
    bus.subscribe(&monitor);
    bus.subscribe(&recorder);

    std::cout << "--- Publishing system startup events ---\n";

    bus.publish(StateChangeEvent{
        "system", "offline", "initialising", Severity::INFO
    });

    bus.publish(StateChangeEvent{
        "system", "initialising", "online", Severity::INFO
    });

    std::cout << "\n--- Sensor measurements coming in ---\n";

    bus.publish(MeasurementEvent{
        "temp_sensor_1", "temperature", 72.3, "C"
    });

    bus.publish(MeasurementEvent{
        "pressure_sensor_2", "pressure", 101.4, "kPa"
    });

    bus.publish(MeasurementEvent{
        "voltage_monitor", "supply_voltage", 3.28, "V"
    });

    std::cout << "\n--- Configuration change ---\n";

    bus.publish(ConfigChangeEvent{
        "temp_sensor_1", "threshold", "80.0", "75.0"
    });

    std::cout << "\n--- Alarm raised and cleared ---\n";

    bus.publish(AlarmEvent{
        "temp_sensor_1",
        "Temperature approaching threshold",
        Severity::WARNING,
        true
    });

    bus.publish(MeasurementEvent{
        "temp_sensor_1", "temperature", 68.1, "C"
    });

    bus.publish(AlarmEvent{
        "temp_sensor_1",
        "Temperature approaching threshold",
        Severity::WARNING,
        false  // Cleared
    });

    std::cout << "\n--- Critical fault ---\n";

    bus.publish(StateChangeEvent{
        "motor_driver_3", "running", "fault", Severity::CRITICAL
    });

    std::cout << "\n--- Heartbeats ---\n";

    for (uint64_t i = 1; i <= 3; ++i) {
        bus.publish(HeartbeatEvent{ "watchdog", i });
    }

    std::cout << "\n--- Summary ---\n";
    std::cout << "Events published:  " << bus.publishedCount() << "\n";
    std::cout << "Events delivered:  " << bus.deliveredCount() << "\n";
    std::cout << "Active alarms:     " << monitor.activeAlarms() << "\n";
    recorder.printSummary();
}


// ── Scenario 2: Asynchronous delivery ────────────────────────────────────────
void runAsynchronousDemo() {
    std::cout << "\n================================================\n";
    std::cout << " Scenario 2: Asynchronous Event Delivery\n";
    std::cout << " (publisher never blocks)\n";
    std::cout << "================================================\n\n";

    EventBus bus(DeliveryMode::ASYNCHRONOUS);

    EventLogger logger("AsyncLog");
    bus.subscribe(&logger);

    std::cout << "Publishing 6 events rapidly from main thread...\n";
    std::cout << "(Background thread delivers them asynchronously)\n\n";

    for (int i = 1; i <= 3; ++i) {
        bus.publish(MeasurementEvent{
            "fast_sensor", "reading", static_cast<double>(i) * 1.5, "units"
        });
        bus.publish(HeartbeatEvent{ "main_loop", static_cast<uint64_t>(i) });
    }

    // Give the async worker thread time to deliver
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << "\nPublished: " << bus.publishedCount()
              << " | Delivered: " << bus.deliveredCount() << "\n";
}


int main() {
    std::cout << "================================================\n";
    std::cout << " C++ Real-Time Event Notification System\n";
    std::cout << " Observer Pattern | C++17 | Thread-Safe\n";
    std::cout << "================================================\n";

    runSynchronousDemo();
    runAsynchronousDemo();

    std::cout << "\n================================================\n";
    std::cout << " All scenarios completed successfully.\n";
    std::cout << "================================================\n";

    return 0;
}
