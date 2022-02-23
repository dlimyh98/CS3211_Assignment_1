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
public:
    // Initialize head_ to point to tail_ (empty list).
    OrderLinkedList() {
        tail_ = new Node({input_buy, 0, 0, 0, "0"});
        head_ = new Node({input_buy, 0, 0, 0, "0"}, tail_);
    }

    /*
    * Insert a node with value val at position pos.
    *
    * To ensure mutual exclusion, the locks of the current node and the
    * previous nodes must be acquired for insertion to work. As soon as the
    * locks are acquired the code does roughly the following:
    *
    *      | prev -> node | prev -> node | prev    node |
    *      |              |          ^   |   v      ^   |
    *      |   new_node   |   new_node   |   new_node   |
    */
    void tryInsert(input i, int pos) {
        // TODO: Write code to check before inserting

        Node* new_node = new Node(i);
        Node* prev = head_;
        std::unique_lock<std::mutex> prev_lk(prev->node_mutex);
        Node* node = prev->next;
        std::unique_lock<std::mutex> node_lk(node->node_mutex);
        for (int i = 0; i < pos && node != tail_; i++) {
            prev = node;
            node = node->next;
            prev_lk.swap(node_lk);
            node_lk = std::unique_lock<std::mutex>(node->node_mutex);
        }
        new_node->next = node;
        prev->next = new_node;
    }

    void tryMatch(int pos) {
        // TODO: Write code to match orders instead of just seeing values

        Node* prev = head_;
        std::unique_lock<std::mutex> prev_lk(prev->node_mutex);
        Node* node = prev->next;
        std::unique_lock<std::mutex> node_lk(node->node_mutex);
        for (int i = 0; i < pos && node != tail_; i++) {
            prev = node;
            node = node->next;
            prev_lk.swap(node_lk);
            node_lk = std::unique_lock<std::mutex>(node->node_mutex);
        }
        if (node == tail_) {
            return;
        }
    }
};

class Engine {
  void ConnectionThread(ClientConnection);
  std::vector<Node> buy_vector;
  std::vector<Node> sell_vector;
  //OrderLinkedList buy_orders;
  //OrderLinkedList sell_orders;

 public:
  void Accept(ClientConnection);
  void trySell(input sell_order, int64_t input_time);
  void tryBuy(input buy_order, int64_t input_time);
  void tryCancel(input cancel_order, int64_t input_time);

  std::atomic<input_type> lastOrderType{input_buy};

  std::mutex switch_mutex;
  std::mutex print_mutex;
};

inline static std::chrono::microseconds::rep CurrentTimestamp() noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
#endif
