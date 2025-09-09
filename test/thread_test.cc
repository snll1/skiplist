#include <cassert>
#include <climits>
#include <iostream>
#include <thread>

#include "locked_skiplist.h"
#include "skiplist.h"

using namespace skiplist;
using namespace std;

int32_t num_threads = 4;
int64_t num_keys = 100000;

void test_insert() {
    LockedSkipList< int, std::string > list;
    vector<thread> thr_vec;
    int64_t num_keys_per_thread = num_keys / num_threads;
    for(int64_t i = 0; i < num_threads; i++) {
        thr_vec.emplace_back([&list](int64_t start, int64_t numkeys) {
            while(numkeys--) {
                list.insert(start, to_string(start));
                start++;
            }
        }, i * num_keys_per_thread , num_keys_per_thread);
    }

    for(auto& thr : thr_vec) thr.join();
    for(int64_t i = 0; i < num_keys;i++) {
        assert(list.search(i).first);
    }
}

void test_remove() {
    LockedSkipList< int, std::string > list;
    vector<thread> thr_vec;
    int64_t num_keys_per_thread = num_keys / num_threads;
    for(int64_t i = 0; i < num_threads; i++) {
        thr_vec.emplace_back([&list](int64_t start, int64_t numkeys) {
            while(numkeys--) {
                list.remove(start);
                start++;
            }
        }, i * num_keys_per_thread , num_keys_per_thread);
    }

    for(auto& thr : thr_vec) thr.join();

    for(int64_t i = 0; i < num_keys;i++) {
        assert(!list.search(i).first);
    }
}

pair<uint64_t, uint64_t> get_random_range(uint64_t num_keys) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist_int(1, num_keys);
    uint64_t start = dist_int(gen);
    uint64_t end = dist_int(gen);
    if (end < start) swap(start, end);
    return {start, end};
}

void test_random_ops() {
    LockedSkipList< int, std::string > list;
    vector<thread> thr_vec;
    for(int64_t i = 0; i < num_threads; i++) {
        thr_vec.emplace_back([&]() {
            // Do random inserts, removes and search
            {
                auto [start, end] = get_random_range(num_keys);
                while(start < end) {
                    list.insert(start, to_string(start));
                    start++;
                }
            }

            {
                auto [start, end] = get_random_range(num_keys);
                while(start < end) {
                    list.remove(start);
                    start++;
                }
            }

            {
                auto [start, end] = get_random_range(num_keys);
                while(start < end) {
                    list.search(start);
                    start++;
                }
            }
        });
    }

    for(auto& thr : thr_vec) thr.join();
}

void locked_skiplist_thread_test() {

    test_insert();
    test_remove();
    test_random_ops();
}


int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    locked_skiplist_thread_test();
    return 0;
}