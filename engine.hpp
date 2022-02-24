// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <algorithm>
#include <vector>
#include <unordered_map>

#include "string.h"
#include "io.h"
#include <mutex>
#include <queue>
#include <atomic>

class Node {
public:
    Node* next;
    input i;
    uint32_t exec_id;
    std::mutex node_mutex;

    Node(input input, Node* next = nullptr) {
        this->i = input;
        this->exec_id = 0;
        this->next = next;
    }

    // Copy Constructor
    Node(const Node& other) { 
        this->i = other.i; 
        this->exec_id = other.exec_id;
        this->next = other.next;
    }

    // Copy-Assignment Constructor
    Node& operator=(const Node& other) {
        this->i = other.i;
        this->exec_id = other.exec_id;
        this->next = other.next;
        return *this;
    }
};

class OrderLinkedList {
private:
    Node* head_;
    Node* tail_;
    bool sort_asc;
public:
    std::mutex print_mutex;
    // Initialize head_ to point to tail_ (empty list).
    OrderLinkedList(bool sort_asc) {
        this->sort_asc = sort_asc;
        tail_ = new Node({input_buy, 0, 0, 0, "0"});
        head_ = new Node({input_buy, 0, 0, 0, "0"}, tail_);
    }

    OrderLinkedList(const OrderLinkedList&) = delete;
    OrderLinkedList& operator=(const OrderLinkedList&) = delete;

    void tryInsert(input i, int64_t input_time);
    input tryMatch(input i, int64_t input_time);
    bool tryCancel(input i);
};

class Engine {
    void ConnectionThread(ClientConnection);
    std::vector<Node> buy_vector;
    std::vector<Node> sell_vector;
    OrderLinkedList buy_orders{false};
    OrderLinkedList sell_orders{true};

public:
    void Accept(ClientConnection);
    void tryCancel(input cancel_order, int64_t input_time);

    std::atomic<input_type> lastOrderType{ input_none };

    std::mutex switch_mutex;
    std::mutex print_mutex;
};

inline static std::chrono::microseconds::rep CurrentTimestamp() noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
#endif
