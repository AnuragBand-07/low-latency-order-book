// benchmark.cpp — Per-operation latency benchmark for the OrderBook matching engine.
//
// Build (NEVER benchmark debug builds — always -O2 or higher):
//     g++ -O2 -std=c++17 -o benchmark benchmark.cpp OrderBook.cpp
//
// Run:
//     ./benchmark
//
// Output:
//   * Per-op latency percentiles (mean, p50, p90, p95, p99, p99.9, max)
//     for addOrder() and cancelOrder().
//   * Aggregate throughput (ops/sec).
//   * Raw latency samples dumped to add_latency_ns.csv / cancel_latency_ns.csv
//     for plotting a histogram (see plot_latency.py).

#include "OrderBook.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;
using namespace std::chrono;
using ns_t = long long;

struct LatencyStats {
    size_t n         = 0;
    double mean_ns   = 0.0;
    ns_t   p50       = 0;
    ns_t   p90       = 0;
    ns_t   p95       = 0;
    ns_t   p99       = 0;
    ns_t   p99_9     = 0;
    ns_t   p_max     = 0;
    double throughput_ops_per_sec = 0.0;
};

static LatencyStats compute(vector<ns_t>& samples) {
    LatencyStats s{};
    if (samples.empty()) return s;

    sort(samples.begin(), samples.end());
    s.n = samples.size();

    long double sum = 0;
    for (ns_t v : samples) sum += v;
    s.mean_ns = static_cast<double>(sum / static_cast<long double>(s.n));

    auto pct = [&](double p) -> ns_t {
        size_t idx = static_cast<size_t>((p / 100.0) * (s.n - 1));
        return samples[idx];
    };
    s.p50   = pct(50.0);
    s.p90   = pct(90.0);
    s.p95   = pct(95.0);
    s.p99   = pct(99.0);
    s.p99_9 = pct(99.9);
    s.p_max = samples.back();

    s.throughput_ops_per_sec = (sum > 0)
        ? static_cast<double>(1e9L * s.n / sum)
        : 0.0;
    return s;
}

static void print_stats(const string& label, const LatencyStats& s) {
    cout << label << " (n = " << s.n << "):\n"
         << "    mean   : " << fixed << setprecision(0) << s.mean_ns << " ns\n"
         << "    p50    : " << s.p50    << " ns\n"
         << "    p90    : " << s.p90    << " ns\n"
         << "    p95    : " << s.p95    << " ns\n"
         << "    p99    : " << s.p99    << " ns\n"
         << "    p99.9  : " << s.p99_9  << " ns\n"
         << "    max    : " << s.p_max  << " ns\n"
         << "    throughput: " << static_cast<long long>(s.throughput_ops_per_sec)
         << " ops/sec\n\n";
}

static void dump_csv(const string& path, const vector<ns_t>& samples) {
    ofstream f(path);
    f << "latency_ns\n";
    for (ns_t v : samples) f << v << "\n";
}

int main() {
    constexpr int      WARMUP_ORDERS = 100'000;
    constexpr int      BENCH_ORDERS  = 1'000'000;
    constexpr int      CANCEL_EVERY  = 10;   // cancel ~10% of measured orders
    constexpr unsigned RNG_SEED      = 42;   // reproducible across runs

    mt19937 gen(RNG_SEED);
    uniform_int_distribution<int> side_dist(0, 1);
    uniform_int_distribution<int> price_dist(9900, 10100);
    uniform_int_distribution<int> qty_dist(1, 100);

    OrderBook engine;

    cout << "Warming up with " << WARMUP_ORDERS << " orders (untimed)...\n";
    for (int i = 1; i <= WARMUP_ORDERS; ++i) {
        OrderSide s = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
        double    p = price_dist(gen) / 100.0;
        int       q = qty_dist(gen);
        engine.addOrder(i, s, p, q);
    }

    cout << "Benchmarking " << BENCH_ORDERS << " addOrder() + "
         << (BENCH_ORDERS / CANCEL_EVERY) << " cancelOrder() calls...\n";

    vector<ns_t> add_lat;
    vector<ns_t> cancel_lat;
    add_lat.reserve(BENCH_ORDERS);
    cancel_lat.reserve(BENCH_ORDERS / CANCEL_EVERY);

    vector<int> cancellable_ids;
    cancellable_ids.reserve(BENCH_ORDERS / CANCEL_EVERY);

    int next_id = WARMUP_ORDERS + 1;
    auto wall_start = steady_clock::now();

    for (int i = 0; i < BENCH_ORDERS; ++i) {
        int       id = next_id++;
        OrderSide s  = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
        double    p  = price_dist(gen) / 100.0;
        int       q  = qty_dist(gen);

        auto t0 = high_resolution_clock::now();
        engine.addOrder(id, s, p, q);
        auto t1 = high_resolution_clock::now();
        add_lat.push_back(duration_cast<nanoseconds>(t1 - t0).count());

        if (i % CANCEL_EVERY == 0) cancellable_ids.push_back(id);
    }

    for (int id : cancellable_ids) {
        auto t0 = high_resolution_clock::now();
        engine.cancelOrder(id);
        auto t1 = high_resolution_clock::now();
        cancel_lat.push_back(duration_cast<nanoseconds>(t1 - t0).count());
    }

    auto wall_end = steady_clock::now();
    double wall_sec = duration_cast<microseconds>(wall_end - wall_start).count() / 1e6;

    auto add_stats    = compute(add_lat);
    auto cancel_stats = compute(cancel_lat);

    cout << "\n========== Benchmark Results ==========\n";
    cout << "Wall-clock time     : " << fixed << setprecision(3) << wall_sec << " s\n";
    cout << "Total ops measured  : "
         << (BENCH_ORDERS + static_cast<long>(cancellable_ids.size())) << "\n";
    cout << "Aggregate throughput: "
         << static_cast<long long>((BENCH_ORDERS + cancellable_ids.size()) / wall_sec)
         << " ops/sec\n\n";

    print_stats("addOrder()",    add_stats);
    print_stats("cancelOrder()", cancel_stats);

    dump_csv("add_latency_ns.csv",    add_lat);
    dump_csv("cancel_latency_ns.csv", cancel_lat);
    cout << "Raw samples written to add_latency_ns.csv / cancel_latency_ns.csv\n";

    return 0;
}
