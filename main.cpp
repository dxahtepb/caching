#include <iostream>
#include <cassert>
#include <random>
#include <fstream>
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_set>
#include <algorithm>
#include "cache.h"


std::unordered_map<std::string, std::unordered_map<std::string, int>> SETTINGS = {
        {
            "random_tests", {
                {"test_size", 1000000},
                {"cache_size", 128 * 1024},
                {"random_min", 0},
                {"random_max", 2000000},
                {"threads", 5},
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
void seq_test();

void run_tests()
{
    test_from_file("/home/student/Documents/zipf_distribution_50M2.txt");
    seq_test();
    std::cout << "All tests OK" << std::endl;
}

void seq_test()
{
    std::cout << "sequential test started\n";
    auto cache = std::make_unique<CarCache<uint64_t, uint64_t, A>>(16384);

    for (size_t i = 0; i < 40000; ++i)
    {
        assert(cache->get(i) == i);
    }

    std::cout << "sequential test finished\n";
}

void test_from_file(std::string const& file_path)
{
    std::cout << "testing from file \"" << file_path << "\" started\n";

    auto current_settings = SETTINGS.at("random_tests");
    const size_t CACHE_SIZE = current_settings.at("cache_size");
    const size_t THREADS_NUM = current_settings.at("threads");

    std::vector<std::unique_ptr<BaseCache<uint64_t, uint64_t, A>>> caches;
    caches.push_back(std::make_unique<CarCache<uint64_t, uint64_t, A>>(CACHE_SIZE));
    caches.push_back(std::make_unique<LruCache<uint64_t, uint64_t, A>>(CACHE_SIZE));

    std::unordered_map<std::string, std::chrono::nanoseconds> times;
    for (auto const& cache : caches)
    {
        times.insert({cache->name(), std::chrono::nanoseconds{0}});
    }

    std::ifstream fin(file_path);
    size_t test_size = 0;

    std::vector<uint64_t> queries;
    queries.reserve(10 * 1000 * 1000);

    while (!fin.eof())
    {
        if (test_size == 10 * 1000 * 1000)
        {
            break;
        }

        uint64_t number = 0;
        fin >> number;
        queries.push_back(number);
        ++test_size;
    }

    auto workload = [file_path, &caches, &times] (std::string thread_name, std::vector<uint64_t> queries)
    {
        std::this_thread::sleep_for(std::chrono::seconds{rand() % 10});

        std::random_device rd;
        std::mt19937 g{rd()};
        std::shuffle(queries.begin(), queries.end(), g);

        for (size_t i = 0; i < queries.size(); ++i)
        {
            auto number = queries[i];

            if (i % (1 * 1000 * 1000) == 0)
            {
                std::cout << thread_name << ": " << i << '\n';
            }

            for (auto & cache : caches)
            {
                times.at(cache->name()) += measure_time<std::chrono::nanoseconds>(
                        [number, &cache] ()
                        {
                            assert(number == cache->get(number));
                        });
            }
        }
    };

    std::vector<std::thread> testing_threads;
    for (size_t i = 0; i < THREADS_NUM; ++i)
    {
        std::string thread_name = "thread_" + std::to_string(i);
        testing_threads.emplace_back(workload, thread_name, queries);
    }
    for (auto & i : testing_threads)
    {
        i.join();
    }

    test_size *= THREADS_NUM;
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
