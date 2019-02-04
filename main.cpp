#include <iostream>
#include <cassert>
#include <random>
#include "cache.h"


std::unordered_map<std::string, std::unordered_map<std::string, int>> SETTINGS = {
        {
            "random_tests", {
                {"test_size", 100000},
                {"cache_size", 1024},
                {"random_min", 0},
                {"random_max", 2000},
            },
        },
};

struct A
{
    int operator() (int key) const
    {
        return key;
    }
};

void random_tests();

void run_tests()
{
    CarCache<int, int, A> cache(1024);

    assert(100 == cache.get(100));
    assert(100 == cache.get(100));
    assert(200 == cache.get(200));
    assert(200 == cache.get(200));
    assert(100 == cache.get(100));
//
//    std::cout << 5 << ' ' << cache.get_cache_misses() << '\n';
//
//    for (auto & item : cache.get_contents_keys())
//    {
//        std::cout << item << ' ';
//    }
//    std::cout << '\n';

    random_tests();

    std::cout << "All tests OK" << std::endl;
}

void random_tests()
{
    std::cout << "random_test_started\n";

    auto current_settings = SETTINGS.at("random_tests");

    const int RANDOM_MIN = current_settings.at("random_min");
    const int RANDOM_MAX = current_settings.at("random_max");
    const int TEST_SIZE = current_settings.at("test_size");
    const size_t CACHE_SIZE = current_settings.at("cache_size");

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(RANDOM_MIN, RANDOM_MAX);

    CarCache<int, int, A> cache(CACHE_SIZE);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        int random_page = dist(mt);
        assert(random_page == cache.get(random_page));
    }

    std::cout << TEST_SIZE << ' ' << cache.get_cache_misses() << ' '
              << (double) cache.get_cache_misses()/TEST_SIZE * 100 << "%\n";
}

int main()
{
    run_tests();
    return 0;
}
