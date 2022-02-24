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

void OrderLinkedList::tryInsert(input i, int64_t input_time) {
    if (i.count <= 0)
        return;

    Node* new_node = new Node(i);
    Node* prev = head_;
    std::unique_lock<std::mutex> prev_lk(prev->node_mutex);
    Node* node = prev->next;
    std::unique_lock<std::mutex> node_lk(node->node_mutex);

    if (sort_asc)
    {
        while (i.price >= node->i.price && node != tail_) {
            prev = node;
            node = node->next;
            prev_lk.swap(node_lk);
            node_lk = std::unique_lock<std::mutex>(node->node_mutex);
        }
    }
    else 
    {
        while (i.price <= node->i.price && node != tail_) {
            prev = node;
            node = node->next;
            prev_lk.swap(node_lk);
            node_lk = std::unique_lock<std::mutex>(node->node_mutex);
        }
    }

    new_node->next = node;
    prev->next = new_node;

    std::unique_lock<std::mutex> printLock(print_mutex);
    Output::OrderAdded(i.order_id, i.instrument, i.price,
        i.count, true,
        input_time, CurrentTimestamp());
}

input OrderLinkedList::tryMatch(input i, int64_t input_time) {
    Node* prev = head_;
    std::unique_lock<std::mutex> prev_lk(prev->node_mutex);
    Node* node = prev->next;
    std::unique_lock<std::mutex> node_lk(node->node_mutex);
    while (node != tail_) {

        // For sell list, node refers to sell node and i refers to buy input
        if (sort_asc) {

            if (strcmp(i.instrument, node->i.instrument) == 0 &&
                i.count > 0 && node->i.count > 0 &&
                i.price >= node->i.price) {

                node->exec_id += 1;       // increment execution order of buy_map (since that was the RESTING ORDER)

                if (i.count > node->i.count) {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, node->i.count, input_time, CurrentTimestamp());

                    i.count -= node->i.count;
                    node->i.count = 0;
                }
                else if (i.count == node->i.count) {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, node->i.count, input_time, CurrentTimestamp());

                    i.count = 0;
                    node->i.count = 0;
                    return i;
                }
                else {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, i.count, input_time, CurrentTimestamp());

                    i.count = 0;
                    node->i.count -= i.count;
                    return i;
                }
            }
        }
        // For buy list, node refers to buy node and i refers to sell input
        else {

            if (strcmp(i.instrument, node->i.instrument) == 0 &&
                i.count > 0 && node->i.count > 0 &&
                node->i.price >= i.price) {

                node->exec_id += 1;       // increment execution order of buy_map (since that was the RESTING ORDER)

                if (i.count > node->i.count) {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, node->i.count, input_time, CurrentTimestamp());

                    i.count -= node->i.count;
                    node->i.count = 0;
                }
                else if (i.count == node->i.count) {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, node->i.count, input_time, CurrentTimestamp());

                    i.count = 0;
                    node->i.count = 0;
                    return i;
                }
                else {
                    std::unique_lock<std::mutex> printLock(print_mutex);
                    Output::OrderExecuted(node->i.order_id, i.order_id, node->exec_id,
                        node->i.price, i.count, input_time, CurrentTimestamp());

                    i.count = 0;
                    node->i.count -= i.count;
                    return i;
                }
            }
        }

        prev = node;
        node = node->next;
        prev_lk.swap(node_lk);
        node_lk = std::unique_lock<std::mutex>(node->node_mutex);
    }
    
    return i;
}

bool OrderLinkedList::tryCancel(input i) {

    // Prep-work to begin traversing down list
    Node* traversal_begin = head_;
    std::unique_lock<std::mutex> traversal_begin_lk(traversal_begin->node_mutex);
    Node* traversal = traversal_begin->next;
    std::unique_lock<std::mutex> traversal_lk(traversal->node_mutex);

    if (traversal->i.order_id == i.order_id) {
        traversal->i.count = 0;
        return true;
    } else {
        // Swap the locks and continue down the list
        traversal_begin = traversal;
        traversal = traversal->next;
        traversal_begin_lk.swap(traversal_lk);
        traversal_lk = std::unique_lock<std::mutex>(traversal->node_mutex);
    }

    return false;
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
        switch (input.type) {
        case input_cancel:
            tryCancel(input, input_time);
            break;
        case input_buy:
            if (lastOrderType.load(std::memory_order_acquire) == input_sell)
            {
                std::unique_lock<std::mutex> switchlock(switch_mutex);
                buy_orders.tryInsert(sell_orders.tryMatch(input, input_time), input_time);
                lastOrderType.store(input_buy, std::memory_order_release);
            }
            else 
            {
                buy_orders.tryInsert(sell_orders.tryMatch(input, input_time), input_time);
            }
            break;
        case input_sell:
            if (lastOrderType.load(std::memory_order_acquire) == input_buy)
            {
                std::unique_lock<std::mutex> switchlock(switch_mutex);
                sell_orders.tryInsert(buy_orders.tryMatch(input, input_time), input_time);
                lastOrderType.store(input_sell, std::memory_order_release);
            }
            else {
                sell_orders.tryInsert(buy_orders.tryMatch(input, input_time), input_time);
            }
            break;
        default:
            break;
        }
    }
}

void Engine::trySell(input sell_order, int64_t input_time) {
    // Matching sell_order to all possible buy_orders
    for (Node& buy_node : buy_vector) {
        //std::unique_lock<std::mutex> nlock{node_mutexes[node_index]};

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

        //nlock.unlock();
    }

    // Insert into position
    Node sell_node = Node(sell_order);
    for (auto it = sell_vector.begin(); it != sell_vector.end();)
    {
        //std::unique_lock<std::mutex> lk1{node_mutexes[node_index], std::defer_lock};
        //std::unique_lock<std::mutex> lk2{node_mutexes[node_index + 1], std::defer_lock};
        //std::try_lock(lk1, lk2);

        if (std::next(it) == sell_vector.end() ||
            (it->i.price <= sell_order.price && std::next(it)->i.price > sell_order.price))
        {
            sell_vector.emplace(it, sell_node);
            break;
        }
        else 
        {
            ++it;
        }
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

        //std::unique_lock<std::mutex> nlock{ node_mutexes[node_index] };

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
    }

    // Insert into position
    Node buy_node = Node(buy_order);
    for (auto it = buy_vector.begin(); it != buy_vector.end();)
    {
        //std::unique_lock<std::mutex> lk1{ node_mutexes[node_index], std::defer_lock };
        //std::unique_lock<std::mutex> lk2{ node_mutexes[node_index + 1], std::defer_lock };
        //std::try_lock(lk1, lk2);

        if (std::next(it) == buy_vector.end() ||
            (it->i.price >= buy_order.price && std::next(it)->i.price < buy_order.price))
        {
            buy_vector.emplace(it, buy_node);
            break;
        }
        else
        {
            ++it;
        }

        //lk1.unlock();
        //lk2.unlock();
    }

    //buy_vector.insert(std::upper_bound(buy_vector.begin(), buy_vector.end(), buy_order, comparePriceDesc), buy_order);

    std::unique_lock<std::mutex> printLock(print_mutex);
    Output::OrderAdded(buy_order.order_id, buy_order.instrument, buy_order.price,
        buy_order.count, false,
        input_time, CurrentTimestamp());
}

void Engine::tryCancel(input cancel_order, int64_t input_time) {
    bool isFoundSell = false;

    bool isFoundBuy = buy_orders.tryCancel(cancel_order);
    if (isFoundBuy) {
        std::unique_lock<std::mutex> printLock(print_mutex);
        Output::OrderDeleted(cancel_order.order_id, isFoundBuy, input_time, CurrentTimestamp());
    }
    else {
        isFoundSell = sell_orders.tryCancel(cancel_order);
        if (isFoundSell) {
            std::unique_lock<std::mutex> printLock(print_mutex);
            Output::OrderDeleted(cancel_order.order_id, isFoundSell, input_time, CurrentTimestamp());
        }
        else {
            std::unique_lock<std::mutex> printLock(print_mutex);
            Output::OrderDeleted(cancel_order.order_id, false, input_time, CurrentTimestamp());
        }
    }
}
