// main.cpp — Small scripted demo of the OrderBook matching engine.
//
// For the latency/throughput benchmark see bench/benchmark.cpp.
// Build via the Makefile:  make && ./build/demo

#include "OrderBook.h"

#include <iomanip>
#include <iostream>

static void printTop(const OrderBook& book) {
    double bid = 0.0, ask = 0.0;
    std::cout << "    best bid: ";
    if (book.bestBid(bid)) std::cout << bid; else std::cout << "--";
    std::cout << "  |  best ask: ";
    if (book.bestAsk(ask)) std::cout << ask; else std::cout << "--";
    std::cout << "  |  live orders: " << book.liveOrders() << "\n";
}

int main() {
    OrderBook book;
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "1) Seeding resting orders (no cross yet):\n";
    book.addOrder(1, OrderSide::BID, 100.00, 10);
    book.addOrder(2, OrderSide::BID,  99.50,  5);
    book.addOrder(3, OrderSide::ASK, 101.00,  8);
    book.addOrder(4, OrderSide::ASK, 101.50, 12);
    printTop(book);

    std::cout << "\n2) Incoming ASK @ 100.00 x6 crosses the best bid -> matches:\n";
    book.addOrder(5, OrderSide::ASK, 100.00, 6);
    printTop(book);   // order 5 fully filled; order 1 partially filled (10 -> 4)

    std::cout << "\n3) Cancelling order 2 (resting bid @ 99.50):\n";
    book.cancelOrder(2);
    printTop(book);

    return 0;
}
