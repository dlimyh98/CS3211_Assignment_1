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

class Node {
public:
    input i;
    uint32_t exec_id;
    std::mutex node_mutex;

    Node(input input) {
        this->i = input;
        this->exec_id = 0;
    }

    // Copy Constructor
    Node(const Node& other) { 
        this->i = other.i; 
        this->exec_id = other.exec_id;
    }

    // Copy-Assignment Constructor
    Node& operator=(const Node& other) {
        this->i = other.i;
        this->exec_id = other.exec_id;
        return *this;
    }
};

class Engine {
  void ConnectionThread(ClientConnection);
  std::vector<Node> buy_vector;
  std::vector<Node> sell_vector;

 public:
  void Accept(ClientConnection);
  void trySell(input sell_order, int64_t input_time);
  void tryBuy(input buy_order, int64_t input_time);
  void tryCancel(input cancel_order, int64_t input_time);

  input_type lastOrderType{input_cancel};

  std::mutex switch_mutex;
  std::mutex print_mutex;
};

inline static std::chrono::microseconds::rep CurrentTimestamp() noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
#endif
