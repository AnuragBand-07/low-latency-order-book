// Price-time priority limit order book — JS port of the C++ engine
// (OrderBook.h / OrderBook.cpp) for the browser demo.
//
// Same design as the C++ version:
//   - Sorted price levels (bids desc, asks asc)
//   - FIFO doubly linked list per price level (time priority)
//   - order_id -> node hash map for O(1) cancels
//   - Trades execute at the resting (maker) order's price

class OrderNode {
  constructor(orderId, side, price, quantity) {
    this.orderId = orderId;
    this.side = side; // 'BID' | 'ASK'
    this.price = price; // integer ticks (cents) to avoid float keys
    this.quantity = quantity;
    this.prev = null;
    this.next = null;
    this.level = null; // back-pointer to PriceLevel => O(1) cancel
  }
}

class PriceLevel {
  constructor(price) {
    this.price = price;
    this.head = null;
    this.tail = null;
    this.totalQty = 0;
    this.orderCount = 0;
  }

  append(node) {
    if (this.head === null) {
      this.head = node;
      this.tail = node;
    } else {
      this.tail.next = node;
      node.prev = this.tail;
      this.tail = node;
    }
    node.level = this;
    this.totalQty += node.quantity;
    this.orderCount += 1;
  }

  remove(node) {
    if (node.prev !== null) node.prev.next = node.next;
    else this.head = node.next;
    if (node.next !== null) node.next.prev = node.prev;
    else this.tail = node.prev;
    this.totalQty -= node.quantity;
    this.orderCount -= 1;
    node.prev = null;
    node.next = null;
    node.level = null;
  }

  get isEmpty() {
    return this.head === null;
  }
}

class OrderBook {
  constructor() {
    // price(int ticks) -> PriceLevel. Kept sorted via sorted key arrays.
    this.bidLevels = new Map();
    this.askLevels = new Map();
    this.bidPrices = []; // sorted descending
    this.askPrices = []; // sorted ascending
    this.orderMap = new Map(); // orderId -> OrderNode
    this.trades = []; // execution reports (most recent last)
    this.tradeSeq = 0;
  }

  // --- sorted price array helpers (binary search insert/remove) ---

  static _insertSorted(arr, price, descending) {
    let lo = 0;
    let hi = arr.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      const before = descending ? arr[mid] > price : arr[mid] < price;
      if (before) lo = mid + 1;
      else hi = mid;
    }
    arr.splice(lo, 0, price);
  }

  static _removeSorted(arr, price, descending) {
    let lo = 0;
    let hi = arr.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      const before = descending ? arr[mid] > price : arr[mid] < price;
      if (before) lo = mid + 1;
      else hi = mid;
    }
    if (arr[lo] === price) arr.splice(lo, 1);
  }

  bestBid() {
    return this.bidPrices.length ? this.bidPrices[0] : null;
  }

  bestAsk() {
    return this.askPrices.length ? this.askPrices[0] : null;
  }

  _getOrCreateLevel(side, price) {
    const levels = side === 'BID' ? this.bidLevels : this.askLevels;
    let level = levels.get(price);
    if (!level) {
      level = new PriceLevel(price);
      levels.set(price, level);
      if (side === 'BID') OrderBook._insertSorted(this.bidPrices, price, true);
      else OrderBook._insertSorted(this.askPrices, price, false);
    }
    return level;
  }

  _eraseLevelIfEmpty(side, level) {
    if (!level.isEmpty) return;
    if (side === 'BID') {
      this.bidLevels.delete(level.price);
      OrderBook._removeSorted(this.bidPrices, level.price, true);
    } else {
      this.askLevels.delete(level.price);
      OrderBook._removeSorted(this.askPrices, level.price, false);
    }
  }

  _recordTrade(price, quantity, makerId, takerId) {
    this.trades.push({
      seq: ++this.tradeSeq,
      price,
      quantity,
      makerId,
      takerId,
      ts: Date.now(),
    });
    if (this.trades.length > 200) this.trades.splice(0, this.trades.length - 200);
  }

  // Match an incoming order against the opposite book BEFORE resting it.
  // Returns remaining quantity.
  _matchIncoming(side, price, quantity, takerId) {
    const oppPrices = side === 'BID' ? this.askPrices : this.bidPrices;
    const oppLevels = side === 'BID' ? this.askLevels : this.bidLevels;
    const oppSide = side === 'BID' ? 'ASK' : 'BID';

    while (quantity > 0 && oppPrices.length > 0) {
      const bestOpp = oppPrices[0];
      const crosses = side === 'BID' ? price >= bestOpp : price <= bestOpp;
      if (!crosses) break;

      const level = oppLevels.get(bestOpp);
      const maker = level.head;
      const tradeQty = Math.min(quantity, maker.quantity);

      quantity -= tradeQty;
      maker.quantity -= tradeQty;
      level.totalQty -= tradeQty;
      // Trade prints at the maker's (resting) price.
      this._recordTrade(bestOpp, tradeQty, maker.orderId, takerId);

      if (maker.quantity === 0) {
        level.remove(maker);
        this.orderMap.delete(maker.orderId);
        this._eraseLevelIfEmpty(oppSide, level);
      }
    }
    return quantity;
  }

  addOrder(orderId, side, price, quantity) {
    if (quantity <= 0 || this.orderMap.has(orderId)) return false;

    const remaining = this._matchIncoming(side, price, quantity, orderId);

    if (remaining > 0) {
      const node = new OrderNode(orderId, side, price, remaining);
      this._getOrCreateLevel(side, price).append(node);
      this.orderMap.set(orderId, node);
    }
    return true;
  }

  cancelOrder(orderId) {
    const node = this.orderMap.get(orderId);
    if (!node) return false;
    const level = node.level;
    level.remove(node);
    this._eraseLevelIfEmpty(node.side, level);
    this.orderMap.delete(orderId);
    return true;
  }

  // Top N levels for rendering the ladder.
  depth(n) {
    const takeLevels = (prices, levels) =>
      prices.slice(0, n).map((p) => {
        const l = levels.get(p);
        return { price: p, qty: l.totalQty, orders: l.orderCount };
      });
    return {
      bids: takeLevels(this.bidPrices, this.bidLevels),
      asks: takeLevels(this.askPrices, this.askLevels),
    };
  }

  get openOrders() {
    return this.orderMap.size;
  }
}

// Allow node-based testing.
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { OrderBook, PriceLevel, OrderNode };
}
