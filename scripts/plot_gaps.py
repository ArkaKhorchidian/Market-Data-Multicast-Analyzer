#!/usr/bin/env python3
"""Plot gap timeline and distribution from analyzer CSV output.

Usage:
    analyzer --pcap session.pcap --gap-report --csv output.csv
    python3 scripts/plot_gaps.py output.csv
"""

import sys
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def load_csv(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    for col in ("timestamp_ns", "gap_detected", "gap_size"):
        if col not in df.columns:
            sys.exit(f"CSV missing column: {col}")
    df["timestamp_s"] = df["timestamp_ns"] / 1e9
    return df


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", help="Analyzer CSV output file")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    df   = load_csv(args.csv)
    gaps = df[df["gap_detected"] == 1].copy()

    if gaps.empty:
        print("No gaps detected in this session.")
        sys.exit(0)

    t0     = df["timestamp_s"].iloc[0]
    gaps["rel_s"]        = gaps["timestamp_s"] - t0
    df["cum_missing"]    = df["gap_size"].cumsum()

    print(f"Total packets:   {len(df)}")
    print(f"Gaps detected:   {len(gaps)}")
    print(f"Missing msgs:    {int(gaps['gap_size'].sum())}")
    print(f"Gap rate:        {len(gaps)/len(df)*100:.4f}%")
    print(f"Largest gap:     {int(gaps['gap_size'].max())} messages")

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Market Data Multicast — Gap Analysis", fontsize=13)

    # Gap timeline
    ax = axes[0]
    ax.scatter(gaps["rel_s"], gaps["gap_size"],
               color="crimson", s=30, alpha=0.7, zorder=3)
    ax.set_xlabel("Elapsed Time (s)")
    ax.set_ylabel("Gap Size (missing messages)")
    ax.set_title("Gap Timeline")
    ax.set_ylim(bottom=0)

    # Gap size histogram
    ax = axes[1]
    ax.hist(gaps["gap_size"], bins=max(1, int(gaps["gap_size"].max())),
            color="crimson", alpha=0.8, edgecolor="white")
    ax.set_xlabel("Gap Size (messages)")
    ax.set_ylabel("Count")
    ax.set_title("Gap Size Distribution")

    # Cumulative missing messages
    ax = axes[2]
    df_sorted = df.sort_values("timestamp_s")
    ax.plot(df_sorted["timestamp_s"] - t0, df_sorted["cum_missing"],
            color="darkorange", linewidth=1)
    ax.set_xlabel("Elapsed Time (s)")
    ax.set_ylabel("Cumulative Missing Messages")
    ax.set_title("Missing Messages Over Session")
    ax.set_ylim(bottom=0)

    plt.tight_layout()
    if args.out:
        plt.savefig(args.out, dpi=150)
        print(f"Saved to {args.out}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
