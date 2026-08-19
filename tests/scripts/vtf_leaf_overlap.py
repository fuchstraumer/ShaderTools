"""Measures the true cost of a sort key: how many clusters overlap each BVH leaf.

Run it with:  py tests/scripts/vtf_leaf_overlap.py

This is a measurement and not a test. It prints numbers and it asserts nothing.

**Why this number and not the others.** Two measurements came before this one, and they disagree. The
leaf diagonal over the scene diagonal says one thing, and the count of depth slices for each leaf says
another. Neither one is what the shader pays. VtfAssignLightsToClustersBVH walks the tree once for
each cluster, and a cluster enters a leaf when its box touches the leaf box. So the count of
(cluster, leaf) pairs is the work, and no proxy stands between it and the traversal.

The cluster boxes come from VtfComputeClusterAABBs on the device, and not from a python copy of it.
The measurement therefore reads the same boxes the shader reads.

**What the numbers mean.**

* `pairs` is the total count of (cluster, leaf) touches. It is the whole traversal cost, and it is the
  headline. A lower number is better.
* `max` is the largest count for one leaf. It sets the longest traversal, and a wave waits for its
  longest lane.
* `median` describes the ordinary leaf.

**Read the control first.** The unsorted arm holds the lights in the order they arrive. Any curve that
does not beat it by a large factor is doing nothing, and a defect in this script would show up there.
"""

import sys

import numpy as np

import vtf_support as vtf

LIGHT_COUNT = 4096
BRANCHING = 32


def measure(name: str, order: np.ndarray, positions: np.ndarray, ranges: np.ndarray,
            cluster_min: np.ndarray, cluster_max: np.ndarray) -> dict:
    leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, BRANCHING)
    counts = vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max)

    diagonal = np.linalg.norm(leaf_max - leaf_min, axis=1)
    return {
        "name": name,
        "pairs": int(counts.sum()),
        "median": float(np.median(counts)),
        "mean": float(counts.mean()),
        "max": int(counts.max()),
        "empty": int((counts == 0).sum()),
        "diagonal": float(np.median(diagonal)),
    }


def main() -> None:
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    grid = vtf.ClusterGrid()
    cluster_min, cluster_max = grid.cluster_aabbs(ctx)
    print(f"\ncluster grid {grid.grid[0]} by {grid.grid[1]} by {grid.grid[2]}, "
          f"{grid.cluster_count} clusters, near {grid.near}, far {grid.far}")

    # The realistic scene is the default. Pass "random" on the command line for the older one, which
    # draws depth and radius without letting them know about each other. The two give very different
    # numbers, and the difference is the point of the comparison in step 2.
    if len(sys.argv) > 1 and sys.argv[1] == "random":
        print("scene: make_lights_in_frustum, depth on a constant ratio, flat radius")
        lights, positions, ranges = vtf.make_lights_in_frustum(grid, LIGHT_COUNT)
    else:
        print("scene: make_lights_realistic, uniform world density with a local room")
        lights, positions, ranges = vtf.make_lights_realistic(grid, LIGHT_COUNT)

    leaf_count = (LIGHT_COUNT + BRANCHING - 1) // BRANCHING
    print(f"{LIGHT_COUNT} lights in the frustum, {leaf_count} leaves of {BRANCHING} lights, "
          f"mean radius {ranges.mean():.2f}")

    # Read this before any number below. A light that holds the camera inside it covers a large part
    # of the grid whatever the tree does, so it sets the irreducible cost and not the tree quality. A
    # level holds a few of them and not many, so a large share here says the scene is wrong before it
    # says anything about the tree.
    depth = -positions[:, 2]
    inside = int((ranges >= depth).sum())
    print(f"scene: median depth {np.median(depth):.1f}, "
          f"{inside} lights of {LIGHT_COUNT} ({100.0 * inside / LIGHT_COUNT:.1f}%) hold the camera")

    # Every arm quantizes the same way. Only the curve that reads the cells differs, so a difference
    # in the result belongs to the curve.
    root_min = (positions - ranges[:, None]).min(axis=0)
    root_max = (positions + ranges[:, None]).max(axis=0)
    cells = vtf.reference_morton_cells(positions, root_min, root_max, 10)

    arms = [
        ("unsorted, the control", np.arange(LIGHT_COUNT)),
        ("Morton 10/10/10", np.argsort(vtf.morton_from_cells(cells, 10), kind="stable")),
        ("Morton 10/10/8", np.argsort(vtf.morton_from_cells_anisotropic(cells, (10, 10, 8)),
                                      kind="stable")),
        ("Morton 10/10/12", np.argsort(vtf.morton_from_cells_anisotropic(cells, (10, 10, 12)),
                                       kind="stable")),
        ("Hilbert 10", np.argsort(vtf.hilbert_from_cells(cells, 10), kind="stable")),
    ]

    rows = [measure(name, order, positions, ranges, cluster_min, cluster_max)
            for name, order in arms]

    print(f"\n{'arm':<24} {'pairs':>10} {'median':>8} {'mean':>8} {'max':>6} "
          f"{'empty':>6} {'diagonal':>9}")
    print("-" * 76)
    baseline = rows[1]["pairs"]
    for row in rows:
        print(f"{row['name']:<24} {row['pairs']:>10} {row['median']:>8.1f} {row['mean']:>8.1f} "
              f"{row['max']:>6} {row['empty']:>6} {row['diagonal']:>9.3f}")

    print(f"\nagainst Morton 10/10/10:")
    for row in rows:
        change = 100.0 * (row["pairs"] - baseline) / baseline
        print(f"  {row['name']:<24} {change:+7.1f}% of the traversal work")

    # Where the cost sits. The mean is far above the median, so a few leaves hold most of the work.
    # A curve can only move a light from one leaf to another. It cannot make a box smaller. So this
    # number sets a limit on what any curve can win.
    print("\nHow much of the work the worst leaves hold:")
    morton_order = arms[1][1]
    leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, morton_order, BRANCHING)
    counts = np.sort(vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max))[::-1]
    total = counts.sum()
    for worst in (1, 4, 8, 16, 32):
        print(f"  the worst {worst:>2} leaves of {len(counts)} hold "
              f"{100.0 * counts[:worst].sum() / total:>5.1f}% of the work")

    # The floor that light radius sets. A light with no radius is a point, so this arm holds the
    # partition and removes the radius. No curve can reach below it.
    zero_radius = np.zeros_like(ranges)
    floor = measure("Morton, radius at zero", morton_order, positions, zero_radius,
                    cluster_min, cluster_max)
    print(f"\nThe floor that light radius sets:")
    print(f"  Morton 10/10/10, radius as authored  {baseline:>10} pairs")
    print(f"  Morton 10/10/10, radius at zero      {floor['pairs']:>10} pairs, "
          f"{100.0 * floor['pairs'] / baseline:.1f}% of the work")

    report_efficiency(rows, positions, ranges, cluster_min, cluster_max)


def report_efficiency(rows, positions: np.ndarray, ranges: np.ndarray,
                      cluster_min: np.ndarray, cluster_max: np.ndarray) -> None:
    """Compares the tests the traversal performs against the tests that must succeed.

    A leaf node of this tree holds one light, and the level above it holds VTF_BVH_BRANCHING lights.
    So a cluster that enters one of those nodes performs VTF_BVH_BRANCHING sphere tests. The count of
    (cluster, node) pairs multiplied by the branching factor is therefore the count of sphere tests,
    and the irreducible count is how many of them can succeed.
    """
    print("\n" + "=" * 76)
    print("The efficiency of the tree")
    print("=" * 76)

    touches = vtf.count_clusters_per_light(positions, ranges, cluster_min, cluster_max)
    irreducible = int(touches.sum())

    print(f"\nirreducible cost, the count of (cluster, light) pairs that really touch:")
    print(f"  {irreducible:>12}  appends any method must perform")
    print(f"  {float(touches.mean()):>12.1f}  clusters for the average light")
    print(f"  {int(np.median(touches)):>12}  clusters for the median light")
    print(f"  {int(touches.max()):>12}  clusters for the largest light, of "
          f"{len(cluster_min)} clusters")

    # The size disparity, which is what a size class would separate.
    ordered = np.sort(touches)[::-1]
    for worst in (1, 16, 64, 256):
        share = 100.0 * ordered[:worst].sum() / irreducible
        print(f"  the largest {worst:>3} lights of {len(touches)} hold {share:>5.1f}% "
              f"of the irreducible cost")

    print(f"\n{'arm':<24} {'sphere tests':>14} {'ratio':>8} {'wasted':>14}")
    print("-" * 64)
    for row in rows:
        tests = row["pairs"] * BRANCHING
        print(f"{row['name']:<24} {tests:>14} {tests / irreducible:>8.2f} "
              f"{tests - irreducible:>14}")

    print(f"\nA ratio of 1.00 would mean every sphere test succeeds, which no tree reaches.")
    print(f"The ratio is what a better partition can attack. The irreducible count is not.")


if __name__ == "__main__":
    main()
