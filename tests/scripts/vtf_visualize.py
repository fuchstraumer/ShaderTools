"""Draws the evidence that a number cannot give, for the VTF compute shaders.

The module never opens a window. It selects the Agg backend before it imports pyplot, and it writes one
PNG next to this file. A missing matplotlib is not an error, because the numeric checks in
test_vtf_compute.py stand on their own.

Each panel answers one question that an assertion answers poorly:

* The Morton curve shows whether the sort really keeps neighbours together. A correct Z-order walk
  stays inside a small region and steps to the next region only after it finishes the current one. A
  broken interleave draws a line across the whole scene on nearly every step.
* The BVH panel shows whether each leaf box really holds its own run of lights. A correct build gives
  small boxes that follow the Morton order. A build that reduces across the wrong lanes gives boxes
  that each span the whole scene.
"""

import pathlib

import matplotlib
matplotlib.use("Agg")  # select the headless backend before pyplot loads, so no window can open

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Rectangle

OUTPUT = pathlib.Path(__file__).resolve().parent / "vtf_verification.png"


def render(positions, ranges, codes, order, leaf_min, leaf_max, root_min, root_max,
           neighbour_distance, random_distance,
           sort_input=None, sort_chunked=None, sort_merged=None):
    figure, axes = plt.subplots(2, 2, figsize=(15, 12))
    figure.suptitle("VolumeTiledForwardShading compute verification", fontsize=15)

    ordered = positions[order]

    # ---- Morton order curve -------------------------------------------------------------------
    ax = axes[0][0]
    step_count = min(1024, len(ordered))
    walk = ordered[:step_count]
    ax.plot(walk[:, 0], walk[:, 1], linewidth=0.6, color="#3b6ea5", alpha=0.85, zorder=1)
    ax.scatter(walk[:, 0], walk[:, 1], s=4, c=np.arange(step_count), cmap="viridis", zorder=2)
    ax.set_title(f"Morton order walk, first {step_count} lights\n"
                 f"{neighbour_distance:.2f} vs {random_distance:.2f} for random pair")
    ax.set_xlabel("view x")
    ax.set_ylabel("view y")
    ax.set_aspect("equal", adjustable="box")

    # ---- BVH leaf boxes -----------------------------------------------------------------------
    ax = axes[0][1]
    ax.scatter(positions[:, 0], positions[:, 1], s=2, color="#9aa0a6", alpha=0.5, zorder=1)
    colors = plt.cm.plasma(np.linspace(0.0, 1.0, len(leaf_min)))
    for index, (lower, upper) in enumerate(zip(leaf_min, leaf_max)):
        ax.add_patch(Rectangle(
            (lower[0], lower[1]), upper[0] - lower[0], upper[1] - lower[1],
            fill=False, edgecolor=colors[index], linewidth=0.7, alpha=0.8, zorder=2))
    ax.set_title(f"{len(leaf_min)} BVH leaf boxes over the lights\n"
                 "each box holds one run of 32 lights in Morton order")
    ax.set_xlabel("view x")
    ax.set_ylabel("view y")
    ax.set_aspect("equal", adjustable="box")

    # ---- The sort, as the GPU left it ----------------------------------------------------------
    #
    # This panel held np.sort(codes) before. That plot sorted the keys itself, so it drew a monotone
    # ramp for a broken sort and for a correct sort alike. It could not fail. The three lines below
    # are buffers that the GPU wrote, and each one fails in its own way:
    #
    # * the input is noise, and it is the control.
    # * the radix output is one ramp for each thread group. A wrong chunk base offset gives one ramp
    #   and then a flat tail of zeros, because every group writes the first chunk.
    # * the merged output is one ramp across the whole range.
    ax = axes[1][0]
    if sort_input is not None:
        chunk_count = 16
        ax.plot(sort_input, linewidth=0.3, color="#c62828", alpha=0.55,
                label="input, unsorted")
        ax.plot(sort_chunked, linewidth=0.8, color="#00ef14",
                label=f"after VtfRadixSort, {chunk_count} chunks")
        ax.plot(sort_merged, linewidth=1.4, color="#1565c0",
                label="after VtfMergeSort, 4 passes")
        for boundary in range(0, len(sort_chunked) + 1, len(sort_chunked) // chunk_count):
            ax.axvline(boundary, color="#9aa0a6", linewidth=0.4, alpha=0.5, zorder=0)
        ax.set_title("GPU sort verification (input noise -> chunked -> merged)")
        ax.set_xlabel("slot")
        ax.set_ylabel("key")
        ax.legend(loc="upper left", fontsize=8)
    else:
        ax.scatter(np.arange(len(codes)), np.sort(codes), s=1, color="#2e7d32")
        ax.set_title("Morton codes, sorted by numpy\n"
                     "no GPU buffer reaches this panel")
        ax.set_xlabel("light, in sorted order")
        ax.set_ylabel("Morton code")

    # ---- The leaf box extent -------------------------------------------------------------------
    ax = axes[1][1]
    extent = np.linalg.norm(leaf_max - leaf_min, axis=1)
    scene = float(np.linalg.norm(np.asarray(root_max) - np.asarray(root_min)))
    ax.hist(extent / scene, bins=40, color="#6a1b9a", alpha=0.85)
    ax.set_title("Leaf Box Diagonal / Scene Diagonal\n"
                 "small values = more tightly packed leaves")
    ax.set_xlabel("leaf diagonal / scene diagonal")
    ax.set_ylabel("leaf count")

    figure.tight_layout()
    figure.savefig(OUTPUT, dpi=110)
    plt.close(figure)
    return OUTPUT
