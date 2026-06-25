"""
plot_latency.py — Renders latency-histogram + CDF plots from benchmark output.

Usage:
    python plot_latency.py
    # produces latency_histogram.png  and  latency_cdf.png

Requires: numpy, matplotlib, pandas  (pip install numpy matplotlib pandas)
"""

import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def percentile_summary(samples: np.ndarray) -> dict:
    return {
        "mean":  float(samples.mean()),
        "p50":   float(np.percentile(samples, 50)),
        "p90":   float(np.percentile(samples, 90)),
        "p95":   float(np.percentile(samples, 95)),
        "p99":   float(np.percentile(samples, 99)),
        "p99.9": float(np.percentile(samples, 99.9)),
        "max":   float(samples.max()),
    }


def plot_histogram(add: np.ndarray, cancel: np.ndarray, out: str) -> None:
    add_p99    = np.percentile(add, 99)
    cancel_p99 = np.percentile(cancel, 99)
    clip = max(add_p99, cancel_p99) * 1.5

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4.5), sharey=True)

    ax1.hist(add[add <= clip], bins=80, color="#1f77b4", alpha=0.85)
    ax1.axvline(np.percentile(add, 50), color="green",  ls="--", lw=1,
                label=f"p50 = {np.percentile(add, 50):.0f} ns")
    ax1.axvline(add_p99, color="red", ls="--", lw=1, label=f"p99 = {add_p99:.0f} ns")
    ax1.set_title(f"addOrder() latency  (n = {len(add):,})")
    ax1.set_xlabel("latency (ns)")
    ax1.set_ylabel("count")
    ax1.legend()

    ax2.hist(cancel[cancel <= clip], bins=80, color="#ff7f0e", alpha=0.85)
    ax2.axvline(np.percentile(cancel, 50), color="green", ls="--", lw=1,
                label=f"p50 = {np.percentile(cancel, 50):.0f} ns")
    ax2.axvline(cancel_p99, color="red", ls="--", lw=1,
                label=f"p99 = {cancel_p99:.0f} ns")
    ax2.set_title(f"cancelOrder() latency  (n = {len(cancel):,})")
    ax2.set_xlabel("latency (ns)")
    ax2.legend()

    fig.suptitle("OrderBook per-operation latency distribution", fontsize=13)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    print(f"wrote {out}")


def plot_cdf(add: np.ndarray, cancel: np.ndarray, out: str) -> None:
    fig, ax = plt.subplots(figsize=(8, 5))
    for samples, label, color in [(add, "addOrder",    "#1f77b4"),
                                  (cancel, "cancelOrder", "#ff7f0e")]:
        sorted_s = np.sort(samples)
        cdf = np.arange(1, len(sorted_s) + 1) / len(sorted_s)
        ax.plot(sorted_s, cdf, label=label, color=color, lw=1.5)

    ax.set_xscale("log")
    ax.set_xlabel("latency (ns, log scale)")
    ax.set_ylabel("cumulative fraction")
    ax.set_title("OrderBook latency CDF")
    ax.grid(True, which="both", ls=":", alpha=0.5)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    print(f"wrote {out}")


def main() -> int:
    try:
        add    = pd.read_csv("add_latency_ns.csv")["latency_ns"].to_numpy()
        cancel = pd.read_csv("cancel_latency_ns.csv")["latency_ns"].to_numpy()
    except FileNotFoundError as e:
        print(f"error: {e}\nrun ./benchmark first to produce the CSVs", file=sys.stderr)
        return 1

    print("addOrder() :", percentile_summary(add))
    print("cancelOrder():", percentile_summary(cancel))

    plot_histogram(add, cancel, "latency_histogram.png")
    plot_cdf(add, cancel, "latency_cdf.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
