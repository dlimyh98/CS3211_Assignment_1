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
#include <condition_variable>

class Engine {
  void ConnectionThread(ClientConnection);
  std::vector<input> buy_vector;
  std::vector<input> sell_vector;
  std::unordered_map<uint32_t, int> buy_map;
  std::unordered_map<uint32_t, int> sell_map;
  std::condition_variable condition_var;

 public:
  void Accept(ClientConnection);
  void trySell(input sell_order, int64_t input_time);
  void tryBuy(input buy_order, int64_t input_time);
  void tryCancel(input cancel_order, int64_t input_time);

  std::mutex producer_mutex;
  std::mutex consumer_mutex;
  std::mutex producer_consumer_mutex;
};

inline static std::chrono::microseconds::rep CurrentTimestamp() noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
#endif
