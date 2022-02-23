#include "engine.hpp"
#include "io.h"

#include <iostream>
#include <thread>

void Engine::Accept(ClientConnection connection) {
    std::thread thread{ &Engine::ConnectionThread, this,
                       std::move(connection) };
    thread.detach();
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
            tryCancel(input, input_time);
            break;
        case input_buy:
            if (lastOrderType == input_sell)
            {
                std::unique_lock<std::mutex> switchlock(switch_mutex);
                tryBuy(input, input_time);
                lastOrderType = input_buy;
                break;
            }
            else 
            {
                tryBuy(input, input_time);
                break;
            }
        case input_sell:
            if (lastOrderType == input_buy)
            {
                std::unique_lock<std::mutex> switchlock(switch_mutex);
                trySell(input, input_time);
                lastOrderType = input_sell;
                break;
            }
            else {
                trySell(input, input_time);
                break;
            }
        default:
            break;
        }
    }
}

void Engine::trySell(input sell_order, int64_t input_time) {
    // Matching sell_order to all possible buy_orders
    for (Node& buy_node : buy_vector) {

        std::unique_lock<std::mutex> nlock{buy_node.node_mutex};
        std::cout << "Buy node locked" << std::endl;

        if (strcmp(sell_order.instrument, buy_node.i.instrument) == 0 &&
            sell_order.count > 0 && buy_node.i.count > 0 &&
            buy_node.i.price >= sell_order.price) {

            buy_node.exec_id += 1;       // increment execution order of buy_map (since that was the RESTING ORDER)

            if (sell_order.count > buy_node.i.count) {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(buy_node.i.order_id, sell_order.order_id, buy_node.exec_id,
                    sell_order.price, buy_node.i.count, input_time, CurrentTimestamp());

                sell_order.count -= buy_node.i.count;
                buy_node.i.count = 0;
            }
            else if (sell_order.count == buy_node.i.count) {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(buy_node.i.order_id, sell_order.order_id, buy_node.exec_id,
                    sell_order.price, buy_node.i.count, input_time, CurrentTimestamp());

                buy_node.i.count = 0;
                return;
            }
            else {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(buy_node.i.order_id, sell_order.order_id, buy_node.exec_id,
                    sell_order.price, sell_order.count, input_time, CurrentTimestamp());

                buy_node.i.count -= sell_order.count;
                return;
            }
        }

        nlock.unlock();
    }

    // Insert into position
    Node sell_node = Node(sell_order);
    for (auto it = sell_vector.begin(); it != sell_vector.end();)
    {
        std::unique_lock<std::mutex> lk1{it->node_mutex, std::defer_lock};
        std::unique_lock<std::mutex> lk2{std::next(it)->node_mutex, std::defer_lock};
        std::try_lock(lk1, lk2);

        if (std::next(it) == sell_vector.end() ||
            (it->i.price <= sell_order.price && std::next(it)->i.price > sell_order.price))
        {
            sell_vector.insert(it, sell_node);
            break;
        }
        else 
        {
            ++it;
        }

        std::cout << "Order " << sell_order.count << " added" << std::endl;

        lk1.unlock();
        lk2.unlock();
    }

    //sell_vector.insert(std::lower_bound(sell_vector.begin(), sell_vector.end(), sell_order, comparePriceAsc), sell_order);

    std::unique_lock<std::mutex> printLock(print_mutex);
    Output::OrderAdded(sell_order.order_id, sell_order.instrument, sell_order.price,
        sell_order.count, true,
        input_time, CurrentTimestamp());
}

void Engine::tryBuy(input buy_order, int64_t input_time) {
    // Matching buy_order to all possible sell_orders
    for (Node& sell_node : sell_vector) {

        std::unique_lock<std::mutex> nlock{sell_node.node_mutex};

        if (strcmp(buy_order.instrument, sell_node.i.instrument) == 0 &&
            buy_order.count > 0 && sell_node.i.count > 0 &&
            buy_order.price >= sell_node.i.price) {

            //std::cout << "Match found." << std::endl;
            sell_node.exec_id += 1;       // increment execution order of sell_map (since that was RESTING ORDER)

            if (buy_order.count > sell_node.i.count) {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(sell_node.i.order_id, buy_order.order_id, sell_node.exec_id,
                    sell_node.i.price, sell_node.i.count, input_time, CurrentTimestamp());

                buy_order.count -= sell_node.i.count;
                sell_node.i.count = 0;
            }
            else if (buy_order.count == sell_node.i.count) {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(sell_node.i.order_id, buy_order.order_id, sell_node.exec_id,
                    sell_node.i.price, sell_node.i.count, input_time, CurrentTimestamp());

                sell_node.i.count = 0;
                return;
            }
            else {
                std::unique_lock<std::mutex> printLock(print_mutex);
                Output::OrderExecuted(sell_node.i.order_id, buy_order.order_id, sell_node.exec_id,
                    sell_node.i.price, buy_order.count, input_time, CurrentTimestamp());

                sell_node.i.count -= buy_order.count;
                return;
            }
        }

        nlock.unlock();
    }

    // Insert into position
    Node buy_node = Node(buy_order);
    for (auto it = buy_vector.begin(); it != buy_vector.end();)
    {
        std::unique_lock<std::mutex> lk1{ it->node_mutex, std::defer_lock };
        std::unique_lock<std::mutex> lk2{ std::next(it)->node_mutex, std::defer_lock };
        std::try_lock(lk1, lk2);

        if (std::next(it) == buy_vector.end() ||
            (it->i.price >= buy_order.price && std::next(it)->i.price < buy_order.price))
        {
            buy_vector.insert(it, buy_node);
            break;
        }
        else
        {
            ++it;
        }

        lk1.unlock();
        lk2.unlock();
    }

    //buy_vector.insert(std::upper_bound(buy_vector.begin(), buy_vector.end(), buy_order, comparePriceDesc), buy_order);

    std::unique_lock<std::mutex> printLock(print_mutex);
    Output::OrderAdded(buy_order.order_id, buy_order.instrument, buy_order.price,
        buy_order.count, false,
        input_time, CurrentTimestamp());
}

void Engine::tryCancel(input cancel_order, int64_t input_time) {

    // TODO

    Output::OrderDeleted(cancel_order.order_id, true,
        input_time, CurrentTimestamp());
}
