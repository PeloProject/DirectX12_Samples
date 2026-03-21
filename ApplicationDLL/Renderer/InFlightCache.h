#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

template <typename TKey, typename TValue, typename THasher>
class InFlightCache
{
public:
    enum class EntryState
    {
        InFlight,
        Success,
        Failed,
    };

    struct Entry
    {
        EntryState state = EntryState::InFlight;
        TValue value{};
        std::condition_variable condition;
    };

    using EntryPtr = std::shared_ptr<Entry>;

    struct AcquireResult
    {
        EntryPtr entry;
        bool isCreator = false;
    };

    AcquireResult Acquire(const TKey& key)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            auto entry = std::make_shared<Entry>();
            cache_.emplace(key, entry);
            return { entry, true };
        }

        auto entry = it->second;
        if (entry->state == EntryState::InFlight)
        {
            entry->condition.wait(lock, [&entry] { return entry->state != EntryState::InFlight; });
        }

        return { entry, false };
    }

    void CompleteSuccess(const TKey& key, const EntryPtr& entry, const TValue& value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entry->value = value;
            entry->state = EntryState::Success;
        }

        entry->condition.notify_all();
    }

    void CompleteFailure(const TKey& key, const EntryPtr& entry, bool eraseOnFailure = true)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entry->state = EntryState::Failed;
        }

        entry->condition.notify_all();

        if (!eraseOnFailure)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end() && it->second == entry)
        {
            cache_.erase(it);
        }
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

private:
    std::mutex mutex_;
    std::unordered_map<TKey, EntryPtr, THasher> cache_;
};
