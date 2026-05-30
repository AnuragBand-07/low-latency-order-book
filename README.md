# Low-Latency Order Book Matching Engine

A from-scratch limit order book matching engine written in C++, implementing the core data structures used in real high-frequency trading (HFT) systems. Processes **1,000,000 orders in under 1 second**.

## Architecture

The engine uses three data structures layered together:

```
Price Level Map (Red-Black Tree)
  └── 101.00 → [Order A] ↔ [Order B] ↔ [Order C]   (Doubly Linked List)
  └── 100.50 → [Order D]
  └── 100.00 → [Order E] ↔ [Order F]

Order Lookup (Hash Map)
  └── order_id → OrderNode*   (O(1) pointer directly into the DLL)
```

**`std::map` (Red-Black Tree)** — Maintains all price levels in sorted order. Bids are sorted descending (highest buyer first); asks ascending (lowest seller first). Best bid/ask is always `O(1)` via `.begin()`; inserting a new price level is `O(log P)`.

**Custom `PriceLevel` (Doubly Linked List)** — Each price level holds a FIFO queue of orders. Appending a new order is `O(1)` via a tail pointer. Cancelling any order mid-queue is also `O(1)` — pointer surgery, no shifting.

**`std::unordered_map` (Hash Map)** — Maps every live `order_id` to its `OrderNode*`. Lets `cancelOrder` skip the tree entirely and jump straight to the node in `O(1)`.

## Complexity

| Operation | Data Structure | Time Complexity |
|---|---|---|
| `addOrder` (no match) | Red-Black Tree + DLL tail insert | O(log P) |
| `addOrder` (with match) | Red-Black Tree + DLL head pop | O(log P) per fill |
| `cancelOrder` | Hash Map + DLL pointer splice | O(1) average |
| Best bid/ask lookup | Red-Black Tree `.begin()` | O(1) |
| Price level cleanup (empty) | Red-Black Tree erase | O(log P) |

*P = number of distinct price levels in the book*

## Benchmark Results

Tested on Apple Silicon (M-series), single-threaded, compiled with `g++ -O2`:

```
Total Orders Processed:   1,000,000
Total Orders Cancelled:   100,000
Total Time Taken:         862 ms
Throughput:               ~1,160,000 orders/sec
```

Random prices drawn uniformly from [99.00, 101.00], quantities from [1, 100], 50/50 bid/ask split. Fixed seed (42) for reproducibility.

## Build & Run

```bash
g++ -std=c++17 -O2 -o engine main.cpp OrderBook.cpp
./engine
```
