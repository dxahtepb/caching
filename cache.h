//
// Created by student on 02.02.19.
//

#ifndef CACHINGPP_CACHE_H
#define CACHINGPP_CACHE_H


#include <unordered_map>
#include <list>
#include <vector>
#include <mutex>


template <typename Key, typename Value, typename EntryAlloc>
class BaseCache
{
public:
    virtual Value get(Key key) = 0;
    virtual bool check_cache_presence(Key const& key) = 0;
    virtual uint64_t get_cache_misses() const = 0;
    virtual size_t size() = 0;
    virtual std::string name() const = 0;
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
            list_.erase(map_.at(key));
        }
        list_.push_front(key);
        map_[key] = list_.begin();
    }

    Key remove_lru()
    {
        auto ret = list_.back();
        map_.erase(list_.back());
        list_.pop_back();
        return ret;
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
        clock_hand_ = list_.erase(clock_hand_);
        advance_clock();
        //if (clock_hand_ == list_.end())
        //{
        //    clock_hand_ = list_.begin();
        //}
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


template <typename Key, typename Value, typename EntryAlloc>
class LruCache : public BaseCache<Key, Value, EntryAlloc>
{
public:
    explicit
    LruCache(size_t cache_size)
            : cache_list_(),
              data_(),
              cache_misses_(0),
              entry_alloc_(),
              cache_size_(cache_size)
    {}

    Value get(Key key) override
    {
        std::lock_guard<std::mutex> lck {mtx};
        if (!check_cache_presence(key))
        {
            ++cache_misses_;
            if (cache_list_.size() == cache_size_)
            {
                auto removed_key = cache_list_.remove_lru();
                data_.erase(removed_key);
            }
            data_[key] = entry_alloc_(key);
            cache_list_.make_mru(key);
        }
        else
        {
            cache_list_.make_mru(key);
        }

        return data_.at(key);
    }

    bool check_cache_presence(Key const & key) override
    {
        return data_.find(key) != data_.end();
    }

    uint64_t get_cache_misses() const override
    {
        return cache_misses_;
    }

    size_t size() override
    {
        return data_.size();
    }

    std::string name() const override
    {
        return "LRU";
    }

private:
    LruList<Key> cache_list_;
    std::unordered_map<Key, Value> data_;
    EntryAlloc entry_alloc_;

    uint64_t cache_misses_;
    size_t cache_size_;

    std::mutex mtx;
};


template<typename Key, typename Value, typename EntryAlloc>
class CarCache : public BaseCache<Key, Value, EntryAlloc>
{
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
              cache_misses_(0),
              data_map_()
//              f("log.log")
    {
    }

    Value get(Key key) override
    {
        std::lock_guard<std::mutex> lock_guard{mtx};
        if (!check_cache_presence(key))
        {
            handle_cache_miss(key);
        }
        else
        {
            data_map_[key].access_bit = true;
        }

//        f << "CAR: full size: " << size() << '\n';
//        f << "CAR: recencyClock size: " << cache_recency_.size() << '\n';
//        f << "CAR: frequencyClock size: " << cache_frequency_.size() << '\n';
//        f << "CAR: recencyHistory size: " << history_recency_.size() << '\n';
//        f << "CAR: frequencyHistory size: " << history_frequency_.size() << '\n';

        return data_map_.at(key).value;
    }

    bool check_cache_presence(Key const & key) override
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

    uint64_t get_cache_misses() const override
    {
        return cache_misses_;
    }

    size_t size() override
    {
        return cache_frequency_.size() + cache_recency_.size() + history_frequency_.size() + history_recency_.size();
    }

    size_t get_target_size()
    {
        return target_size_;
    }

    std::string name() const override
    {
        return "CAR";
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

    std::mutex mtx;

    std::unordered_map<Key, Entry> data_map_;

//    std::ofstream f;

    void remove_from_cache(ClockList<Key>& cache_list, LruList<Key>& history_list, Key const& victim_element)
    {
        data_map_[victim_element].is_history = true;
        history_list.make_mru(victim_element);
        cache_list.remove();
    }

    Key get_victim_element(ClockList<Key>& cache_list)
    {
        cache_list.advance_clock();
        return cache_list.head();
    }

    bool evict_from_recency_cache()
    {
        Key victim_element = get_victim_element(cache_recency_);
        if (data_map_[victim_element].access_bit == 0)
        {
            remove_from_cache(cache_recency_, history_recency_, victim_element);
            return true;
        }
        else
        {
            data_map_[victim_element].access_bit = 0;
            cache_frequency_.push(victim_element);
            cache_recency_.remove();
        }
        return false;
    }

    bool evict_from_frequency_cache()
    {
        Key victim_element = get_victim_element(cache_frequency_);
        if (data_map_[victim_element].access_bit == 0)
        {
            remove_from_cache(cache_frequency_, history_frequency_, victim_element);
            return true;
        }
        else
        {
            data_map_[victim_element].access_bit = 0;
        }
        return false;
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
                grow_recency_cache();
                history_recency_.erase(key);
            }
            else
            {
                decrease_recency_cache();
                history_frequency_.erase(key);
            }

            data_map_[key].access_bit = 0;
            data_map_[key].is_history = false;
            cache_frequency_.push(key);
        }
    }

    void grow_recency_cache()
    {
        const unsigned long long growth_factor = history_frequency_.size() / history_recency_.size();
        target_size_ = std::min(target_size_ + std::max(1ULL, growth_factor), (unsigned long long) cache_size_);
    }

    void decrease_recency_cache()
    {
        const unsigned long long growth_factor = history_recency_.size() / history_frequency_.size();
        target_size_ = std::max(target_size_ - std::max(1ULL, growth_factor), 0ULL);
    }
};



template<typename Key, typename Value, typename EntryAlloc>
class CartCache : public BaseCache<Key, Value, EntryAlloc>
{
    struct Entry
    {
        char filter_bit;
        int access_bit;
        bool is_history;
        Value value;
    };
public:
    explicit
    CartCache(size_t capacity)
            : capacity_(capacity),
              entry_alloc_(),
              cache_size_(capacity / 2),
              cache_recency_(),
              cache_frequency_(),
              history_frequency_(),
              history_recency_(),
              target_cache_size_(0),
              target_history_size_(0),
              long_pages_count_(0),
              short_pages_count_(0),
              cache_misses_(0),
              data_map_()
    {
    }

    Value get(Key key) override
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

    bool check_cache_presence(Key const & key) const override
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

    uint64_t get_cache_misses() const override
    {
        return cache_misses_;
    }

    size_t size() const override
    {
        return data_map_.size();
    }

    std::string name() const override
    {
        return "CART";
    }

private:
    size_t capacity_;
    size_t cache_size_;
    size_t target_cache_size_;
    size_t target_history_size_;
    size_t long_pages_count_;
    size_t short_pages_count_;

    SecondChanceList<Key> cache_recency_;
    SecondChanceList<Key> cache_frequency_;
    LruList<Key> history_recency_;
    LruList<Key> history_frequency_;

    EntryAlloc entry_alloc_;

    uint64_t cache_misses_;

    std::unordered_map<Key, Entry> data_map_;

    void evict_from_cache()
    {
        while (cache_frequency_.size()
               && data_map_[cache_frequency_.head()].access_bit == 1)
        {
            auto frequency_head = cache_frequency_.head();
            cache_recency_.push(frequency_head);
            cache_frequency_.remove();
            data_map_[frequency_head].access_bit = 0;
            if (cache_frequency_.size() + history_frequency_.size()
                + cache_recency_.size() - short_pages_count_ >= cache_size_)
            {
                target_history_size_ = std::min(target_history_size_ + 1, 2 * cache_size_ - cache_recency_.size());
            }
        }

        while (cache_recency_.size()
               && (data_map_[cache_recency_.head()].filter_bit == 'L'
                   || data_map_[cache_recency_.head()].access_bit == 1))
        {
            if (data_map_[cache_recency_.head()].access_bit == 1)
            {
                auto moved_page = cache_recency_.head();
                cache_recency_.push(moved_page);
                cache_recency_.remove();
                data_map_[moved_page].access_bit == 0;
                if (cache_recency_.size() >= std::min(target_cache_size_ + 1, history_recency_.size())
                    && data_map_[moved_page].filter_bit == 'S')
                {
                    data_map_[moved_page].filter_bit = 'L';
                    ++long_pages_count_;
                    --short_pages_count_;
                }
            }
            else
            {
                auto moved_page = cache_recency_.head();
                cache_recency_.remove();
                cache_frequency_.push(moved_page);
                data_map_[moved_page].access_bit = 0;
                target_history_size_ = std::max(target_history_size_ - 1, cache_size_ - cache_recency_.size());
            }
        }

        if (cache_recency_.size() >= std::max((size_t) 1ULL, target_cache_size_))
        {
            history_recency_.make_mru(cache_recency_.head());
            cache_recency_.remove();
            --short_pages_count_;
        }
        else
        {
            history_frequency_.make_mru(cache_frequency_.head());
            cache_frequency_.remove();
            --long_pages_count_;
        }
    }

    void handle_cache_miss(Key key)
    {
        if (cache_frequency_.size() + cache_recency_.size() == cache_size_)
        {
            evict_from_cache();

            if (!history_recency_.check_presence(key) && !history_frequency_.check_presence(key))
            {
                if (history_recency_.size() > std::max((size_t) 0, target_history_size_)
                    || history_frequency_.size() == 0)
                {
                    auto removed_entry = history_recency_.remove_lru();
                    data_map_.erase(removed_entry);
                }
                else if (history_recency_.size() + history_frequency_.size() == cache_size_ + 1)
                {
                    auto removed_entry = history_frequency_.remove_lru();
                    data_map_.erase(removed_entry);
                }
            }
        }

        if (!history_frequency_.check_presence(key) && !history_recency_.check_presence(key))
        {
            data_map_.insert({key, {'S', 0, false, entry_alloc_(key)}});
            ++cache_misses_;
            ++short_pages_count_;
            cache_recency_.push(key);
        }
        else if (history_recency_.check_presence(key))
        {
            const unsigned long long growth_factor = short_pages_count_ / history_recency_.size();
            target_cache_size_ = std::min(
                    target_cache_size_ + std::max(1ULL, growth_factor),
                    (unsigned long long) cache_size_);
            history_recency_.erase(key);
            cache_recency_.push(key);
            data_map_[key].access_bit = 0;
            data_map_[key].filter_bit = 'L';
            ++long_pages_count_;
        }
        else if (history_frequency_.check_presence(key))
        {
            const unsigned long long growth_factor = long_pages_count_ / history_frequency_.size();
            target_cache_size_ = std::max(target_cache_size_ - std::max(1ULL, growth_factor), 0ULL);
            history_frequency_.erase(key);
            cache_recency_.push(key);
            data_map_[key].access_bit = 0;
            ++long_pages_count_;
            if (cache_frequency_.size() + history_frequency_.size()
                + cache_recency_.size() - short_pages_count_ >= cache_size_)
            {
                target_history_size_ = std::min(target_history_size_ + 1, 2 * cache_size_ - cache_recency_.size());
            }
        }
    }

};


#endif //CACHINGPP_CACHE_H
