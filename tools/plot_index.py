#!/usr/bin/env python3
"""
Visualize the FELINE index as a 2D scatter plot (X, Y coordinates).
Replicates the style of Figures 3 and 12 from the FELINE paper.

Usage:
    python3 plot_index.py <index.csv> [-o output.png] [--edges graph.txt] [--no-labels]

Arguments:
    index.csv       CSV exported by: feline --export-index <graph> <index.csv>
    -o, --output    Output file (PNG/SVG/PDF). If omitted, shows interactive plot.
    --edges         Graph edge-list file to draw edges as arrows.
    --no-labels     Do not draw vertex labels (useful for large graphs).
"""

import argparse
import csv
import sys

import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np


def load_index(csv_path):
    """Load index CSV: vertex,x,y,level"""
    vertices, xs, ys, levels = [], [], [], []
    with open(csv_path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            vertices.append(int(row["vertex"]))
            xs.append(int(row["x"]))
            ys.append(int(row["y"]))
            levels.append(int(row["level"]))
    return vertices, np.array(xs), np.array(ys), np.array(levels)


def load_edges(graph_path):
    """Load edges from graph file (first line: n m, then u v pairs)."""
    edges = []
    with open(graph_path) as f:
        header = f.readline().split()
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                edges.append((int(parts[0]), int(parts[1])))
    return edges


def main():
    parser = argparse.ArgumentParser(description="Plot FELINE index as 2D scatter")
    parser.add_argument("index_csv", help="Index CSV file (vertex,x,y,level)")
    parser.add_argument("-o", "--output", help="Output image file (PNG/SVG/PDF)")
    parser.add_argument("--edges", help="Graph edge-list file to draw edges")
    parser.add_argument("--no-labels", action="store_true", help="Hide vertex labels")
    args = parser.parse_args()

    vertices, xs, ys, levels = load_index(args.index_csv)
    n = len(vertices)

    fig, ax = plt.subplots(figsize=(10, 8))

    # Color by level
    if levels.max() > 0:
        norm = plt.Normalize(vmin=levels.min(), vmax=levels.max())
        colors = cm.viridis(norm(levels))
    else:
        colors = "steelblue"

    # Draw edges as arrows if provided
    if args.edges:
        edges = load_edges(args.edges)
        # Build vertex -> (x, y) mapping
        coord = {v: (xs[i], ys[i]) for i, v in enumerate(vertices)}
        for u, v in edges:
            if u in coord and v in coord:
                x0, y0 = coord[u]
                x1, y1 = coord[v]
                ax.annotate(
                    "",
                    xy=(x1, y1), xytext=(x0, y0),
                    arrowprops=dict(arrowstyle="->", color="lightgray", lw=0.8),
                )

    # Scatter plot
    scatter = ax.scatter(xs, ys, c=levels, cmap="viridis", s=80, zorder=5, edgecolors="black", linewidths=0.5)

    # Labels
    if not args.no_labels and n <= 500:
        for i, v in enumerate(vertices):
            ax.annotate(str(v), (xs[i], ys[i]), textcoords="offset points",
                        xytext=(5, 5), fontsize=8, fontweight="bold")

    # Colorbar
    if isinstance(colors, np.ndarray):
        cbar = plt.colorbar(scatter, ax=ax, label="Level")

    ax.set_xlabel("X (Topological Order)", fontsize=12)
    ax.set_ylabel("Y (Kornaropoulos Order)", fontsize=12)
    ax.set_title("FELINE Index - Dominance Drawing", fontsize=14)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    if args.output:
        plt.savefig(args.output, dpi=150)
        print(f"Plot saved to: {args.output}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
