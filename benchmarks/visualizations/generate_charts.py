#!/usr/bin/env python3
"""Generate clean, publication-ready charts for KITES RVV benchmark results.

Input:
  benchmarks/<benchmark>/results/execution_summary.json

Outputs:
  benchmarks/visualizations/*.png
  benchmarks/visualizations/*.svg

Requires:
  python -m pip install matplotlib
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, MaxNLocator


ROOT = Path(__file__).resolve().parents[2]  # project root if script is in benchmarks/
BENCHMARKS = [
    ("dot_product", "Dot product", "N = 256"),
    ("matrix_multiplication", "Matrix multiplication", "16 × 16"),
    ("fir_filter", "FIR filter", "256 samples, 16 taps"),
    ("sobel", "Sobel filter", "32 × 32"),
]

OUT_DIR = ROOT / "benchmarks" / "visualizations"

# Restrained, color-blind-friendlier palette.
SCALAR_COLOR = "#526581"       # slate blue
VECTOR_COLOR = "#168C78"       # teal
SCALAR_LIGHT = "#B9C5D3"
VECTOR_LIGHT = "#9BCFC4"
TEXT_COLOR = "#202B38"
MUTED_COLOR = "#687586"
GRID_COLOR = "#E7ECF1"
ACCENT_COLOR = "#C07832"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "axes.titlesize": 15,
    "axes.titleweight": "semibold",
    "axes.labelsize": 10,
    "axes.labelcolor": MUTED_COLOR,
    "xtick.labelsize": 9,
    "ytick.labelsize": 9,
    "text.color": TEXT_COLOR,
    "axes.edgecolor": "#D7DEE6",
    "axes.linewidth": 0.8,
    "figure.facecolor": "white",
    "axes.facecolor": "white",
    "savefig.facecolor": "white",
})


def load_data() -> list[dict[str, Any]]:
    """Load benchmark JSON and attach stable display labels."""
    rows = []
    for slug, title, subtitle in BENCHMARKS:
        path = ROOT / "benchmarks" / slug / "results" / "execution_summary.json"
        if not path.is_file():
            raise FileNotFoundError(f"Missing benchmark result: {path}")

        with path.open(encoding="utf-8") as handle:
            item = json.load(handle)

        for mode in ("scalar", "vector"):
            if mode not in item:
                raise KeyError(f"{path}: missing '{mode}' object")
            for key in ("kernel_instructions", "harness_instructions", "total_instructions"):
                if key not in item[mode]:
                    raise KeyError(f"{path}: missing '{mode}.{key}'")

        item["slug"] = slug
        item["title"] = title
        item["subtitle"] = subtitle

        # Derive metrics from the raw counts instead of trusting potentially
        # stale/precomputed percentages or ratios in the JSON.
        scalar_kernel = item["scalar"]["kernel_instructions"]
        vector_kernel = item["vector"]["kernel_instructions"]
        scalar_total = item["scalar"]["total_instructions"]
        vector_total = item["vector"]["total_instructions"]

        if min(scalar_kernel, vector_kernel, scalar_total, vector_total) < 0:
            raise ValueError(f"{path}: instruction counts must be non-negative")
        if scalar_kernel == 0 or vector_kernel == 0 or scalar_total == 0 or vector_total == 0:
            raise ValueError(f"{path}: cannot calculate ratios from zero instruction counts")

        item["kernel_reduction_percent"] = (
            (scalar_kernel - vector_kernel) / scalar_kernel * 100
        )
        item["total_reduction_percent"] = (
            (scalar_total - vector_total) / scalar_total * 100
        )
        item["kernel_speedup_ratio"] = scalar_kernel / vector_kernel
        item["total_speedup_ratio"] = scalar_total / vector_total

        rows.append(item)

    return rows


def style_axis(ax: plt.Axes) -> None:
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color("#D7DEE6")
    ax.spines["bottom"].set_color("#D7DEE6")
    ax.grid(axis="y", color=GRID_COLOR, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.tick_params(axis="both", colors=MUTED_COLOR, length=0, pad=7)


def add_value_labels(ax: plt.Axes, bars, values, formatter=str) -> None:
    for bar, value in zip(bars, values):
        ax.annotate(
            formatter(value),
            (bar.get_x() + bar.get_width() / 2, bar.get_height()),
            xytext=(0, 5),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=8.5,
            color=TEXT_COLOR,
        )


def save_figure(fig: plt.Figure, stem: str) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for extension in ("png", "svg"):
        fig.savefig(
            OUT_DIR / f"{stem}.{extension}",
            dpi=300,
            bbox_inches="tight",
            pad_inches=0.15,
        )
    plt.close(fig)
    print(f"Generated {stem}.png and {stem}.svg")


def benchmark_axis_labels(data: list[dict[str, Any]]) -> list[str]:
    return [f'{d["title"]}\n{d["subtitle"]}' for d in data]


def plot_kernel_instruction_counts(data: list[dict[str, Any]]) -> None:
    """Compare scalar and vector kernel instruction counts."""
    x = np.arange(len(data))
    width = 0.32
    scalar = [d["scalar"]["kernel_instructions"] for d in data]
    vector = [d["vector"]["kernel_instructions"] for d in data]

    fig, ax = plt.subplots(figsize=(10.5, 5.8))
    fig.subplots_adjust(left=0.10, right=0.98, top=0.86, bottom=0.24)
    bars_s = ax.bar(
        x - width / 2, scalar, width, label="Scalar · RV32IM",
        color=SCALAR_COLOR, edgecolor="none", zorder=3,
    )
    bars_v = ax.bar(
        x + width / 2, vector, width, label="Vector · RV32IMV",
        color=VECTOR_COLOR, edgecolor="none", zorder=3,
    )

    add_value_labels(ax, bars_s, scalar, lambda v: f"{v:,.0f}")
    add_value_labels(ax, bars_v, vector, lambda v: f"{v:,.0f}")

    # Put the reduction in a separate line below each benchmark label.
    for i, d in enumerate(data):
        ax.annotate(
            f'{d["kernel_reduction_percent"]:.1f}% fewer',
            (x[i], 0),
            xytext=(0, -37),
            textcoords="offset points",
            ha="center",
            va="top",
            fontsize=8.5,
            fontweight="semibold",
            color=VECTOR_COLOR if d["kernel_reduction_percent"] >= 0 else ACCENT_COLOR,
            annotation_clip=False,
        )

    ax.set_title("Kernel instruction count", loc="left", pad=15)
    ax.set_ylabel("Dynamic instructions retired")
    ax.set_xticks(x, benchmark_axis_labels(data))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=6, integer=True))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:,.0f}"))
    ax.legend(frameon=False, ncol=2, loc="upper right")
    style_axis(ax)
    ax.margins(y=0.20)
    save_figure(fig, "instruction_counts_comparison")


def plot_kernel_speedup(data: list[dict[str, Any]]) -> None:
    """Show kernel instruction-count ratio, with a clearly marked 1× baseline."""
    x = np.arange(len(data))
    ratios = [d["kernel_speedup_ratio"] for d in data]
    reductions = [d["kernel_reduction_percent"] for d in data]

    fig, ax = plt.subplots(figsize=(10.5, 5.4))
    fig.subplots_adjust(left=0.10, right=0.98, top=0.86, bottom=0.22)
    bars = ax.bar(
        x, ratios, width=0.48, color=VECTOR_COLOR, edgecolor="none", zorder=3
    )
    baseline = ax.axhline(
        1.0, color=SCALAR_COLOR, linewidth=1.2, linestyle=(0, (4, 3)),
        label="1.0× scalar baseline", zorder=2,
    )

    for bar, ratio, reduction in zip(bars, ratios, reductions):
        ax.annotate(
            f"{ratio:.2f}×",
            (bar.get_x() + bar.get_width() / 2, bar.get_height()),
            xytext=(0, 6), textcoords="offset points",
            ha="center", va="bottom", fontsize=10, fontweight="semibold",
        )
        ax.annotate(
            f"{reduction:.1f}% fewer",
            (bar.get_x() + bar.get_width() / 2, 0),
            xytext=(0, -27), textcoords="offset points",
            ha="center", va="top", fontsize=8.5, color=VECTOR_COLOR,
            annotation_clip=False,
        )

    ax.set_title("Kernel instruction-count reduction", loc="left", pad=15)
    ax.set_ylabel("Scalar instructions ÷ vector instructions")
    ax.set_xticks(x, benchmark_axis_labels(data))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=6))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:.1f}×"))
    ax.legend(handles=[baseline], frameon=False, loc="upper left")
    style_axis(ax)
    ax.margins(y=0.22)
    save_figure(fig, "kernel_speedup")


def plot_instruction_breakdown(data: list[dict[str, Any]]) -> None:
    """Show kernel and harness contributions, separately for scalar and vector."""
    x = np.arange(len(data))
    width = 0.30

    scalar_kernel = np.array([d["scalar"]["kernel_instructions"] for d in data])
    scalar_harness = np.array([d["scalar"]["harness_instructions"] for d in data])
    vector_kernel = np.array([d["vector"]["kernel_instructions"] for d in data])
    vector_harness = np.array([d["vector"]["harness_instructions"] for d in data])

    fig, ax = plt.subplots(figsize=(11, 6))
    fig.subplots_adjust(left=0.10, right=0.98, top=0.86, bottom=0.18)

    scalar_x = x - width / 2
    vector_x = x + width / 2

    ax.bar(
        scalar_x, scalar_kernel, width, color=SCALAR_COLOR,
        label="Scalar kernel", edgecolor="none", zorder=3,
    )
    ax.bar(
        scalar_x, scalar_harness, width, bottom=scalar_kernel,
        color=SCALAR_LIGHT, label="Scalar harness", edgecolor="none", zorder=3,
    )
    ax.bar(
        vector_x, vector_kernel, width, color=VECTOR_COLOR,
        label="Vector kernel", edgecolor="none", zorder=3,
    )
    ax.bar(
        vector_x, vector_harness, width, bottom=vector_kernel,
        color=VECTOR_LIGHT, label="Vector harness", edgecolor="none", zorder=3,
    )

    for xpos, kernel, harness in (
        (scalar_x, scalar_kernel, scalar_harness),
        (vector_x, vector_kernel, vector_harness),
    ):
        totals = kernel + harness
        for x_pos, total in zip(xpos, totals):
            ax.annotate(
                f"{total:,.0f}",
                (x_pos, total),
                xytext=(0, 5), textcoords="offset points",
                ha="center", va="bottom", fontsize=8,
            )

    ax.set_title("Total instruction count: kernel and harness", loc="left", pad=15)
    ax.set_ylabel("Dynamic instructions retired")
    ax.set_xticks(x, benchmark_axis_labels(data))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=6, integer=True))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:,.0f}"))
    ax.legend(frameon=False, ncol=2, loc="upper right")
    style_axis(ax)
    ax.margins(y=0.18)
    save_figure(fig, "instruction_breakdown")


def main() -> None:
    data = load_data()
    plot_kernel_instruction_counts(data)
    plot_kernel_speedup(data)
    plot_instruction_breakdown(data)
    print(f"\nCharts saved in: {OUT_DIR}")


if __name__ == "__main__":
    main()
