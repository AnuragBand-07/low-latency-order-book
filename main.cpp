#include <iostream>
#include <map>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <random>
#include <vector>
#include <iomanip>


using namespace std;

// --- ENUMS & STRUCTS ---

enum class OrderSide {
    BID, // Buyer
    ASK  // Seller
};

// The Order: Basic struct containing core details
struct Order {
    int order_id;
    OrderSide side;
    double price;
    int quantity;
    long long timestamp; // Useful for strict time-priority if needed beyond DLL order
};

// The Node: Wrapper for the Doubly Linked List (DLL)
struct OrderNode {
    Order data;
    OrderNode* prev;
    OrderNode* next;

    OrderNode(Order o) : data(o), prev(nullptr), next(nullptr) {}
};

// --- DATA STRUCTURES ---

// The Price Level (Queue): Doubly Linked List representing orders at a specific price
class PriceLevel {
public:
    OrderNode* head;
    OrderNode* tail;

    PriceLevel() : head(nullptr), tail(nullptr) {}

    // TODO: Phase 1 - Handle pushing to the back (new orders)
    void appendOrder(OrderNode* node) {
        if (head == nullptr) {
            head = node;
            tail = node;
        }
        else{
            tail->next = node;
            node->prev = tail;
            //Update the tail pointer to be the incoming node
            tail = node; 
        }

    }

    // TODO: Phase 1 - Handle removing an order in O(1) time
    void removeOrder(OrderNode* node) {
        // 1. Handle the previous node (or update head)
        if (node->prev != nullptr) {
            node->prev->next = node->next; // Skip over current node
        } else {
            head = node->next; // Node was the head, move head forward
        }

        // 2. Handle the next node (or update tail)
        if (node->next != nullptr) {
            node->next->prev = node->prev; // Skip backward over current node
        } else {
            tail = node->prev; // Node was the tail, move tail backward
        }
    }
};

// --- THE ENGINE ---

class OrderBook {
private:
    // The Books (Red-Black Trees)
    // Bid book: sorted descending (highest buyer first) using greater
    map<double, PriceLevel, greater<double>> bids;

    // Ask book: sorted ascending (lowest seller first) using default less
    map<double, PriceLevel> asks;

    // The Lookup Table (Hash Map) for O(1) cancellations
    unordered_map<int, OrderNode*> orderMap;

    // --- PHASE 2: CORE LOGIC ---

    // The continuous loop that checks for crosses
    void matchEngine() {
        // Your beautifully clean while loop condition!
        while(!bids.empty() && !asks.empty() && bids.begin()->first >= asks.begin()->first) {
            
            // Grab references to the maps so we can erase them easily later if needed
            auto best_bid = bids.begin();
            auto best_ask = asks.begin();
    
            PriceLevel& bidlist = best_bid->second;
            PriceLevel& asklist = best_ask->second;
    
            OrderNode* bidNode = bidlist.head;
            OrderNode* askNode = asklist.head;
    
            // The exact trade logic you wrote
            int tradeQty = min(bidNode->data.quantity, askNode->data.quantity);
            bidNode->data.quantity -= tradeQty;
            askNode->data.quantity -= tradeQty;
    
            // ONLY pop if the Bid order is completely filled
            if (bidNode->data.quantity == 0) {
                orderMap.erase(bidNode->data.order_id); // 1. Remove from Hash Map
                bidlist.removeOrder(bidNode);           // 2. Remove from DLL
                delete bidNode;                         // 3. Free Memory
    
                // 4. Prevent Segfault: If the DLL is empty, destroy the PriceLevel in the map
                if (bidlist.head == nullptr) {
                    bids.erase(best_bid);
                }
            }
    
            // ONLY pop if the Ask order is completely filled
            if (askNode->data.quantity == 0) {
                orderMap.erase(askNode->data.order_id);
                asklist.removeOrder(askNode);
                delete askNode;
    
                if (asklist.head == nullptr) {
                    asks.erase(best_ask);
                }
            }
        }
    }

public:
    ~OrderBook() {
        for (auto& pair : orderMap) {
            delete pair.second; // Delete every remaining node
        }
        orderMap.clear();
        bids.clear();
        asks.clear();
    }
    // Does this new order cross the spread?
    void addOrder(int order_id, OrderSide side, double price, int quantity) {
        // 1. Create the Order and physical OrderNode safely in memory
        Order o = {order_id, side, price, quantity, 0}; // 0 is just a dummy timestamp
        OrderNode* node = new OrderNode(o);             // Ask OS for memory!
    
        // 2. Put them in the correct Red-Black tree map
        // (C++ automatically creates the PriceLevel queue if this price is brand new)
        if (side == OrderSide::BID) {
            bids[price].appendOrder(node);
        } else {
            asks[price].appendOrder(node);
        }
    
        // 3. Add to the Hash Map for O(1) lookups/cancellations later
        orderMap[order_id] = node;
    
        // 4. Now that it's in the book, check if it causes any trades!
        matchEngine();
    }

    // Instantly find and remove an order
    void cancelOrder(int order_id) {
        // 1. Safely find the order without accidentally creating a blank one
        auto it = orderMap.find(order_id);
        
        // If it equals .end(), it means the order isn't in the map (already filled/cancelled)
        if (it == orderMap.end()) {
            return; 
        }

        // 2. Extract the node and figure out where it lives
        OrderNode* node = it->second;
        double price = node->data.price;

        // 3. Snip it out of the correct Doubly Linked List
        if (node->data.side == OrderSide::BID) {
            bids[price].removeOrder(node);
            
            // CLEANUP: If the line is now empty, destroy the price level
            if (bids[price].head == nullptr) {
                bids.erase(price);
            }
        } else {
            asks[price].removeOrder(node);
            
            // CLEANUP: If the line is now empty, destroy the price level
            if (asks[price].head == nullptr) {
                asks.erase(price);
            }
        }

        // 4. Delete the node and erase from orderMap
        orderMap.erase(it); // Erasing by iterator is slightly faster than by ID
        delete node;        // Free the RAM!
    }
};

// --- PHASE 3: BENCHMARKING (MAIN) ---

int main() {
    OrderBook engine;

    cout << "Starting Low-Latency Order Book Engine..." << endl;
    cout << "Generating 1,000,000 orders..." << endl;

    // Fast random number generator setup
    mt19937 gen(42); // Fixed seed for consistent benchmarking
    uniform_int_distribution<> side_dist(0, 1);
    uniform_int_distribution<> price_dist(9900, 10100); // Prices between 99.00 and 101.00
    uniform_int_distribution<> qty_dist(1, 100);        // Quantities between 1 and 100

    vector<int> orders_to_cancel;
    int NUM_ORDERS = 1000000;

    auto start = chrono::high_resolution_clock::now();

    // 1. Blast the engine with 1,000,000 orders
    for (int i = 1; i <= NUM_ORDERS; ++i) {
        OrderSide side = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
        double price = price_dist(gen) / 100.0; 
        int qty = qty_dist(gen);

        engine.addOrder(i, side, price, qty);

        // Save 10% of order IDs to test O(1) cancellations later
        if (i % 10 == 0) {
            orders_to_cancel.push_back(i);
        }
    }

    // 2. Blast the engine with 100,000 cancellations
    for (int id : orders_to_cancel) {
        engine.cancelOrder(id);
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    cout << "-----------------------------------" << endl;
    cout << "Engine processing complete." << endl;
    cout << "Total Orders Processed:  " << NUM_ORDERS << endl;
    cout << "Total Orders Cancelled:  " << orders_to_cancel.size() << endl;
    cout << "Total Time Taken:        " << duration.count() << " milliseconds." << endl;
    cout << "Throughput:              " << (NUM_ORDERS / (duration.count() > 0 ? duration.count() : 1)) * 1000 << " orders/second" << endl;
    cout << "-----------------------------------" << endl;

    return 0;
}