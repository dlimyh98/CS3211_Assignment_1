#include "engine.hpp"
#include "io.h"

#include <iostream>
#include <thread>

void Engine::Accept(ClientConnection connection) {
    std::thread thread{ &Engine::ConnectionThread, this,
                       std::move(connection) };
    thread.detach();
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
        i.count, (i.type == input_sell),
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

                    node->i.count -= i.count;
                    i.count = 0;
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

                    node->i.count -= i.count;
                    i.count = 0;
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

    // Traverse down list
    while (traversal != tail_) {
        if (traversal->i.order_id == i.order_id && traversal->i.count > 0) {
            traversal->i.count = 0;
            return true;
        }
        else {
            // Swap the locks and continue down the list
            traversal_begin = traversal;
            traversal = traversal->next;
            traversal_begin_lk.swap(traversal_lk);
            traversal_lk = std::unique_lock<std::mutex>(traversal->node_mutex);
        }
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
