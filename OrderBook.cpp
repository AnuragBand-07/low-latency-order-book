#include "OrderBook.h"
#include <algorithm>

// --- PriceLevel ---

void PriceLevel::appendOrder(OrderNode* node) {
    if (head == nullptr) {
        head = node;
        tail = node;
    } else {
        tail->next = node;
        node->prev = tail;
        tail = node;
    }
}

void PriceLevel::removeOrder(OrderNode* node) {
    if (node->prev != nullptr)
        node->prev->next = node->next;
    else
        head = node->next;

    if (node->next != nullptr)
        node->next->prev = node->prev;
    else
        tail = node->prev;
}

// --- OrderBook ---

OrderBook::~OrderBook() {
    for (auto& pair : orderMap)
        delete pair.second;
    orderMap.clear();
    bids.clear();
    asks.clear();
}

// Continuously match the best bid against the best ask while a spread cross exists.
void OrderBook::matchEngine() {
    while (!bids.empty() && !asks.empty() &&
           bids.begin()->first >= asks.begin()->first) {

        auto best_bid = bids.begin();
        auto best_ask = asks.begin();

        PriceLevel& bidList = best_bid->second;
        PriceLevel& askList = best_ask->second;

        OrderNode* bidNode = bidList.head;
        OrderNode* askNode = askList.head;

        int tradeQty = std::min(bidNode->data.quantity, askNode->data.quantity);
        bidNode->data.quantity -= tradeQty;
        askNode->data.quantity -= tradeQty;

        if (bidNode->data.quantity == 0) {
            orderMap.erase(bidNode->data.order_id);
            bidList.removeOrder(bidNode);
            delete bidNode;
            if (bidList.head == nullptr)
                bids.erase(best_bid);
        }

        if (askNode->data.quantity == 0) {
            orderMap.erase(askNode->data.order_id);
            askList.removeOrder(askNode);
            delete askNode;
            if (askList.head == nullptr)
                asks.erase(best_ask);
        }
    }
}

void OrderBook::addOrder(int order_id, OrderSide side, double price, int quantity) {
    Order o = {order_id, side, price, quantity, 0};
    OrderNode* node = new OrderNode(o);

    if (side == OrderSide::BID)
        bids[price].appendOrder(node);
    else
        asks[price].appendOrder(node);

    orderMap[order_id] = node;
    matchEngine();
}

void OrderBook::cancelOrder(int order_id) {
    auto it = orderMap.find(order_id);
    if (it == orderMap.end()) return;

    OrderNode* node = it->second;
    double price = node->data.price;

    if (node->data.side == OrderSide::BID) {
        bids[price].removeOrder(node);
        if (bids[price].head == nullptr)
            bids.erase(price);
    } else {
        asks[price].removeOrder(node);
        if (asks[price].head == nullptr)
            asks.erase(price);
    }

    orderMap.erase(it);
    delete node;
}
