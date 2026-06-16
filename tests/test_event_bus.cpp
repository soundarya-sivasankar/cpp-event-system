#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include "event.h"
#include "event_bus.h"
#include "subscriber.h"

// ─────────────────────────────────────────────────────────────────────────────
// Unit tests for the C++ Event Notification System
//
// Coverage:
//   - Event type construction and string formatting
//   - EventBus subscribe / unsubscribe
//   - Synchronous delivery — ordering and completeness
//   - Asynchronous delivery — all events delivered eventually
//   - Concurrent publishing — thread safety
//   - Selective subscriber handling (std::visit patterns)
//   - Statistics (published / delivered counts)
//   - Edge cases: no subscribers, duplicate subscribe, unsubscribe mid-run
// ─────────────────────────────────────────────────────────────────────────────


// ── Test subscriber helpers ───────────────────────────────────────────────────

// Counts every event received — useful for delivery verification
class CountingSubscriber : public ISubscriber {
public:
    explicit CountingSubscriber(const std::string& n) : name_(n) {}

    void onEvent(const Event&) override { ++count_; }
    std::string name() const override { return name_; }
    int count() const { return count_.load(); }
    void reset() { count_ = 0; }

private:
    std::string        name_;
    std::atomic<int>   count_{0};
};

// Records the string representation of every event received
class RecordingSubscriber : public ISubscriber {
public:
    void onEvent(const Event& e) override {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(eventToString(e));
    }
    std::string name() const override { return "RecordingSubscriber"; }
    int count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }
    std::vector<std::string> records() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }

private:
    mutable std::mutex       mutex_;
    std::vector<std::string> records_;
};

// Only handles AlarmEvents — ignores everything else
class AlarmOnlySubscriber : public ISubscriber {
public:
    void onEvent(const Event& event) override {
        std::visit([this](const auto& e) { handle(e); }, event);
    }
    std::string name() const override { return "AlarmOnly"; }
    int alarmCount() const { return alarm_count_; }

private:
    int alarm_count_ = 0;
    void handle(const AlarmEvent&)        { ++alarm_count_; }
    void handle(const StateChangeEvent&)  {}
    void handle(const MeasurementEvent&)  {}
    void handle(const ConfigChangeEvent&) {}
    void handle(const HeartbeatEvent&)    {}
};


// ── Event type tests ──────────────────────────────────────────────────────────

TEST(EventTest, StateChangeEventToString) {
    StateChangeEvent e{"motor", "idle", "running", Severity::INFO};
    auto s = e.toString();
    EXPECT_NE(s.find("StateChange"), std::string::npos);
    EXPECT_NE(s.find("motor"), std::string::npos);
    EXPECT_NE(s.find("idle"), std::string::npos);
    EXPECT_NE(s.find("running"), std::string::npos);
}

TEST(EventTest, MeasurementEventToString) {
    MeasurementEvent e{"sensor_1", "temperature", 72.3, "C"};
    auto s = e.toString();
    EXPECT_NE(s.find("Measurement"), std::string::npos);
    EXPECT_NE(s.find("temperature"), std::string::npos);
    EXPECT_NE(s.find("72.30"), std::string::npos);
}

TEST(EventTest, AlarmEventActiveToString) {
    AlarmEvent e{"pump_1", "pressure exceeded", Severity::CRITICAL, true};
    auto s = e.toString();
    EXPECT_NE(s.find("Alarm"), std::string::npos);
    EXPECT_NE(s.find("ACTIVE"), std::string::npos);
    EXPECT_NE(s.find("CRITICAL"), std::string::npos);
}

TEST(EventTest, AlarmEventClearedToString) {
    AlarmEvent e{"pump_1", "pressure exceeded", Severity::CRITICAL, false};
    EXPECT_NE(e.toString().find("CLEARED"), std::string::npos);
}

TEST(EventTest, ConfigChangeEventToString) {
    ConfigChangeEvent e{"controller", "gain", "1.0", "2.5"};
    auto s = e.toString();
    EXPECT_NE(s.find("ConfigChange"), std::string::npos);
    EXPECT_NE(s.find("gain"), std::string::npos);
    EXPECT_NE(s.find("1.0"), std::string::npos);
    EXPECT_NE(s.find("2.5"), std::string::npos);
}

TEST(EventTest, HeartbeatEventToString) {
    HeartbeatEvent e{"watchdog", 42};
    auto s = e.toString();
    EXPECT_NE(s.find("Heartbeat"), std::string::npos);
    EXPECT_NE(s.find("42"), std::string::npos);
}

TEST(EventTest, EventToStringVariant) {
    Event e = MeasurementEvent{"s", "v", 1.0, "unit"};
    EXPECT_NE(eventToString(e).find("Measurement"), std::string::npos);
}


// ── Severity tests ────────────────────────────────────────────────────────────

TEST(SeverityTest, ToStringAllValues) {
    EXPECT_EQ(severityToString(Severity::INFO),     "INFO");
    EXPECT_EQ(severityToString(Severity::WARNING),  "WARNING");
    EXPECT_EQ(severityToString(Severity::ERROR),    "ERROR");
    EXPECT_EQ(severityToString(Severity::CRITICAL), "CRITICAL");
}

TEST(SeverityTest, ComparisonOperators) {
    EXPECT_LT(Severity::INFO,    Severity::WARNING);
    EXPECT_LT(Severity::WARNING, Severity::ERROR);
    EXPECT_LT(Severity::ERROR,   Severity::CRITICAL);
    EXPECT_GE(Severity::ERROR,   Severity::ERROR);
}


// ── EventBus synchronous delivery tests ──────────────────────────────────────

TEST(EventBusSyncTest, SubscriberReceivesPublishedEvent) {
    EventBus bus;
    CountingSubscriber sub("test");
    bus.subscribe(&sub);

    bus.publish(HeartbeatEvent{"src", 1});
    EXPECT_EQ(sub.count(), 1);
}

TEST(EventBusSyncTest, MultipleSubscribersAllReceiveEvent) {
    EventBus bus;
    CountingSubscriber a("a"), b("b"), c("c");
    bus.subscribe(&a);
    bus.subscribe(&b);
    bus.subscribe(&c);

    bus.publish(HeartbeatEvent{"src", 1});

    EXPECT_EQ(a.count(), 1);
    EXPECT_EQ(b.count(), 1);
    EXPECT_EQ(c.count(), 1);
}

TEST(EventBusSyncTest, AllEventTypesDelivered) {
    EventBus bus;
    CountingSubscriber sub("all");
    bus.subscribe(&sub);

    bus.publish(StateChangeEvent{"c", "a", "b", Severity::INFO});
    bus.publish(MeasurementEvent{"s", "p", 1.0, "u"});
    bus.publish(AlarmEvent{"s", "d", Severity::WARNING, true});
    bus.publish(ConfigChangeEvent{"c", "p", "old", "new"});
    bus.publish(HeartbeatEvent{"src", 1});

    EXPECT_EQ(sub.count(), 5);
}

TEST(EventBusSyncTest, EventsDeliveredInOrder) {
    EventBus bus;
    RecordingSubscriber sub;
    bus.subscribe(&sub);

    bus.publish(HeartbeatEvent{"src", 1});
    bus.publish(HeartbeatEvent{"src", 2});
    bus.publish(HeartbeatEvent{"src", 3});

    auto records = sub.records();
    ASSERT_EQ(records.size(), 3u);
    EXPECT_NE(records[0].find("seq=1"), std::string::npos);
    EXPECT_NE(records[1].find("seq=2"), std::string::npos);
    EXPECT_NE(records[2].find("seq=3"), std::string::npos);
}

TEST(EventBusSyncTest, NoSubscribersDoesNotCrash) {
    EventBus bus;
    EXPECT_NO_THROW(bus.publish(HeartbeatEvent{"src", 1}));
    EXPECT_EQ(bus.publishedCount(), 1u);
    EXPECT_EQ(bus.deliveredCount(), 0u);
}

TEST(EventBusSyncTest, DuplicateSubscribeIgnored) {
    EventBus bus;
    CountingSubscriber sub("dup");
    bus.subscribe(&sub);
    bus.subscribe(&sub);  // Should be ignored

    EXPECT_EQ(bus.subscriberCount(), 1u);

    bus.publish(HeartbeatEvent{"src", 1});
    EXPECT_EQ(sub.count(), 1);  // Not 2
}

TEST(EventBusSyncTest, UnsubscribeStopsDelivery) {
    EventBus bus;
    CountingSubscriber sub("unsub");
    bus.subscribe(&sub);

    bus.publish(HeartbeatEvent{"src", 1});
    EXPECT_EQ(sub.count(), 1);

    bus.unsubscribe(&sub);
    bus.publish(HeartbeatEvent{"src", 2});
    EXPECT_EQ(sub.count(), 1);  // Still 1 — not receiving anymore
}

TEST(EventBusSyncTest, StatisticsAreAccurate) {
    EventBus bus;
    CountingSubscriber a("a"), b("b");
    bus.subscribe(&a);
    bus.subscribe(&b);

    bus.publish(HeartbeatEvent{"src", 1});
    bus.publish(HeartbeatEvent{"src", 2});

    EXPECT_EQ(bus.publishedCount(), 2u);
    EXPECT_EQ(bus.deliveredCount(), 4u);  // 2 events x 2 subscribers
}


// ── Selective handling tests ──────────────────────────────────────────────────

TEST(SelectiveHandlingTest, AlarmOnlySubscriberIgnoresOtherEvents) {
    EventBus bus;
    AlarmOnlySubscriber sub;
    bus.subscribe(&sub);

    bus.publish(MeasurementEvent{"s", "p", 1.0, "u"});
    bus.publish(HeartbeatEvent{"src", 1});
    bus.publish(StateChangeEvent{"c", "a", "b", Severity::INFO});
    bus.publish(AlarmEvent{"s", "d", Severity::WARNING, true});
    bus.publish(ConfigChangeEvent{"c", "p", "old", "new"});

    EXPECT_EQ(sub.alarmCount(), 1);  // Only the AlarmEvent
}


// ── Asynchronous delivery tests ───────────────────────────────────────────────

TEST(EventBusAsyncTest, AllEventsDeliveredEventually) {
    EventBus bus(DeliveryMode::ASYNCHRONOUS);
    CountingSubscriber sub("async");
    bus.subscribe(&sub);

    const int count = 20;
    for (int i = 0; i < count; ++i) {
        bus.publish(HeartbeatEvent{"src", static_cast<uint64_t>(i)});
    }

    // Wait for async delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(sub.count(), count);
}

TEST(EventBusAsyncTest, ConcurrentPublishersNoDataCorruption) {
    EventBus bus(DeliveryMode::ASYNCHRONOUS);
    CountingSubscriber sub("concurrent");
    bus.subscribe(&sub);

    const int threads = 4;
    const int per_thread = 25;

    std::vector<std::thread> publishers;
    for (int t = 0; t < threads; ++t) {
        publishers.emplace_back([&bus, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                bus.publish(HeartbeatEvent{"thread", static_cast<uint64_t>(i)});
            }
        });
    }
    for (auto& t : publishers) t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(sub.count(), threads * per_thread);
}
