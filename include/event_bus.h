#pragma once

#include "event.h"
#include "subscriber.h"

#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <string>
#include <algorithm>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

// ─────────────────────────────────────────────────────────────────────────────
// event_bus.h — Central event dispatcher (Observer pattern)
//
// The EventBus is the heart of the system. Publishers call publish() to
// emit an event. All registered subscribers receive the event via onEvent().
//
// Two delivery modes:
//
//   SYNCHRONOUS (default):
//     publish() calls each subscriber's onEvent() directly, in the calling
//     thread, before returning. Simple, predictable, zero latency overhead.
//     Use when publishers and subscribers are in the same thread context,
//     or when event ordering must be strictly preserved.
//
//   ASYNCHRONOUS:
//     publish() queues the event and returns immediately. A dedicated
//     background thread drains the queue and notifies subscribers.
//     Use when publishers must not block (e.g. hardware interrupt handlers,
//     real-time control loops), or when subscriber processing is slow.
//
// Thread safety:
//   All public methods are thread-safe. Multiple threads can publish
//   concurrently. Subscriber registration/removal is also thread-safe.
//
// Design patterns used:
//   Observer  — EventBus as subject, ISubscriber as observer
//   RAII      — background thread and mutex managed by EventBus lifetime
// ─────────────────────────────────────────────────────────────────────────────

enum class DeliveryMode {
    SYNCHRONOUS,   // Subscribers notified in publish() call
    ASYNCHRONOUS   // Events queued; background thread notifies subscribers
};


class EventBus {
public:

    // ── Construction ──────────────────────────────────────────────────────────

    explicit EventBus(DeliveryMode mode = DeliveryMode::SYNCHRONOUS)
        : mode_(mode), running_(false)
    {
        if (mode_ == DeliveryMode::ASYNCHRONOUS) {
            startWorkerThread();
        }
    }

    // Destructor: stop worker thread cleanly if async mode
    ~EventBus() {
        if (mode_ == DeliveryMode::ASYNCHRONOUS) {
            stopWorkerThread();
        }
    }

    // Non-copyable — owning a mutex and thread makes copying unsafe
    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;


    // ── Subscriber management ─────────────────────────────────────────────────

    // Register a subscriber to receive all events.
    // Duplicates (same pointer) are silently ignored.
    void subscribe(ISubscriber* subscriber) {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto it = std::find(subscribers_.begin(), subscribers_.end(), subscriber);
        if (it == subscribers_.end()) {
            subscribers_.push_back(subscriber);
        }
    }

    // Remove a subscriber. Safe to call even if not registered.
    void unsubscribe(ISubscriber* subscriber) {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_.erase(
            std::remove(subscribers_.begin(), subscribers_.end(), subscriber),
            subscribers_.end()
        );
    }

    // How many subscribers are currently registered
    size_t subscriberCount() const {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        return subscribers_.size();
    }


    // ── Publishing ────────────────────────────────────────────────────────────

    // Publish an event to all registered subscribers.
    // In SYNCHRONOUS mode: notifies all subscribers before returning.
    // In ASYNCHRONOUS mode: queues event and returns immediately.
    void publish(Event event) {
        ++published_count_;

        if (mode_ == DeliveryMode::SYNCHRONOUS) {
            notifyAll(event);
        } else {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                event_queue_.push(std::move(event));
            }
            queue_cv_.notify_one();
        }
    }


    // ── Statistics ────────────────────────────────────────────────────────────

    uint64_t publishedCount()  const { return published_count_.load(); }
    uint64_t deliveredCount()  const { return delivered_count_.load(); }
    DeliveryMode deliveryMode() const { return mode_; }


private:

    // ── Internal: notify all subscribers of one event ─────────────────────────
    void notifyAll(const Event& event) {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (auto* sub : subscribers_) {
            sub->onEvent(event);
            ++delivered_count_;
        }
    }

    // ── Async worker thread ───────────────────────────────────────────────────
    void startWorkerThread() {
        running_ = true;
        worker_thread_ = std::thread([this]() {
            while (running_ || !event_queue_.empty()) {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                // Wait until there's an event or we're shutting down
                queue_cv_.wait(lock, [this]() {
                    return !event_queue_.empty() || !running_;
                });

                // Drain the queue
                while (!event_queue_.empty()) {
                    Event event = std::move(event_queue_.front());
                    event_queue_.pop();
                    lock.unlock();          // Release queue lock while notifying
                    notifyAll(event);
                    lock.lock();            // Re-acquire for next iteration
                }
            }
        });
    }

    void stopWorkerThread() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            running_ = false;
        }
        queue_cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }


    // ── Member variables ──────────────────────────────────────────────────────

    DeliveryMode               mode_;

    // Subscriber list — protected by subscribers_mutex_
    mutable std::mutex         subscribers_mutex_;
    std::vector<ISubscriber*>  subscribers_;

    // Async event queue — protected by queue_mutex_
    std::mutex                 queue_mutex_;
    std::condition_variable    queue_cv_;
    std::queue<Event>          event_queue_;
    std::thread                worker_thread_;
    std::atomic<bool>          running_;

    // Statistics — atomic for lock-free reads
    std::atomic<uint64_t>      published_count_{0};
    std::atomic<uint64_t>      delivered_count_{0};
};
