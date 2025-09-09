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

#include <unordered_set>

#include "skiplist.h"

namespace skiplist {
template <typename K, typename V>
class LockedSkipList : public SkipList<K, V> {
   private:
    struct Node {
        K key;
        V value;
        std::vector<Node*> forward;
        std::atomic<bool> marked = {false};
        std::atomic<bool> fully_linked = {false};
        int node_level = 0;
        std::mutex mtx;

        Node() = delete;
        Node(const K& k, const V& v, int level) : key(k), value(v), node_level(level) {
            forward.resize(level + 1, nullptr);
        }

        void lock() { mtx.lock(); }
        void unlock() { mtx.unlock(); }
    };

   public:
    LockedSkipList(int max_level = 16, double prob = 0.5) : max_level_(max_level), probability_(prob), gen_(rd_()) {
        header_ = new Node(K(), V(), max_level_);
        tail_ = new Node(K(), V(), 0);
        for (int i = 0; i <= max_level_; ++i) {
            header_->forward[i] = tail_;
        }
    }

    ~LockedSkipList() {
        auto curr = header_->forward[0];
        while (curr != tail_) {
            auto tmp = curr;
            curr = curr->forward[0];
            delete tmp;
        }
        delete header_;
        delete tail_;
    }

    std::pair<bool, V> search(const K& key) const override {
        std::vector<Node*> preds(max_level_ + 1), succs(max_level_ + 1);
        auto found_level = find(key, preds, succs);
        if (found_level != -1) {
            auto node = succs[found_level];
            if (node != tail_ && node->key == key && node->fully_linked && !node->marked) {
                return {true, node->value};
            }
        }

        return {false, V{}};
    }

    int find(const K& key, std::vector<Node*>& preds, std::vector<Node*>& succs) const {
        auto pred = header_;
        int found = -1;
        for (int level = max_level_; level >= 0; level--) {
            auto curr = pred->forward[level];
            while (curr != tail_ && key > curr->key) {
                pred = curr;
                curr = curr->forward[level];
            }
            if (found == -1 && curr != tail_ && key == curr->key) {
                found = level;
            }

            preds[level] = pred;
            succs[level] = curr;
        }

        return found;
    }

    bool insert(const K& key, const V& value) override {
        int node_level = get_rand_level();
        std::vector<Node*> preds(max_level_ + 1), succs(max_level_ + 1);
        while (true) {
            int found_level = find(key, preds, succs);
            if (found_level != -1) {
                auto node_found = succs[found_level];
                if (!node_found->marked) {
                    // If the key already exists, we wait till its fully linked
                    // as it may be in process of parallel add and return false
                    while (!node_found->fully_linked) {
                    }
                    break;
                }
                continue;
            }

            bool valid = true;
            std::unordered_set<Node*> locked_nodes;
            for (int level = 0; level <= node_level; level++) {
                auto pred = preds[level];
                auto succ = succs[level];
                if (!locked_nodes.count(pred)) {
                    pred->lock();
                    locked_nodes.insert(pred);
                }

                if (pred->marked || succ->marked || pred->forward[level] != succ) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                // If we cannot succesfully lock all the predecssors, then retry.
                for (auto& node : locked_nodes) {
                    node->unlock();
                }
                continue;
            }

            auto new_node = new Node(key, value, node_level);
            for (int level = 0; level <= node_level; level++) {
                new_node->forward[level] = succs[level];
                preds[level]->forward[level] = new_node;
            }

            new_node->fully_linked = true;
            for (auto& node : locked_nodes) {
                node->unlock();
            }
            return true;
        }
        return false;
    }

    bool remove(const K& key) override {
        std::vector<Node*> preds(max_level_ + 1), succs(max_level_ + 1);
        Node* victim = nullptr;
        bool is_marked = false;
        int node_level = -1;
        while (true) {
            int found_level = find(key, preds, succs);
            if (found_level == -1) {
                // Return false if key not found.
                return false;
            }

            victim = succs[found_level];
            if (is_marked || (found_level != -1 && (victim->fully_linked && victim->node_level == found_level && !victim->marked))) {
                if (!is_marked) {
                    node_level = victim->node_level;
                    victim->lock();
                    if (victim->marked) {
                        // Return false if already removed
                        victim->unlock();
                        return false;
                    }
                    victim->marked = true;
                    is_marked = true;
                }

                std::unordered_set<Node*> locked_nodes;
                bool valid = true;
                for (int level = 0; level <= node_level; level++) {
                    auto pred = preds[level];
                    if (!locked_nodes.count(pred)) {
                        pred->lock();
                        locked_nodes.insert(pred);
                    }
                    if (pred->marked || pred->forward[level] != victim) {
                        valid = false;
                        break;
                    }
                }

                if (!valid) {
                    // If we cannot succesfully lock all the predecssors, then retry.
                    for (auto& node : locked_nodes) {
                        node->unlock();
                    }
                }

                for (int level = node_level; level >= 0; level--) {
                    preds[level]->forward[level] = victim->forward[level];
                }

                victim->unlock();
                delete victim;

                for (auto& node : locked_nodes) {
                    node->unlock();
                }
                return true;
            }
        }
    }

    void dump() const override {
        for (int level = max_level_; level >= 0; level--) {
            auto node = header_->forward[level];
            if (node == tail_) {
                std::cout << level << std::endl;
                continue;
            }

            std::cout << "Level: " << level << " Keys: ";
            while (node != tail_) {
                std::cout << "(" << node->key << "," << node->value << ") ";
                // std::cout << node->key << " ";
                node = node->forward[level];
            }
            std::cout << std::endl;
        }
    }

    void for_each(const std::function<void(const K& key, const V& value)>& cb) const override {
        auto node = header_->forward[0];
        while (node != tail_) {
            cb(node->key, node->value);
            node = node->forward[0];
        }
    }

   private:
    int get_rand_level() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        int level = 0;
        while (dist(gen_) < probability_ && level < max_level_) {
            level++;
        }
        return level;
    }

   private:
    int max_level_;
    double probability_;
    Node* header_;
    Node* tail_;
    std::random_device rd_;
    std::mt19937 gen_;
};

}  // namespace skiplist