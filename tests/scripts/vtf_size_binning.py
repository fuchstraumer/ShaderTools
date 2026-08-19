"""Measures what a size class does to the cost of the light assignment.

Run it with:  py tests/scripts/vtf_size_binning.py

This is a measurement and not a test. It prints numbers and it asserts nothing.

**The problem it attacks.** A leaf of the BVH holds VTF_BVH_BRANCHING lights, whichever lights the sort
put next to each other. The sort orders lights by position only. So a light that covers 6000 clusters
can share a leaf with 31 lights that cover 20 clusters each. The leaf box then holds the large light,
and every cluster that touches it performs 32 sphere tests to find the one light that succeeds.

vtf_leaf_overlap.py measures this. On a realistic scene the traversal performs 7.14 sphere tests for
each test that can succeed, and the largest 64 lights of 4096 hold half of the real work.

**The change.** Give each light a size class, and put that class in the high bits of the sort key. The
sort then groups lights by size first and by position second. A large light shares its leaf with other
large lights, and a leaf of small lights stays small.

**The size measure.** The class comes from the angle the light covers, which is its radius divided by
its depth. That one number describes all three axes of the cluster grid, because the grid divides x
and y by angle and divides z by a constant ratio. A light at twice the depth with twice the radius
covers the same count of clusters, and it takes the same class.

The measure costs one divide and one exponent read for each light. It needs no cluster and no grid.

**Two ways to group.** `mixed` puts the class in the key and cuts the result into runs of 32. A run
that crosses a class boundary then holds two classes. `aligned` starts a new run at each class, so no
leaf holds two classes, and the last leaf of a class holds fewer than 32 lights. A real build needs
the second one, and it is the one an indirect dispatch already supports.
"""

import numpy as np

import vtf_support as vtf

LIGHT_COUNT = 4096
BRANCHING = 32


def size_class(positions: np.ndarray, ranges: np.ndarray, bins: int) -> np.ndarray:
    """Gives each light a class from the angle it covers, largest angle in the highest class.

    The angle is the radius over the depth. Each class covers one doubling of it, and the classes at
    the two ends absorb everything past them, so no light falls outside.
    """
    depth = np.maximum(-positions[:, 2], 1.0e-4)
    angle = ranges / depth

    # log2 of the angle, shifted so that the ordinary light lands in the middle of the range.
    steps = np.floor(np.log2(np.maximum(angle, 1.0e-6)))
    middle = np.median(steps)
    return np.clip(steps - middle + bins // 2, 0, bins - 1).astype(np.int64)


def mixed_order(classes: np.ndarray, spatial_key: np.ndarray) -> np.ndarray:
    """Sorts by class first and by position second, as one key would."""
    return np.lexsort((spatial_key, classes))


def aligned_leaves(positions: np.ndarray, ranges: np.ndarray,
                   classes: np.ndarray, spatial_key: np.ndarray):
    """Builds leaf boxes that never hold two classes.

    Each class is ordered by position and then cut into runs of BRANCHING. The last run of a class
    holds what is left, so it can hold fewer lights. The shader already reads a count for each node,
    so a short run costs nothing that a full run does not.
    """
    lower, upper = [], []
    for value in np.unique(classes):
        members = np.flatnonzero(classes == value)
        members = members[np.argsort(spatial_key[members], kind="stable")]

        for start in range(0, len(members), BRANCHING):
            run = members[start:start + BRANCHING]
            lower.append((positions[run] - ranges[run, None]).min(axis=0))
            upper.append((positions[run] + ranges[run, None]).max(axis=0))

    return np.array(lower, dtype=np.float64), np.array(upper, dtype=np.float64)


def report(name: str, leaf_min, leaf_max, cluster_min, cluster_max, irreducible: int) -> dict:
    counts = vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max)
    pairs = int(counts.sum())
    tests = pairs * BRANCHING
    return {
        "name": name,
        "leaves": len(leaf_min),
        "pairs": pairs,
        "tests": tests,
        "ratio": tests / irreducible,
        "max": int(counts.max()),
    }


def main() -> None:
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    # near at 1.0, which is the result of step 2. The grid wastes its front at 0.1.
    grid = vtf.ClusterGrid(near=1.0)
    cluster_min, cluster_max = grid.cluster_aabbs(ctx)

    lights, positions, ranges = vtf.make_lights_realistic(grid, LIGHT_COUNT)
    irreducible = int(vtf.count_clusters_per_light(positions, ranges,
                                                   cluster_min, cluster_max).sum())

    depth = -positions[:, 2]
    print(f"\nscene: {LIGHT_COUNT} lights, median depth {np.median(depth):.1f}, "
          f"{int((ranges >= depth).sum())} hold the camera")
    print(f"grid: near {grid.near}, {grid.cluster_count} clusters")
    print(f"irreducible cost: {irreducible} (cluster, light) pairs that really touch")

    root_min = (positions - ranges[:, None]).min(axis=0)
    root_max = (positions + ranges[:, None]).max(axis=0)
    cells = vtf.reference_morton_cells(positions, root_min, root_max, 10)
    morton = vtf.morton_from_cells(cells, 10)
    hilbert = vtf.hilbert_from_cells(cells, 10)

    rows = []

    def add_flat(name, key):
        order = np.argsort(key, kind="stable")
        leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, BRANCHING)
        rows.append(report(name, leaf_min, leaf_max, cluster_min, cluster_max, irreducible))

    add_flat("Morton, no class", morton)
    add_flat("Hilbert, no class", hilbert)

    for bins in (2, 4, 8, 16):
        classes = size_class(positions, ranges, bins)
        order = mixed_order(classes, morton)
        leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, BRANCHING)
        rows.append(report(f"Morton, {bins:>2} classes, mixed", leaf_min, leaf_max,
                           cluster_min, cluster_max, irreducible))

        leaf_min, leaf_max = aligned_leaves(positions, ranges, classes, morton)
        rows.append(report(f"Morton, {bins:>2} classes, aligned", leaf_min, leaf_max,
                           cluster_min, cluster_max, irreducible))

    classes = size_class(positions, ranges, 8)
    leaf_min, leaf_max = aligned_leaves(positions, ranges, classes, hilbert)
    rows.append(report("Hilbert,  8 classes, aligned", leaf_min, leaf_max,
                       cluster_min, cluster_max, irreducible))

    # The control. Order by size and ignore position, so leaves hold one size and no locality. It
    # separates what the class gives from what the curve gives.
    add_flat("size only, no curve", size_class(positions, ranges, 4096) * np.int64(1 << 20)
             + np.arange(LIGHT_COUNT))

    print(f"\n{'arm':<32} {'leaves':>7} {'tests':>12} {'ratio':>7} {'max':>7} {'against Morton':>15}")
    print("-" * 86)
    baseline = rows[0]["tests"]
    for row in rows:
        change = 100.0 * (row["tests"] - baseline) / baseline
        print(f"{row['name']:<32} {row['leaves']:>7} {row['tests']:>12} {row['ratio']:>7.2f} "
              f"{row['max']:>7} {change:>14.1f}%")

    best = min(rows, key=lambda row: row["tests"])
    print(f"\nbest arm: {best['name']}, ratio {best['ratio']:.2f}, "
          f"{100.0 * (baseline - best['tests']) / baseline:.1f}% fewer sphere tests than Morton")

    # How the classes divide the lights, and how much real work each one holds.
    print("\nWhere the lights and the work sit, at 8 classes:")
    classes = size_class(positions, ranges, 8)
    touches = vtf.count_clusters_per_light(positions, ranges, cluster_min, cluster_max)
    print(f"  {'class':>5} {'lights':>7} {'leaves':>7} {'median clusters':>16} {'share of work':>14}")
    for value in range(8):
        members = classes == value
        if not members.any():
            continue
        leaves = (int(members.sum()) + BRANCHING - 1) // BRANCHING
        print(f"  {value:>5} {int(members.sum()):>7} {leaves:>7} "
              f"{int(np.median(touches[members])):>16} "
              f"{100.0 * touches[members].sum() / touches.sum():>13.1f}%")


    diagnose_by_class(positions, ranges, cluster_min, cluster_max, morton, touches)


def diagnose_by_class(positions, ranges, cluster_min, cluster_max, spatial_key,
                      touches, bins: int = 8) -> None:
    """Says which classes spend the tests, and what a different method would cost for each one.

    The table above says the class does almost nothing. This says why. It counts the tests that each
    class spends once its leaves hold that class alone, and it compares them against the tests the
    same lights must pass.
    """
    print("\n" + "=" * 86)
    print("What each class spends, once its leaves hold that class alone")
    print("=" * 86)

    classes = size_class(positions, ranges, bins)
    print(f"\n  {'class':>5} {'lights':>7} {'leaves':>7} {'tests':>12} {'must pass':>11} "
          f"{'ratio':>8} {'share':>7}")

    spent, floor_of = {}, {}
    for value in np.unique(classes):
        members = np.flatnonzero(classes == value)
        ordered = members[np.argsort(spatial_key[members], kind="stable")]

        lower, upper = [], []
        for start in range(0, len(ordered), BRANCHING):
            run = ordered[start:start + BRANCHING]
            lower.append((positions[run] - ranges[run, None]).min(axis=0))
            upper.append((positions[run] + ranges[run, None]).max(axis=0))

        counts = vtf.count_cluster_overlaps(np.array(lower), np.array(upper),
                                            cluster_min, cluster_max)
        spent[value] = int(counts.sum()) * BRANCHING
        floor_of[value] = int(touches[members].sum())

    total_spent = sum(spent.values())
    for value in sorted(spent):
        members = int((classes == value).sum())
        leaves = (members + BRANCHING - 1) // BRANCHING
        print(f"  {value:>5} {members:>7} {leaves:>7} {spent[value]:>12} {floor_of[value]:>11} "
              f"{spent[value] / max(floor_of[value], 1):>8.1f} "
              f"{100.0 * spent[value] / total_spent:>6.1f}%")

    print(f"\nA class whose ratio is very large gains nothing from a tree. Its lights each cover a "
          f"large\npart of the grid, so a leaf of 32 of them covers nearly all of it.")

    # The hybrid. Keep the tree for the classes below the cut, and write the classes at or above it
    # straight into their clusters, which costs one append for each pair that really touches.
    print("\n" + "=" * 86)
    print("A hybrid: tree for the small classes, direct write for the large ones")
    print("=" * 86)
    print(f"\n  {'cut':>5} {'tree tests':>12} {'appends':>10} {'total':>12} {'against tree':>14}")

    everything = total_spent
    for cut in range(bins + 1):
        tree = sum(spent[value] for value in spent if value < cut)
        appends = sum(floor_of[value] for value in floor_of if value >= cut)
        total = tree + appends
        label = "all tree" if cut == bins else ("all direct" if cut == 0 else str(cut))
        print(f"  {label:>5} {tree:>12} {appends:>10} {total:>12} "
              f"{100.0 * (total - everything) / everything:>13.1f}%")

    print("\nAn append is not a sphere test. An append writes memory under contention and a sphere")
    print("test is branchless arithmetic. Read the counts as counts, and not as time.")

    sweep_leaf_size(positions, ranges, cluster_min, cluster_max, spatial_key, touches)


def sweep_leaf_size(positions, ranges, cluster_min, cluster_max, spatial_key, touches) -> None:
    """Measures what the count of lights in one leaf costs.

    The class table says the tree is worst on the small lights and best on the large ones. That is
    the opposite of what a size class attacks. It happens because the ratio follows how much larger
    the leaf box is than one light inside it. A leaf of 32 small lights covers far more than any one
    of them. A leaf of 32 large lights covers little more than one of them.

    So the lever for a small light is a leaf that holds fewer lights, and not a leaf that holds lights
    of one size.
    """
    print("\n" + "=" * 86)
    print("The count of lights in one leaf")
    print("=" * 86)

    irreducible = int(touches.sum())
    order = np.argsort(spatial_key, kind="stable")

    print(f"\n  {'lights per leaf':>16} {'leaves':>7} {'tests':>12} {'ratio':>7} "
          f"{'node tests':>11} {'against 32':>12}")

    measured = {}
    for size in (2, 4, 8, 16, 32, 64):
        leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, size)
        counts = vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max)
        # A smaller leaf gives more node boxes, and a cluster tests every node it reaches. The count
        # of (cluster, node) pairs is that cost, and it grows as the leaf shrinks.
        measured[size] = (len(leaf_min), int(counts.sum()) * size, int(counts.sum()))

    baseline = measured[32][1]
    for size, (leaves, tests, node_tests) in measured.items():
        print(f"  {size:>16} {leaves:>7} {tests:>12} {tests / irreducible:>7.2f} "
              f"{node_tests:>11} {100.0 * (tests - baseline) / baseline:>11.1f}%")

    print(f"\n  {'lights per leaf':>16} {'sphere + node tests':>21} {'against 32':>12}")
    both_at_32 = measured[32][1] + measured[32][2]
    for size, (_, tests, node_tests) in measured.items():
        both = tests + node_tests
        print(f"  {size:>16} {both:>21} {100.0 * (both - both_at_32) / both_at_32:>11.1f}%")

    print("\nA smaller leaf costs more node boxes to build and to test. The node test count above is")
    print("the count of (cluster, node) pairs at the leaf level, and it grows as the leaf shrinks.")


if __name__ == "__main__":
    main()
