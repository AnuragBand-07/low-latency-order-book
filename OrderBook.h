#pragma once

#include <map>
#include <unordered_map>
#include <functional>

enum class OrderSide {
    BID,
    ASK
};

struct Order {
    int order_id;
    OrderSide side;
    double price;
    int quantity;
    long long timestamp;
};

struct OrderNode {
    Order data;
    OrderNode* prev;
    OrderNode* next;

    OrderNode(Order o) : data(o), prev(nullptr), next(nullptr) {}
};

// FIFO queue of orders at a single price level, backed by a doubly linked list.
// Append and remove are both O(1) — no shifting required.
class PriceLevel {
public:
    OrderNode* head;
    OrderNode* tail;

    PriceLevel() : head(nullptr), tail(nullptr) {}

    void appendOrder(OrderNode* node);
    void removeOrder(OrderNode* node);
};

class OrderBook {
private:
    // Bids: highest price first (descending RB-tree)
    std::map<double, PriceLevel, std::greater<double>> bids;
    // Asks: lowest price first (ascending RB-tree)
    std::map<double, PriceLevel> asks;
    // O(1) lookup by order_id for cancellations
    std::unordered_map<int, OrderNode*> orderMap;

    void matchEngine();

public:
    ~OrderBook();
    void addOrder(int order_id, OrderSide side, double price, int quantity);
    void cancelOrder(int order_id);
};
