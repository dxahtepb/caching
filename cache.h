//
// Created by student on 02.02.19.
//

#ifndef CACHINGPP_CACHE_H
#define CACHINGPP_CACHE_H


#include <unordered_map>
#include <list>
#include <vector>

enum class ListType {SECOND_CHANCE, CLOCK};


template <typename Key>
class LruList
{
public:
    bool check_presence(Key key)
    {
        return map_.find(key) != map_.end();
    }

    size_t size() const
    {
        return map_.size();
    }

    void make_mru(Key key)
    {
        if (check_presence(key))
        {
            list_.erase(map_[key]);
        }
        list_.push_front(key);
        map_[key] = list_.begin();
    }

    Key remove_lru()
    {
        map_.erase(list_.back());
        list_.pop_back();
    }

    void erase(Key key)
    {
        list_.erase(map_.at(key));
        map_.erase(key);
    }

private:
    std::list<Key> list_;
    std::unordered_map<Key, typename std::list<Key>::iterator> map_;
};


template <typename Key>
class BaseCacheList
{
public:
    virtual void push(Key) = 0;
    virtual void remove() = 0;
    virtual Key head() = 0;
    virtual size_t size() const = 0;
    virtual void advance_clock() = 0;
};


//TODO: IMPLEMENT CLOCK WITH FREE BUFFERS POOL FOR T1 AND T2
template <typename Key>
class ClockList : BaseCacheList<Key>
{
public:
    ClockList()
        : list_(),
        clock_hand_(list_.begin())
    {
    }

    void push(Key key) override
    {
        list_.insert(clock_hand_, key);
    }

    void remove() override
    {
        //TODO: INVALIDATED SOMETIMES :(
        clock_hand_ = list_.erase(clock_hand_);
        if (clock_hand_ == list_.end())
        {
            clock_hand_ = list_.begin();
        }
    }

    Key head() override
    {
        return *clock_hand_;
    }

    size_t size() const override
    {
        return list_.size();
    }

    void advance_clock() override
    {
        if (++clock_hand_ == list_.end())
        {
            clock_hand_ = list_.begin();
        }
    }

private:
    std::list<Key> list_;
    typename std::list<Key>::iterator clock_hand_;
};


template <typename Key>
class SecondChanceList : BaseCacheList<Key>
{
public:
    SecondChanceList() = default;

    void push(Key key) override
    {
        inner_list_.push_back(key);
    }

    void remove() override
    {
        inner_list_.pop_front();
    }

    Key head() override
    {
        return inner_list_.front();
    }

    size_t size() const override
    {
        return inner_list_.size();
    }

    void advance_clock() override
    {}

private:
    std::list<Key> inner_list_;
};


template<typename Key, typename Value, typename EntryAlloc>
class CarCache
{

    using KeyType = Key;
    using ValueType = Value;

    struct Entry
    {
        int access_bit;
        bool is_history;
        Value value;
    };

public:

    explicit
    CarCache(size_t capacity)
        : capacity_(capacity),
        entry_alloc_(),
        cache_size_(capacity / 2),
        cache_recency_(),
        cache_frequency_(),
        history_frequency_(),
        history_recency_(),
        target_size_(0),
        cache_misses_(0)
    {
    }

    Value get(Key key)
    {
        if (!check_cache_presence(key))
        {
            handle_cache_miss(key);
        }
        else
        {
            data_map_[key].access_bit = true;
        }

        return data_map_[key].value;
    }

    bool check_cache_presence(Key const & key) const
    {
        if (data_map_.find(key) != data_map_.end())
        {
            if (!data_map_.at(key).is_history)
            {
                return true;
            }
        }
        return false;
    }

    auto get_contents_keys()
    {
        std::vector<Key> keys;
        keys.reserve(data_map_.size());
        for (auto & item : data_map_)
        {
            keys.push_back(item.first);
        }
        return keys;
    }

    uint64_t get_cache_misses()
    {
        return cache_misses_;
    }

private:

    size_t capacity_;
    size_t cache_size_;
    size_t target_size_;
    ClockList<Key> cache_recency_;
    ClockList<Key> cache_frequency_;
    LruList<Key> history_recency_;
    LruList<Key> history_frequency_;
    EntryAlloc entry_alloc_;
    uint64_t cache_misses_;

    std::unordered_map<Key, Entry> data_map_;

    //TODO: remove duplicate code
    bool evict_from_recency_cache()
    {
        cache_recency_.advance_clock();
        auto victim_element = cache_recency_.head();
        if (data_map_[victim_element].access_bit == 0)
        {
            data_map_[victim_element].is_history = true;
            history_recency_.make_mru(victim_element);
            cache_recency_.remove();
            return true;
        }
        else
        {
            data_map_[victim_element].access_bit = 0;
            cache_frequency_.push(victim_element);
            cache_recency_.remove();
        }
    }

    bool evict_from_frequency_cache()
    {
        cache_recency_.advance_clock();
        auto victim_element = cache_frequency_.head();
        if (data_map_[victim_element].access_bit == 0)
        {
            data_map_[victim_element].is_history = true;
            history_frequency_.make_mru(victim_element);
            cache_frequency_.remove();
            return true;
        }
        else
        {
            data_map_[victim_element].access_bit = 0;
            cache_frequency_.remove();
            cache_frequency_.push(victim_element);
        }
    }

    void evict_entry_from_cache()
    {
        while (true)
        {
            if (cache_recency_.size() >= std::max((uint64_t) 1, (uint64_t) target_size_))
            {
                if (evict_from_recency_cache())
                {
                    return;
                }
            }
            else
            {
                if (evict_from_frequency_cache())
                {
                    return;
                }
            }
        }
    }

    void evict_from_history(Key key)
    {
        if (!history_recency_.check_presence(key) && !history_frequency_.check_presence(key))
        {
            if (cache_recency_.size() + history_recency_.size() == cache_size_)
            {
                auto removed_key = history_recency_.remove_lru();
                data_map_.erase(removed_key);
            }
            else if (cache_recency_.size() + cache_frequency_.size()
                     + history_recency_.size() + history_frequency_.size() == capacity_)
            {
                auto removed_key = history_frequency_.remove_lru();
                data_map_.erase(removed_key);
            }
        }
    }

    void replace(Key key)
    {
        evict_entry_from_cache();
        evict_from_history(key);
    }

    Value handle_cache_miss(Key key)
    {
        if (cache_frequency_.size() + cache_recency_.size() == cache_size_)
        {
            replace(key);
        }

        if (!history_frequency_.check_presence(key) && !history_recency_.check_presence(key))
        {
            data_map_.insert({key, {0, false, entry_alloc_(key)}});
            ++cache_misses_;
            cache_recency_.push(key);
        }
        else
        {
            if (history_recency_.check_presence(key))
            {
                grow_recency_history();
                history_recency_.erase(key);
            }
            else
            {
                decrease_recency_history();
                history_frequency_.erase(key);
            }

            data_map_[key].access_bit = 0;
            data_map_[key].is_history = false;
            cache_frequency_.push(key);
        }
    }

    void grow_recency_history()
    {
        const unsigned long long growth_factor = history_frequency_.size() / history_recency_.size();
        target_size_ = std::min(target_size_ + std::max(1ULL, growth_factor), (unsigned long long) cache_size_);
    }

    void decrease_recency_history()
    {
        const unsigned long long growth_factor = history_recency_.size() / history_frequency_.size();
        target_size_ = std::max(target_size_ - std::max(1ULL, growth_factor), 0ULL);
    }
};


class CartCache
{
public:

private:

};


#endif //CACHINGPP_CACHE_H