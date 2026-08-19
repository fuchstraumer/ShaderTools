"""Measures what the cluster grid costs, against the near plane and the slice count.

Run it with:  py tests/scripts/vtf_grid_tuning.py

This is a measurement and not a test. It prints numbers and it asserts nothing.

**The question.** The depth slices of the grid grow by a constant ratio between the near plane and the
far plane. So the near plane decides where the resolution goes. A near plane of 0.1 with 24 slices
gives a ratio of 1.43, and the first slices then divide the space between 0.1 and 1.0, where a level
holds nothing. Those slices are spent, and the region that holds the lights takes what is left.

**What each column means.**

* `used` is the count of depth slices that hold at least one light. A slice that holds nothing still
  takes its share of the resolution, so a low count is waste.
* `irreducible` is the count of (cluster, light) pairs that really touch. It is the work that any
  method must perform, and it is the number to minimize.
* `tests` is the count of sphere tests the traversal performs, and `ratio` is `tests / irreducible`.
* `depth 1` is the depth of the first slice. A grid whose first slice ends before the near geometry
  is a grid that wastes its front.

**What this cannot say.** A grid that holds fewer clusters in front of the eye makes the assignment
cheaper and the shading coarser. This script measures the assignment only. A cluster that grows too
large makes every pixel inside it read lights it does not need, and no number here sees that.
"""

import numpy as np

import vtf_support as vtf

LIGHT_COUNT = 4096
BRANCHING = 32


def measure_grid(ctx: vtf.VtfDevice, grid: vtf.ClusterGrid,
                 positions: np.ndarray, ranges: np.ndarray) -> dict:
    cluster_min, cluster_max = grid.cluster_aabbs(ctx)

    root_min = (positions - ranges[:, None]).min(axis=0)
    root_max = (positions + ranges[:, None]).max(axis=0)
    cells = vtf.reference_morton_cells(positions, root_min, root_max, 10)
    order = np.argsort(vtf.morton_from_cells(cells, 10), kind="stable")

    leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, BRANCHING)
    pairs = int(vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max).sum())
    irreducible = int(vtf.count_clusters_per_light(positions, ranges,
                                                   cluster_min, cluster_max).sum())

    # Which depth slices hold a light. The grid runs x fastest, then y, then z.
    slice_of_cluster = np.arange(grid.cluster_count) // (grid.grid[0] * grid.grid[1])
    touched = np.zeros(grid.grid[2], dtype=bool)
    depth = -positions[:, 2]
    for index in range(grid.grid[2]):
        in_slice = slice_of_cluster == index
        lower = -cluster_max[in_slice, 2].max()
        upper = -cluster_min[in_slice, 2].min()
        touched[index] = bool(np.any((depth + ranges >= lower) & (depth - ranges <= upper)))

    first_slice_end = float(-cluster_min[slice_of_cluster == 0, 2].min())

    return {
        "near": grid.near,
        "slices": grid.grid[2],
        "clusters": grid.cluster_count,
        "ratio_per_slice": grid.near_k,
        "used": int(touched.sum()),
        "first": first_slice_end,
        "pairs": pairs,
        "tests": pairs * BRANCHING,
        "irreducible": irreducible,
    }


def print_table(title: str, rows) -> None:
    print(f"\n{title}")
    print(f"{'near':>7} {'slices':>7} {'k':>6} {'used':>6} {'depth 1':>9} "
          f"{'irreducible':>12} {'tests':>12} {'ratio':>7}")
    print("-" * 78)
    for row in rows:
        print(f"{row['near']:>7.2f} {row['slices']:>7} {row['ratio_per_slice']:>6.3f} "
              f"{row['used']:>6} {row['first']:>9.3f} {row['irreducible']:>12} "
              f"{row['tests']:>12} {row['tests'] / row['irreducible']:>7.2f}")


def main() -> None:
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    # The scene stays the same across every grid, so a change in the numbers belongs to the grid.
    reference_grid = vtf.ClusterGrid()
    lights, positions, ranges = vtf.make_lights_realistic(reference_grid, LIGHT_COUNT)
    depth = -positions[:, 2]
    inside = int((ranges >= depth).sum())
    print(f"\nscene: {LIGHT_COUNT} lights, depth {depth.min():.1f} to {depth.max():.1f}, "
          f"median {np.median(depth):.1f}, mean radius {ranges.mean():.2f}")
    print(f"       {inside} lights ({100.0 * inside / LIGHT_COUNT:.1f}%) hold the camera")

    near_rows = [measure_grid(ctx, vtf.ClusterGrid(near=near), positions, ranges)
                 for near in (0.1, 0.5, 1.0, 2.0, 5.0)]
    print_table("The near plane, at 24 slices and 9216 clusters", near_rows)

    baseline = near_rows[0]
    print(f"\nagainst near {baseline['near']}:")
    for row in near_rows:
        change = 100.0 * (row["irreducible"] - baseline["irreducible"]) / baseline["irreducible"]
        print(f"  near {row['near']:>5.2f}  {change:+7.1f}% of the irreducible cost, "
              f"{row['used']} of {row['slices']} slices hold a light")

    slice_rows = [measure_grid(ctx, vtf.ClusterGrid(grid=(24, 16, depth_slices), near=1.0),
                               positions, ranges)
                  for depth_slices in (16, 24, 32, 48)]
    print_table("The slice count, at near 1.0. Note the cluster count changes with it", slice_rows)
    for row in slice_rows:
        print(f"  {row['slices']:>3} slices, {row['clusters']:>6} clusters, "
              f"{row['irreducible'] / row['clusters']:>6.1f} lights for the average cluster")


if __name__ == "__main__":
    main()
