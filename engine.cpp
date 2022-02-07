#include "engine.hpp"

#include <iostream>
#include <thread>

#include "io.h"

void Engine::Accept(ClientConnection connection) {
  std::thread thread{&Engine::ConnectionThread, this,
                     std::move(connection)};
  thread.detach();
}

void Engine::ConnectionThread(ClientConnection connection) {
  while (true) {
    input input;
    switch (connection.ReadInput(input)) {
      case ReadResult::Error:
        std::cerr << "Error reading input" << std::endl;
      case ReadResult::EndOfFile:
        return;
      case ReadResult::Success:
        break;
    }
    int64_t input_time = CurrentTimestamp();
    // Functions for printing output actions in the prescribed format are
    // provided in the Output class:
    switch (input.type) {
      case input_cancel:
        std::cout << "Got cancel: ID: " << input.order_id << std::endl;
        Output::OrderDeleted(input.order_id, true, input_time,
                             CurrentTimestamp());
        break;
      default:
        std::cout << "Got order: " << static_cast<char>(input.type) << " "
                  << input.instrument << " x " << input.count << " @ "
                  << input.price << " ID: " << input.order_id
                  << std::endl;
        Output::OrderAdded(input.order_id, input.instrument, input.price,
                           input.count, input.type == input_sell,
                           input_time, CurrentTimestamp());

        break;
    }
    // Additionally:
    Output::OrderExecuted(123, 124, 1, 2000, 10, input_time, CurrentTimestamp());
    // Check the parameter names in `io.h`.
  }
}
