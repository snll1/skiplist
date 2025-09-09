/**
The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <vector>

namespace skiplist {

template <typename K, typename V>
class SkipList {
   public:
    virtual bool insert(const K& key, const V& value) = 0;
    virtual bool remove(const K& key) = 0;
    virtual std::pair<bool, V> search(const K& key) const = 0;
    virtual void dump() const = 0;
    virtual void for_each(const std::function<void(const K& key, const V& value)>& cb) const = 0;
};

template <typename K, typename V>
class FatSkipList : public SkipList<K, V> {
   private:
    struct Node {
        K key;
        V value;
        std::vector<Node*> forward;
        Node() = default;
        Node(const K& k, const V& v, int level) : key(k), value(v) {
            forward.resize(level + 1, nullptr);
        }
    };

   public:
    FatSkipList(int max_level = 16, double prob = 0.5) : cur_level_(0), max_level_(max_level), probability_(prob), gen_(rd_()) {
        header_ = new Node(K(), V(), max_level_);
    }

    ~FatSkipList() {
        auto node = header_;
        while (node) {
            auto tmp = node;
            node = node->forward[0];
            delete tmp;
        }
    };

    bool insert(const K& key, const V& value) override {
        std::lock_guard lock(mutex_);
        std::vector<Node*> to_be_updated;
        auto node = find_node(key, &to_be_updated);
        if (node && node->key == key) {
            node->value = value;
            return false;
        }

        int node_level = get_rand_level();
        if (node_level > cur_level_) {
            for (int i = cur_level_ + 1; i <= node_level; i++) {
                to_be_updated[i] = header_;
            }
            cur_level_ = node_level;
        }

        auto new_node = new Node(key, value, node_level);
        for (int level = 0; level <= node_level; level++) {
            new_node->forward[level] = to_be_updated[level]->forward[level];
            to_be_updated[level]->forward[level] = new_node;
        }

        return true;
    }

    bool remove(const K& key) override {
        std::lock_guard lock(mutex_);
        std::vector<Node*> to_be_updated;
        auto node = find_node(key, &to_be_updated);
        if (!node || node->key != key) {
            return false;
        }

        for (int level = 0; level <= cur_level_; level++) {
            if (to_be_updated[level]->forward[level] != node) break;
            to_be_updated[level]->forward[level] = node->forward[level];
        }

        delete node;

        while (cur_level_ > 0 && header_->forward[cur_level_] == nullptr) {
            cur_level_--;
        }
        return true;
    }

    std::pair<bool, V> search(const K& key) const override {
        std::lock_guard lock(mutex_);
        auto node = find_node(key);
        if (node && node->key == key) {
            return {true, node->value};
        }

        return {false, V{}};
    }

    void dump() const override {
        for (int level = cur_level_; level >= 0; level--) {
            std::cout << "Level: " << level << " Keys: ";
            auto node = header_->forward[level];
            while (node) {
                std::cout << "(" << node->key << "," << node->value << ") ";
                // std::cout << node->key << " ";
                node = node->forward[level];
            }
            std::cout << std::endl;
        }
    }

    void for_each(const std::function<void(const K& key, const V& value)>& cb) const override {
        auto node = header_->forward[0];
        while (node) {
            cb(node->key, node->value);
            node = node->forward[0];
        }
    }

   private:
    Node* find_node(const K& key, std::vector<Node*>* to_be_updated = nullptr) const {
        Node* node = header_;
        if (to_be_updated) {
            (*to_be_updated).resize(max_level_ + 1, nullptr);
        }

        for (int level = cur_level_; level >= 0; level--) {
            while (node->forward[level] && node->forward[level]->key < key) {
                node = node->forward[level];
            }
            if (to_be_updated) {
                (*to_be_updated)[level] = node;
            }
        }

        node = node->forward[0];
        return node;
    }

    int get_rand_level() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        int level = 0;
        while (dist(gen_) < probability_ && level < max_level_) {
            level++;
        }
        return level;
    }

   private:
    int cur_level_;
    int max_level_;
    double probability_;
    Node* header_;
    std::random_device rd_;
    std::mt19937 gen_;
    mutable std::mutex mutex_;
};

}  // namespace skiplist
