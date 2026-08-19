"""Times VtfAssignLightsToClusters on the device, and checks the cost model against the clock.

Run it with:  py tests/scripts/vtf_timing.py

This is a measurement and not a test. It prints numbers and it asserts nothing.

**Why this script exists.** vtf_leaf_overlap.py and vtf_size_binning.py count sphere tests in numpy and
then rank the arms by that count. Nothing has checked that a sphere test count predicts a time. This
script measures the time, so the model gets a second opinion from the device clock.

**What it can compare with no change to a shader.** The order of the lights arrives as a buffer,
`PointLightIndices`. So Morton, Hilbert, and no order at all are three arms of the same shader, and
the device can time all three today. The grid is a set of parameters, so the near plane is the same.

**What it cannot compare.** VTF_BVH_BRANCHING is 32 in the shader and in the host arithmetic, so a
leaf of 4 lights needs a change to VtfBuildBVH and to the traversal. The count of lights in one leaf
is the largest lever the model found, and this script cannot reach it.

**How to read the result.** If the measured time follows the predicted test count across every arm,
the model holds and the arms it cannot time can be trusted. If it does not, the model is wrong, and
every ranking that came from it needs to be taken again.
"""

import numpy as np

import vtf_support as vtf

LIGHT_COUNT = 4096
BRANCHING = 32
FIRST_NODE_INDEX = [0, 1, 33, 1057, 33825, 1082401, 34636833]
# How many times each arm runs. The best time of all the rounds is the one the table holds.
ROUNDS = 5


class Tree:
    """One BVH over one set of lights, rebuilt on the device for each order.

    **Every buffer is allocated once.** A benchmark that allocates a buffer for each arm measures the
    allocator as well as the shader, and the two do not separate afterwards. Measured on this device,
    a rebuilt set of buffers made the same dispatch report 0.33 ms on the first pass and 2.34 ms by
    the fifth, and the spread grew with it. So `rebuild` writes into the buffers that already exist.
    """

    def __init__(self, ctx: vtf.VtfDevice, lights, cluster_count: int, capacity: int) -> None:
        self.ctx = ctx
        self.count = len(lights)

        self.levels = 1
        node_capacity = BRANCHING
        while node_capacity < self.count and self.levels < 7:
            node_capacity *= BRANCHING
            self.levels += 1

        node_count = FIRST_NODE_INDEX[self.levels] + BRANCHING

        self.bvh = ctx.buffer(np.zeros(node_count, dtype=vtf.AABB_DTYPE))
        self.lights = ctx.buffer(lights)
        self.indices = ctx.zeros(self.count)
        self.empty_light = ctx.buffer(np.zeros(1, dtype=vtf.SPOT_LIGHT_DTYPE))
        self.empty_bvh = ctx.buffer(np.zeros(64, dtype=vtf.AABB_DTYPE))
        self.empty_index = ctx.zeros(1)

        self.unique_clusters = ctx.buffer(np.arange(cluster_count, dtype=np.uint32))
        self.cluster_boxes = ctx.buffer(np.zeros(cluster_count, dtype=vtf.AABB_DTYPE))
        self.index_list = ctx.zeros(capacity)
        self.light_grid = ctx.zeros(cluster_count * 2)
        self.counter = ctx.zeros(1)

        self.leaf_builder = ctx.kernel("VtfBuildBVH", {"VTF_BUILD_STAGE": 0})
        self.upper_builder = ctx.kernel("VtfBuildBVH", {"VTF_BUILD_STAGE": 1})

    def rebuild(self, order: np.ndarray) -> None:
        self.ctx.upload(self.indices, order.astype(np.uint32))
        self.leaf_builder.dispatch([self.count, 1, 1], self.build_resources(0))
        for child_level in range(self.levels - 1, 0, -1):
            self.upper_builder.dispatch([BRANCHING ** child_level, 1, 1],
                                        self.build_resources(child_level))

    def counts(self) -> dict:
        return {"NumPointLights": self.count, "NumSpotLights": 0,
                "NumDirectionalLights": 0, "Padding": 0}

    def build_resources(self, child_level: int) -> dict:
        return {
            "PointLightBVH": self.bvh,
            "SpotLightBVH": self.empty_bvh,
            "PointLights": self.lights,
            "SpotLights": self.empty_light,
            "BvhParams": {"PointLightLevels": self.levels, "SpotLightLevels": 0,
                          "ChildLevel": child_level, "Padding": 0},
            "Counts": self.counts(),
            "PointLightIndices": self.indices,
            "SpotLightIndices": self.empty_index,
        }

    def set_clusters(self, cluster_array: np.ndarray) -> None:
        self.ctx.upload(self.cluster_boxes, cluster_array)

    def assign_resources(self) -> dict:
        return {
            "UniqueClusters": self.unique_clusters,
            "ClusterAABBs": self.cluster_boxes,
            "BvhParams": {"PointLightLevels": self.levels, "SpotLightLevels": 0,
                          "ChildLevel": 0, "Padding": 0},
            "Counts": self.counts(),
            "PointLights": self.lights,
            "SpotLights": self.empty_light,
            "PointLightBVH": self.bvh,
            "SpotLightBVH": self.empty_bvh,
            "PointLightIndices": self.indices,
            "SpotLightIndices": self.empty_index,
            "PointLightIndexList": self.index_list,
            "SpotLightIndexList": self.empty_index,
            "PointLightGrid": self.light_grid,
            "SpotLightGrid": self.light_grid,
            "PointLightIndexCounter": self.counter,
            "SpotLightIndexCounter": self.empty_index,
        }


def as_aabb_array(lower: np.ndarray, upper: np.ndarray) -> np.ndarray:
    boxes = np.zeros(len(lower), dtype=vtf.AABB_DTYPE)
    boxes["Min"] = lower.astype(np.float32)
    boxes["Max"] = upper.astype(np.float32)
    return boxes


def predicted_tests(positions, ranges, order, cluster_min, cluster_max) -> int:
    leaf_min, leaf_max = vtf.reference_bvh_leaves(positions, ranges, order, BRANCHING)
    counts = vtf.count_cluster_overlaps(leaf_min, leaf_max, cluster_min, cluster_max)
    return int(counts.sum()) * BRANCHING


def run_arms(ctx, tree, positions, ranges, arms) -> list:
    """Times every arm, round by round, and keeps the smallest time each one reached.

    **Every arm must sit in one round robin.** This device runs the same dispatch at two clock
    states, and they are up to 5 times apart. It holds a state for a while, so a group of arms timed
    together shares that state and a group timed later does not. Two groups therefore cannot be
    compared, whatever the statistic. Measured here: one configuration read 0.23 ms in the first
    group and 1.16 ms in the fourth, with the same buffers and the same work.

    Inside one state the median holds to 0.3 percent, so the noise is not random. It is the clock.
    Taking the smallest time over the rounds gives the time when the device was not busy, and that
    is the number that compares.

    An arm carries its own grid, because the near plane is a grid parameter, and the near plane must
    take part in the same round robin as the order of the lights.
    """
    prepared = []
    for name, order, grid in arms:
        cluster_min, cluster_max = grid.cluster_aabbs(ctx)
        prepared.append({
            "name": name,
            "order": order,
            "grid": grid,
            "boxes": as_aabb_array(cluster_min, cluster_max),
            "tests": predicted_tests(positions, ranges, order, cluster_min, cluster_max),
            "irreducible": int(vtf.count_clusters_per_light(
                positions, ranges, cluster_min, cluster_max).sum()),
            "ms": float("inf"),
            "appended": 0,
        })

    resources = tree.assign_resources()
    kernel = ctx.kernel("VtfAssignLightsToClusters", {"VTF_USE_WAVE_OPS": True})

    for _ in range(ROUNDS):
        for arm in prepared:
            tree.set_clusters(arm["boxes"])
            tree.rebuild(arm["order"])
            # The counter and the grid must start each run empty. The counter rises across the runs
            # otherwise, the list overflows, and every run after that measures a shader out of room.
            timing = ctx.time_dispatch(kernel, [arm["grid"].cluster_count * BRANCHING, 1, 1],
                                       resources, repeat=16, warmup=4,
                                       reset=[tree.counter, tree.light_grid])
            arm["ms"] = min(arm["ms"], timing["min"])
            # The counter after a run must equal the count numpy predicts. This check ties the
            # reference to the shader, and it is the reason the model can be trusted at all.
            arm["appended"] = int(ctx.read(tree.counter)[0])

    return prepared


def print_table(rows, title: str) -> None:
    print(f"\n{title}")
    print(f"{'arm':<28} {'predicted tests':>16} {'best ms':>9} {'appends':>10} "
          f"{'ns per test':>12} {'appends agree':>14}")
    print("-" * 94)
    for row in rows:
        per_test = row["ms"] * 1.0e6 / row["tests"]
        agree = "yes" if row["appended"] == row["irreducible"] else "NO"
        print(f"{row['name']:<28} {row['tests']:>16} {row['ms']:>9.4f} "
              f"{row['appended']:>10} {per_test:>12.4f} {agree:>14}")


def main() -> None:
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    grid = vtf.ClusterGrid(near=1.0)
    lights, positions, ranges = vtf.make_lights_realistic(grid, LIGHT_COUNT)
    print(f"\nscene: {LIGHT_COUNT} lights, median depth {np.median(-positions[:, 2]):.1f}")
    print(f"grid: near {grid.near}, {grid.cluster_count} clusters")

    root_min = (positions - ranges[:, None]).min(axis=0)
    root_max = (positions + ranges[:, None]).max(axis=0)
    cells = vtf.reference_morton_cells(positions, root_min, root_max, 10)

    unsorted = np.arange(LIGHT_COUNT)
    morton = np.argsort(vtf.morton_from_cells(cells, 10), kind="stable")
    hilbert = np.argsort(vtf.hilbert_from_cells(cells, 10), kind="stable")

    # Every arm sits in one list, so one round robin covers the order and the near plane together.
    arms = [
        ("unsorted, near 1.0, control", unsorted, vtf.ClusterGrid(near=1.0)),
        ("Morton, near 1.0", morton, vtf.ClusterGrid(near=1.0)),
        ("Hilbert, near 1.0", hilbert, vtf.ClusterGrid(near=1.0)),
        ("Morton, near 0.1", morton, vtf.ClusterGrid(near=0.1)),
        ("Morton, near 0.5", morton, vtf.ClusterGrid(near=0.5)),
        ("Hilbert, near 0.1", hilbert, vtf.ClusterGrid(near=0.1)),
    ]

    # One set of buffers, sized for the worst grid the script uses, and reused by every arm.
    worst_case = int(vtf.count_clusters_per_light(
        positions, ranges, *vtf.ClusterGrid(near=0.1).cluster_aabbs(ctx)).sum())
    tree = Tree(ctx, lights, grid.cluster_count, int(worst_case * 1.5) + 1024)

    rows = run_arms(ctx, tree, positions, ranges, arms)
    print_table(rows, f"Every arm, interleaved over {ROUNDS} rounds")

    print("\nDoes the model predict the clock?")
    print(f"  {'arm':<28} {'model':>9} {'clock':>9}")
    baseline = rows[1]
    for row in rows:
        by_model = 100.0 * (row["tests"] - baseline["tests"]) / baseline["tests"]
        by_clock = 100.0 * (row["ms"] - baseline["ms"]) / baseline["ms"]
        print(f"  {row['name']:<28} {by_model:>+8.1f}% {by_clock:>+8.1f}%")

    model = np.array([row["tests"] for row in rows], dtype=np.float64)
    clock = np.array([row["ms"] for row in rows], dtype=np.float64)
    print(f"\n  correlation of predicted tests against measured time: "
          f"{np.corrcoef(model, clock)[0, 1]:.4f}")
    print(f"  the same, with the control left out:                   "
          f"{np.corrcoef(model[1:], clock[1:])[0, 1]:.4f}")

    estimate_append_cost(rows)


def estimate_append_cost(rows) -> None:
    """Separates the cost of one sphere test from the cost of one append.

    The control and the Morton arm write the same count of appends and perform very different counts
    of sphere tests. So the difference between their times belongs to the tests alone, and it gives
    the cost of one test. What is left of the Morton time then belongs to the appends.

    This decides whether a method that writes each light straight into its clusters can win. That
    method performs no sphere test and the same appends.

    **Read this as an estimate and not as a result.** It rests on two points and on the assumption
    that the control differs from Morton in the count of tests only. The control also diverges more
    and reads memory less well, so a part of its extra time is not the tests. That makes the cost of
    one test too large here, and the share left for the appends too small. A microbenchmark that
    varies the appends alone is what settles it.
    """
    print("\n" + "=" * 94)
    print("What one sphere test costs, and what one append costs")
    print("=" * 94)

    control, morton = rows[0], rows[1]
    per_test = (control["ms"] - morton["ms"]) / (control["tests"] - morton["tests"])
    test_share = per_test * morton["tests"]
    append_share = morton["ms"] - test_share
    per_append = append_share / morton["appended"]

    print(f"\n  one sphere test        {per_test * 1.0e6:>8.4f} ns")
    print(f"  one append             {per_append * 1.0e6:>8.4f} ns, "
          f"{per_append / per_test:>5.1f} times a sphere test")
    print(f"\n  of the {morton['ms']:.4f} ms that Morton takes:")
    print(f"    {test_share:>8.4f} ms goes to {morton['tests']} sphere tests")
    print(f"    {append_share:>8.4f} ms goes to {morton['appended']} appends")
    print(f"\n  a method with no tests and the same appends would take about "
          f"{append_share:.4f} ms,")
    print(f"  which is {morton['ms'] / append_share:.1f} times less than the tree takes today.")


if __name__ == "__main__":
    main()
