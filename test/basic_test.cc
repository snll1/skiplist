#include <cassert>
#include <climits>
#include <iostream>
#include <thread>

#include "locked_skiplist.h"
#include "skiplist.h"

using namespace skiplist;
using namespace std;

void test_basic_operations(SkipList< int, std::string >& list) {
    // Test 1: Insert and Search
    assert(list.insert(10, "ten"));
    assert(list.insert(20, "twenty"));
    assert(list.insert(5, "five"));

    auto search_result_1 = list.search(10);
    assert(search_result_1.first && search_result_1.second == "ten");

    auto search_result_2 = list.search(5);
    assert(search_result_2.first && search_result_2.second == "five");

    auto search_result_3 = list.search(20);
    assert(search_result_3.first && search_result_3.second == "twenty");

    // Test 2: Search for non-existent element
    assert(!list.search(15).first);

    // Test 3: Remove
    assert(list.remove(10));
    assert(!list.search(10).first); // Should be gone

    // Test 4: Remove non-existent element
    assert(!list.remove(10));

    std::cout << "Basic functionality tests passed." << std::endl;
}

void test_edge_cases(SkipList< int, std::string >& list) {
    // Test 1: Insert duplicates
    assert(list.insert(100, "100"));
    assert(!list.insert(100, "101")); // Should fail

    // Test 2: Insert/Remove from empty list
    LockedSkipList< int, int > empty_list;
    assert(!empty_list.remove(50));
    assert(!empty_list.search(50).first);

    // Test 3: Insert and remove boundaries
    assert(list.insert(0, "0"));
    assert(list.insert(1000, "1000"));
    assert(list.remove(0));
    assert(list.remove(1000));
    assert(!list.search(0).first);
    assert(!list.search(1000).first);

    std::cout << "Edge case tests passed." << std::endl;
}

void test_sorted_after_large_inserts(SkipList< int, std::string >& list) {
    const int num_inserts = 10000;
    std::vector< int > random_keys(num_inserts);
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate a list of unique random keys
    for (int i = 0; i < num_inserts; ++i) {
        random_keys[i] = gen() % 20000; // Random keys within a range
    }

    // Insert all keys into the list
    for (int key : random_keys) {
        list.insert(key, std::to_string(key));
    }

    // Verify that the list is sorted after all insertions
    int prev_key = INT_MIN;
    list.for_each([&prev_key](const int& key, const string& value) {
        assert(key >= prev_key);
        prev_key = key;
    });

    std::cout << "Large insert and sorted order check passed." << std::endl;
}

void fatskiplist_test() {
    FatSkipList< int, std::string > list;
    test_basic_operations(list);
    test_edge_cases(list);
    test_sorted_after_large_inserts(list);
}

void locked_skiplist_test() {
    LockedSkipList< int, std::string > list;
    test_basic_operations(list);
    test_edge_cases(list);
    test_sorted_after_large_inserts(list);
}

int main() {
    srand(static_cast< unsigned int >(time(nullptr)));
    fatskiplist_test();
    locked_skiplist_test();
    return 0;
}