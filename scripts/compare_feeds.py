#!/usr/bin/env python3
"""Compare primary and secondary multicast feed CSVs.

Overlays IAT CDFs, highlights packets missing on primary but present
on secondary, and reports gap coverage.

Usage:
    analyzer --pcap primary.pcap  --csv primary.csv
    analyzer --pcap secondary.pcap --csv secondary.csv
    python3 scripts/compare_feeds.py primary.csv secondary.csv
"""

import sys
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


def load(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df["iat_us"] = df["iat_ns"] / 1e3
    return df


def iat_cdf(iat_us: np.ndarray):
    s   = np.sort(iat_us[iat_us > 0])
    cdf = np.arange(1, len(s) + 1) / len(s)
    return s, cdf


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("primary",   help="Primary feed CSV")
    parser.add_argument("secondary", help="Secondary feed CSV")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    pri = load(args.primary)
    sec = load(args.secondary)

    # Gap coverage: which sequences missing on primary appear on secondary?
    pri_gaps = pri[pri["gap_detected"] == 1]
    if not pri_gaps.empty:
        missing_ranges = set()
        for _, row in pri_gaps.iterrows():
            for seq in range(int(row["seq_num"]) - int(row["gap_size"]),
                             int(row["seq_num"])):
                missing_ranges.add(seq)

        sec_seqs       = set(sec["seq_num"].dropna().astype(int))
        covered        = missing_ranges & sec_seqs
        coverage_pct   = 100.0 * len(covered) / len(missing_ranges) if missing_ranges else 100.0

        print(f"Primary gaps:          {len(pri_gaps)}")
        print(f"Missing on primary:    {len(missing_ranges)} messages")
        print(f"Covered by secondary:  {len(covered)} ({coverage_pct:.1f}%)")
    else:
        print("No gaps on primary feed.")
        coverage_pct = 100.0

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.suptitle("Primary vs Secondary Feed Comparison", fontsize=13)

    # IAT CDF overlay
    ax = axes[0]
    for df, label, color in [(pri, "Primary", "steelblue"),
                              (sec, "Secondary", "darkorange")]:
        s, cdf = iat_cdf(df["iat_us"].values)
        ax.plot(s, cdf, color=color, linewidth=1, label=label)
    ax.set_xscale("log")
    ax.set_xlabel("IAT (µs)")
    ax.set_ylabel("CDF")
    ax.set_title("IAT CDF — Primary vs Secondary")
    ax.legend()
    ax.xaxis.set_major_formatter(ticker.ScalarFormatter())

    # Gap coverage pie
    ax = axes[1]
    if not pri_gaps.empty and missing_ranges:
        uncovered = 100.0 - coverage_pct
        ax.pie([coverage_pct, uncovered],
               labels=[f"Covered by secondary\n({coverage_pct:.1f}%)",
                       f"Lost on both\n({uncovered:.1f}%)"],
               colors=["steelblue", "crimson"],
               startangle=90)
    else:
        ax.text(0.5, 0.5, "No gaps on primary", ha="center", va="center",
                transform=ax.transAxes, fontsize=12)
    ax.set_title("Gap Coverage by Secondary Line")

    plt.tight_layout()
    if args.out:
        plt.savefig(args.out, dpi=150)
        print(f"Saved to {args.out}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
