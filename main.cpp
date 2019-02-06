#include <iostream>
#include <cassert>
#include <random>
#include <fstream>
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_set>
#include "cache.h"


std::unordered_map<std::string, std::unordered_map<std::string, int>> SETTINGS = {
        {
            "random_tests", {
                {"test_size", 1000000},
                {"cache_size", 128 * 1024},
                {"random_min", 0},
                {"random_max", 2000000},
            },
        },
};

struct A
{
    uint64_t operator() (uint64_t key) const
    {
//        std::this_thread::sleep_for(std::chrono::nanoseconds{50});
        return key;
    }
};

template <typename ChronoTimeSignature>
inline ChronoTimeSignature measure_time(std::function<void (void)> const& lambda)
{
    auto start_time = std::chrono::steady_clock::now();
    lambda();
    auto end_time = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<ChronoTimeSignature>(end_time - start_time);
}

void test_from_file(std::string const&);

void run_tests()
{
    test_from_file("/home/student/Documents/zipf_distribution_50M2.txt");
    std::cout << "All tests OK" << std::endl;
}

void test_from_file(std::string const& file_path)
{
    std::cout << "testing from file \"" << file_path << "\" started\n";
    std::ifstream fin(file_path);

    auto current_settings = SETTINGS.at("random_tests");
    const size_t CACHE_SIZE = current_settings.at("cache_size");

    std::vector<std::unique_ptr<BaseCache<uint64_t, uint64_t, A>>> caches;
    caches.push_back(std::make_unique<CarCache<uint64_t, uint64_t, A>>(CACHE_SIZE));
    caches.push_back(std::make_unique<LruCache<uint64_t, uint64_t, A>>(CACHE_SIZE));

    std::unordered_map<std::string, std::chrono::nanoseconds> times;
    for (auto const& cache : caches)
    {
        times.insert({cache->name(), std::chrono::nanoseconds{0}});
    }

    std::unordered_set<uint64_t> unique_counter_set;

    size_t test_size = 0;
    while (!fin.eof())
    {
        if (test_size == 10 * 1000 * 1000)
        {
//            break;
        }

        uint64_t number = 0;
        fin >> number;
        unique_counter_set.insert(number);

        if (test_size % (1 * 1000 * 1000) == 0)
        {
            std::cout << test_size << '\n';
        }
        ++test_size;

        for (auto & cache : caches)
        {
            times.at(cache->name()) += measure_time<std::chrono::nanoseconds>(
                    [number, &cache] ()
                    {
                        assert(number == cache->get(number));
                    });
        }
    }

    std::cout << unique_counter_set.size() << '\n';

    for (auto const& cache : caches)
    {
        std::cout << cache->name() << ":  " << test_size << ' ' << cache->get_cache_misses() << ' '
                  << (double) (test_size - cache->get_cache_misses()) / (test_size) * 100 << '%'
                  << " duration: " << std::chrono::duration_cast<std::chrono::seconds>(times.at(cache->name())).count()
                  << "\n";
    }

    std::cout << "testing from file \"" << file_path << "\" finished\n";
}

int main()
{
    run_tests();
    return 0;
}
