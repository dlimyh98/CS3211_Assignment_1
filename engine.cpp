#include "engine.hpp"
#include "io.h"

#include <iostream>
#include <thread>

void Engine::Accept(ClientConnection connection) {
    std::thread thread{ &Engine::ConnectionThread, this,
                       std::move(connection) };
    thread.detach();
    std::cout << "New connection accepted." << std::endl;
}

bool comparePriceAsc(input i1, input i2) {
    return i1.price < i2.price;
}

bool comparePriceDesc(input i1, input i2) {
    return i1.price > i2.price;
}


void Engine::ConnectionThread(ClientConnection connection) {
    while (true) {
        input input;
        switch (connection.ReadInput(input)) {
        case ReadResult::Error:
            std::cerr << "Error reading input." << std::endl;
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
            tryCancel(input, input_time);
            break;
        case input_buy:
            std::cout << "Got order: " << static_cast<char>(input.type) << " "
                << input.instrument << " x " << input.count << " @ "
                << input.price << " ID: " << input.order_id
                << std::endl;
            std::cout << "Comparing..." << std::endl;
            tryBuy(input, input_time);
            break;
        case input_sell:
            std::cout << "Got order: " << static_cast<char>(input.type) << " "
                << input.instrument << " x " << input.count << " @ "
                << input.price << " ID: " << input.order_id
                << std::endl;
            std::cout << "Comparing..." << std::endl;
            trySell(input, input_time);
            break;
        default:
            break;
        }
    }
}

void Engine::trySell(input sell_order, int64_t input_time) {
    std::unique_lock<std::mutex> producerLock(producer_mutex);

    // checking if a particular Sell Order is being registered for the FIRST TIME
    for (input& i : sell_vector) {
        if (i.order_id == sell_order.order_id) {
            sell_map.insert({ sell_order.order_id, 0 });
            break;
        }
    }

    // Matching sell_order to all possible buy_orders
    for (input& buy_order : buy_vector) {

        if (strcmp(sell_order.instrument, buy_order.instrument) == 0 &&
            sell_order.count > 0 && buy_order.count > 0 &&
            buy_order.price >= sell_order.price) {

            std::cout << "Match found." << std::endl;
            buy_map.at(buy_order.order_id) += 1;       // increment execution order of buy_map (since that was the RESTING ORDER)

            if (sell_order.count > buy_order.count) {
                Output::OrderExecuted(buy_order.order_id, sell_order.order_id, buy_map[buy_order.order_id],
                    sell_order.price, buy_order.count, input_time, CurrentTimestamp());

                sell_order.count -= buy_order.count;
                buy_order.count = 0;
            }
            else if (sell_order.count == buy_order.count) {
                Output::OrderExecuted(buy_order.order_id, sell_order.order_id, buy_map[buy_order.order_id],
                    sell_order.price, buy_order.count, input_time, CurrentTimestamp());

                buy_order.count = 0;
                return;
            }
            else {
                Output::OrderExecuted(buy_order.order_id, sell_order.order_id, buy_map[buy_order.order_id],
                    sell_order.price, sell_order.count, input_time, CurrentTimestamp());

                buy_order.count -= sell_order.count;
                return;
            }
        }
    }

    //Buy vector cleanup
    for (auto it = buy_vector.begin(); it != buy_vector.end();) {
        if (it->count == 0) {
            it = buy_vector.erase(it);
            std::cout << "ERASE THE WEAK" << std::endl;
        }
        else {
            ++it;
        }
    }

    std::cout << "Storing remainder..." << std::endl;

    // Insert into position
    sell_vector.insert(std::lower_bound(sell_vector.begin(), sell_vector.end(), sell_order, comparePriceAsc), sell_order);

    Output::OrderAdded(sell_order.order_id, sell_order.instrument, sell_order.price,
        sell_order.count, true,
        input_time, CurrentTimestamp());
}

void Engine::tryBuy(input buy_order, int64_t input_time) {
    std::unique_lock<std::mutex> consumerLock(consumer_mutex);

    // checking if a particular Buy Order is being registered for the FIRST TIME
    for (input& i : buy_vector) {
        if (i.order_id == buy_order.order_id) {
            buy_map.insert({ buy_order.order_id, 0 });
            break;
        }
    }

    // Matching buy_order to all possible sell_orders
    for (input& sell_order : sell_vector) {

        if (strcmp(buy_order.instrument, sell_order.instrument) == 0 &&
            buy_order.count > 0 && sell_order.count > 0 &&
            buy_order.price >= sell_order.price) {

            std::cout << "Match found." << std::endl;
            sell_map.at(sell_order.order_id) += 1;       // increment execution order of sell_map (since that was RESTING ORDER)

            if (buy_order.count > sell_order.count) {
                Output::OrderExecuted(sell_order.order_id, buy_order.order_id, sell_map[sell_order.order_id],
                    sell_order.price, sell_order.count, input_time, CurrentTimestamp());

                buy_order.count -= sell_order.count;
                sell_order.count = 0;
            }
            else if (buy_order.count == sell_order.count) {
                Output::OrderExecuted(sell_order.order_id, buy_order.order_id, sell_map[sell_order.order_id],
                    sell_order.price, sell_order.count, input_time, CurrentTimestamp());

                sell_order.count = 0;
                return;
            }
            else {
                Output::OrderExecuted(sell_order.order_id, buy_order.order_id, sell_map[sell_order.order_id],
                    sell_order.price, buy_order.count, input_time, CurrentTimestamp());

                sell_order.count -= buy_order.count;
                return;
            }
        }
    }

    //Sell vector cleanup
    for (auto it = sell_vector.begin(); it != sell_vector.end();) {
        if (it->count == 0) {
            it = sell_vector.erase(it);
            std::cout << "ERASE THE WEAK" << std::endl;
        }
        else {
            ++it;
        }
    }

    std::cout << "Storing remainder..." << std::endl;

    // Insert into position
    buy_vector.insert(std::upper_bound(buy_vector.begin(), buy_vector.end(), buy_order, comparePriceDesc), buy_order);

    Output::OrderAdded(buy_order.order_id, buy_order.instrument, buy_order.price,
        buy_order.count, false,
        input_time, CurrentTimestamp());
}

void Engine::tryCancel(input cancel_order, int64_t input_time) {

    bool isSuccessful = false;

    for (auto it = sell_vector.begin(); it != sell_vector.end();) {
        if (cancel_order.order_id == it->order_id) {
            isSuccessful = true;
            it = sell_vector.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto it = buy_vector.begin(); it != buy_vector.end();) {
        if (isSuccessful)
            break;

        if (cancel_order.order_id == it->order_id) {
            isSuccessful = true;
            it = buy_vector.erase(it);
            break;
        }
        else {
            ++it;
        }
    }

    if (isSuccessful) {
        std::cout << "Cancel successful." << std::endl;
    }
    else {
        std::cout << "Cancel rejected." << std::endl;
    }

    Output::OrderDeleted(cancel_order.order_id, isSuccessful,
        input_time, CurrentTimestamp());
}
