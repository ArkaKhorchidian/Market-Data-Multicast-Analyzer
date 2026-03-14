#!/usr/bin/env python3
"""Plot IAT histogram, CDF, and packet rate over time from analyzer CSV output.

Usage:
    analyzer --pcap session.pcap --latency-report --csv output.csv
    python3 scripts/plot_latency.py output.csv
"""

import sys
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


def load_csv(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    required = {"timestamp_ns", "iat_ns"}
    missing  = required - set(df.columns)
    if missing:
        sys.exit(f"CSV missing columns: {missing}")
    df = df[df["iat_ns"] > 0].copy()
    df["timestamp_s"]  = df["timestamp_ns"] / 1e9
    df["iat_us"]       = df["iat_ns"]       / 1e3
    return df


def plot_iat_histogram(ax, iat_us: np.ndarray) -> None:
    ax.hist(iat_us, bins=200, color="steelblue", alpha=0.8, edgecolor="none")
    ax.set_xscale("log")
    ax.set_xlabel("Inter-Arrival Time (µs)")
    ax.set_ylabel("Count")
    ax.set_title("IAT Histogram (log x-axis)")
    ax.xaxis.set_major_formatter(ticker.ScalarFormatter())

    for p, label in [(0.99, "p99"), (0.999, "p99.9")]:
        val = np.percentile(iat_us, p * 100)
        ax.axvline(val, color="red", linestyle="--", linewidth=1)
        ax.text(val * 1.05, ax.get_ylim()[1] * 0.9, label, color="red", fontsize=8)


def plot_iat_cdf(ax, iat_us: np.ndarray) -> None:
    sorted_us = np.sort(iat_us)
    cdf       = np.arange(1, len(sorted_us) + 1) / len(sorted_us)
    ax.plot(sorted_us, cdf, color="steelblue", linewidth=1)
    ax.set_xscale("log")
    ax.set_xlabel("Inter-Arrival Time (µs)")
    ax.set_ylabel("CDF")
    ax.set_title("IAT Cumulative Distribution")
    ax.set_ylim(0, 1)
    ax.xaxis.set_major_formatter(ticker.ScalarFormatter())

    for p, label in [(0.99, "p99"), (0.999, "p99.9")]:
        val = np.percentile(iat_us, p * 100)
        ax.axvline(val, color="red", linestyle="--", linewidth=1)
        ax.axhline(p,   color="red", linestyle=":",  linewidth=0.5)
        ax.text(val * 1.05, p - 0.04, f"{label}={val:.1f}µs",
                color="red", fontsize=7)


def plot_packet_rate(ax, ts_s: np.ndarray) -> None:
    # Rolling 1-second window packet rate
    t0       = ts_s[0]
    rel_s    = ts_s - t0
    duration = rel_s[-1] if rel_s[-1] > 0 else 1.0
    bins     = np.arange(0, duration + 1, 1.0)
    counts, edges = np.histogram(rel_s, bins=bins)
    centres  = (edges[:-1] + edges[1:]) / 2

    ax.plot(centres, counts, color="darkorange", linewidth=1)
    ax.set_xlabel("Elapsed Time (s)")
    ax.set_ylabel("Packets per second")
    ax.set_title("Packet Rate (1-second window)")
    ax.set_ylim(bottom=0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", help="Analyzer CSV output file")
    parser.add_argument("--out", default=None, help="Save figure to file (optional)")
    args = parser.parse_args()

    df = load_csv(args.csv)
    iat_us = df["iat_us"].values
    ts_s   = df["timestamp_s"].values

    print(f"Packets:    {len(df)}")
    print(f"Mean IAT:   {np.mean(iat_us):.2f} µs")
    print(f"p50 IAT:    {np.percentile(iat_us,  50):.2f} µs")
    print(f"p99 IAT:    {np.percentile(iat_us,  99):.2f} µs")
    print(f"p99.9 IAT:  {np.percentile(iat_us, 99.9):.2f} µs")
    print(f"Max IAT:    {np.max(iat_us):.2f} µs")

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Market Data Multicast — Latency Analysis", fontsize=13)

    plot_iat_histogram(axes[0], iat_us)
    plot_iat_cdf(axes[1], iat_us)
    plot_packet_rate(axes[2], ts_s)

    plt.tight_layout()

    if args.out:
        plt.savefig(args.out, dpi=150)
        print(f"Saved to {args.out}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
