CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -Isrc
BUILD    := build
CORE     := src/OrderBook.cpp

.PHONY: all run-demo run-bench plots clean

all: $(BUILD)/demo $(BUILD)/benchmark

$(BUILD)/demo: src/main.cpp $(CORE) src/OrderBook.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(CORE)

$(BUILD)/benchmark: bench/benchmark.cpp $(CORE) src/OrderBook.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ bench/benchmark.cpp $(CORE)

$(BUILD):
	mkdir -p $(BUILD)

# Run the scripted matching demo.
run-demo: $(BUILD)/demo
	./$(BUILD)/demo

# Run the latency/throughput benchmark (writes *.csv to the repo root).
run-bench: $(BUILD)/benchmark
	./$(BUILD)/benchmark

# Regenerate latency plots into docs/img/ (needs Python: numpy, pandas, matplotlib).
plots: run-bench
	python3 scripts/plot_latency.py

clean:
	rm -rf $(BUILD) *.csv
