"""Verifies the VolumeTiledForwardShading compute shaders against numpy references.

Run it with:  py tests/scripts/test_vtf_compute.py

Each check states what it compares. A check that compares a shader against itself proves nothing, so
each one either compares against numpy, or compares the wave arm against the shared-memory arm, which
are two independent implementations of the same answer.

The script writes one image, vtf_verification.png, next to itself. The image is the evidence a number
cannot give: whether the Morton order really keeps neighbours together, and whether the BVH boxes
really hold their lights.
"""

import sys

import numpy as np

import vtf_support as vtf


class Results:
    def __init__(self) -> None:
        self.passed = 0
        self.failed = 0

    def check(self, condition: bool, description: str, detail: str = "") -> bool:
        if condition:
            self.passed += 1
            print(f"  PASS  {description}")
        else:
            self.failed += 1
            print(f"  FAIL  {description}")
            if detail:
                print(f"        {detail}")
        return condition


# ---------------------------------------------------------------------------------------------


def test_radix_sort(ctx: vtf.VtfDevice, results: Results):
    """The sort must order the keys, carry each value with its key, and give the same answer on both
    arms of VTF_USE_WAVE_OPS."""
    print("\nVtfRadixSort")

    count = 256
    rng = np.random.default_rng(7)
    keys = rng.integers(0, 1 << 30, size=count, dtype=np.uint32)
    values = np.arange(count, dtype=np.uint32)

    outputs = {}
    for label, constants in (("wave", {"VTF_USE_WAVE_OPS": True}),
                             ("lds", {"VTF_USE_WAVE_OPS": False})):
        out_keys = ctx.zeros(count)
        out_values = ctx.zeros(count)
        kernel = ctx.kernel("VtfRadixSort", constants)
        kernel.dispatch([count, 1, 1], {
            "InputKeys": ctx.buffer(keys),
            "InputValues": ctx.buffer(values),
            "OutputKeys": out_keys,
            "OutputValues": out_values,
            "MergePathPartitions": ctx.zeros(64),
            "Params": {"NumElements": count, "ChunkSize": count, "Padding": (0, 0)},
        })
        outputs[label] = (ctx.read(out_keys), ctx.read(out_values))

    expected = np.sort(keys)
    for label, (got_keys, got_values) in outputs.items():
        results.check(np.array_equal(got_keys, expected),
                      f"{label} arm sorts the keys",
                      f"first mismatch at {np.argmax(got_keys != expected)}")
        results.check(np.array_equal(keys[got_values], got_keys),
                      f"{label} arm carries each value with its key")

    results.check(np.array_equal(outputs["wave"][0], outputs["lds"][0]),
                  "both arms of VTF_USE_WAVE_OPS agree")

    # The reference entry point must not change when the axis changes. This is what
    # ExpectedAxisInfluence asserts inside the cooker.
    ref_out = ctx.zeros(count)
    ctx.kernel("VtfRadixSort_NoWaveOps", {"VTF_USE_WAVE_OPS": True}).dispatch([count, 1, 1], {
        "InputKeys": ctx.buffer(keys), "InputValues": ctx.buffer(values),
        "OutputKeys": ref_out, "OutputValues": ctx.zeros(count),
        "MergePathPartitions": ctx.zeros(64),
        "Params": {"NumElements": count, "ChunkSize": count, "Padding": (0, 0)},
    })
    results.check(np.array_equal(ctx.read(ref_out), expected),
                  "the reference entry point sorts, and ignores the wave axis")


def test_merge_sort(ctx: vtf.VtfDevice, results: Results):
    """The radix sort must produce one sorted chunk for each thread group, and the merge sort must
    join those chunks into one sorted list.

    The pass structure copies the frame code in DiamondDogs, `MergeSort` in
    modules/VtfModule/src/vtfTasksAndSteps.cpp. Each iteration runs the partition pass and then the
    merge pass, swaps the buffers, and doubles the chunk size, until one chunk is left.
    """
    print("\nVtfRadixSort chunks, then VtfMergeSort")

    chunk_size = 256          # VTF_RADIX_SORT_NUM_THREADS
    merge_threads = 256       # VTF_MERGE_SORT_NUM_THREADS
    values_per_group = 1024   # VTF_MERGE_VALUES_PER_GROUP

    count = 4096
    rng = np.random.default_rng(19)
    keys = rng.integers(0, 1 << 30, size=count, dtype=np.uint32)
    values = np.arange(count, dtype=np.uint32)

    src_keys, src_values = ctx.buffer(keys), ctx.buffer(values)
    dst_keys, dst_values = ctx.zeros(count), ctx.zeros(count)
    partitions = ctx.zeros(count // chunk_size + 8)

    def sort_params(chunk):
        return {"NumElements": count, "ChunkSize": chunk, "Padding": (0, 0)}

    def resources(input_keys, input_values, output_keys, output_values, chunk):
        return {
            "InputKeys": input_keys, "InputValues": input_values,
            "OutputKeys": output_keys, "OutputValues": output_values,
            "MergePathPartitions": partitions,
            "Params": sort_params(chunk),
        }

    # ---- Step 1: radix sort each chunk -------------------------------------------------------
    group_count = count // chunk_size
    ctx.kernel("VtfRadixSort").dispatch(
        [group_count * chunk_size, 1, 1],
        resources(src_keys, src_values, dst_keys, dst_values, chunk_size))

    chunked = ctx.read(dst_keys)
    chunk_view = chunked.reshape(group_count, chunk_size)
    input_view = keys.reshape(group_count, chunk_size)

    # Each chunk must be sorted AND hold exactly the keys of the matching input chunk. The second half
    # of that is what makes the check mean anything: a chunk of all zeros is sorted, so a group base
    # offset of zero leaves every chunk past the first full of zeros and passes a sortedness test.
    sorted_rows = 0
    matching_rows = 0
    for row in range(group_count):
        if np.all(np.diff(chunk_view[row].astype(np.int64)) >= 0):
            sorted_rows += 1
        if np.array_equal(np.sort(chunk_view[row]), np.sort(input_view[row])):
            matching_rows += 1

    results.check(sorted_rows == group_count,
                  f"the radix sort orders all {group_count} chunks",
                  f"{group_count - sorted_rows} chunks are unsorted")
    results.check(matching_rows == group_count,
                  f"each of the {group_count} chunks holds its own keys and not another chunk's",
                  f"{group_count - matching_rows} chunks read the wrong range, so the group base "
                  f"offset is not reaching the buffer index")
    results.check(np.array_equal(np.sort(chunked), np.sort(keys)),
                  "the chunked result is a permutation of the input, so no key was lost or duplicated")

    # ---- Step 2: merge the chunks --------------------------------------------------------------
    src_keys, dst_keys = dst_keys, src_keys
    src_values, dst_values = dst_values, src_values

    num_chunks = -(-count // chunk_size)
    passes = 0

    while num_chunks > 1:
        passes += 1
        num_sort_groups = num_chunks // 2
        groups_per_sort_group = -(-(chunk_size * 2) // values_per_group)

        # The partition buffer is cleared before each partition pass, as vkCmdFillBuffer does.
        partitions = ctx.zeros(count // chunk_size + 8)

        total_partitions = (groups_per_sort_group + 1) * num_sort_groups
        partition_groups = -(-total_partitions // merge_threads)
        ctx.kernel("VtfMergeSort", {"VTF_SORT_PASS": 0}).dispatch(
            [partition_groups * merge_threads, 1, 1],
            resources(src_keys, src_values, dst_keys, dst_values, chunk_size))

        values_per_sort_group = min(chunk_size * 2, count)
        merge_groups = -(-values_per_sort_group // values_per_group) * num_sort_groups
        ctx.kernel("VtfMergeSort", {"VTF_SORT_PASS": 1}).dispatch(
            [merge_groups * merge_threads, 1, 1],
            resources(src_keys, src_values, dst_keys, dst_values, chunk_size))

        src_keys, dst_keys = dst_keys, src_keys
        src_values, dst_values = dst_values, src_values

        chunk_size *= 2
        num_chunks = -(-count // chunk_size)

    got_keys = ctx.read(src_keys)
    got_values = ctx.read(src_values)
    expected = np.sort(keys)

    results.check(passes == 4, f"the merge ran the expected {passes} passes for {count} elements")
    results.check(np.array_equal(got_keys, expected),
                  "the merged list is fully sorted",
                  f"{int((got_keys != expected).sum())} of {count} slots differ")
    results.check(np.array_equal(keys[got_values], got_keys),
                  "each value still travels with its own key")

    # The three stages the picture draws: the input, the radix output of 16 chunks, and the merged
    # list. The middle one is the stage that a wrong chunk base offset changes.
    return keys, chunked, got_keys


def test_reduce_light_aabbs(ctx: vtf.VtfDevice, results: Results):
    """The reduction must find the box that holds every light sphere, on both arms."""
    print("\nVtfReduceLightAABBs")

    count = 4096
    lights, positions, ranges = vtf.make_lights(count)
    want_min, want_max = vtf.reference_light_aabb(positions.astype(np.float64),
                                                  ranges.astype(np.float64))

    boxes = {}
    for label, use_wave in (("wave", True), ("lds", False)):
        out = ctx.buffer(np.zeros(2, dtype=vtf.AABB_DTYPE))
        kernel = ctx.kernel("VtfReduceLightAABBs", {
            "VTF_USE_WAVE_OPS": use_wave,
            "VTF_REDUCTION_TYPE": 0,
        })
        # One thread group, so the single group result lands at index 0.
        kernel.dispatch([256, 1, 1], {
            "Counts": {"NumPointLights": count, "NumSpotLights": 0,
                       "NumDirectionalLights": 0, "Padding": 0},
            "PointLights": ctx.buffer(lights),
            "SpotLights": ctx.buffer(np.zeros(1, dtype=vtf.SPOT_LIGHT_DTYPE)),
            "LightAABBs": out,
            "Params": {"NumThreadGroupsX": 1, "ReductionNumElements": count,
                       "Padding": (0, 0)},
        })
        box = ctx.read(out, vtf.AABB_DTYPE)[0]
        boxes[label] = (box["Min"].astype(np.float64), box["Max"].astype(np.float64))

        results.check(np.allclose(box["Min"], want_min, atol=1e-3),
                      f"{label} arm finds the lower corner",
                      f"got {box['Min']} want {want_min}")
        results.check(np.allclose(box["Max"], want_max, atol=1e-3),
                      f"{label} arm finds the upper corner",
                      f"got {box['Max']} want {want_max}")

    results.check(
        np.array_equal(boxes["wave"][0], boxes["lds"][0])
        and np.array_equal(boxes["wave"][1], boxes["lds"][1]),
        "both arms of VTF_USE_WAVE_OPS agree")

    return lights, positions, ranges, want_min, want_max


def test_morton_codes(ctx: vtf.VtfDevice, results: Results, lights, positions,
                      root_min, root_max):
    """The Morton code must match the numpy reference for every light."""
    print("\nVtfComputeMortonCodes")

    count = len(lights)
    root = np.zeros(1, dtype=vtf.AABB_DTYPE)
    root["Min"][0] = root_min
    root["Max"][0] = root_max

    codes_buffer = ctx.zeros(count)
    indices_buffer = ctx.zeros(count)

    ctx.kernel("VtfComputeMortonCodes").dispatch([count, 1, 1], {
        "PointLights": ctx.buffer(lights),
        "SpotLights": ctx.buffer(np.zeros(1, dtype=vtf.SPOT_LIGHT_DTYPE)),
        "PointLightIndices": indices_buffer,
        "SpotLightIndices": ctx.zeros(1),
        "PointLightMortonCodes": codes_buffer,
        "SpotLightMortonCodes": ctx.zeros(1),
        "Counts": {"NumPointLights": count, "NumSpotLights": 0,
                   "NumDirectionalLights": 0, "Padding": 0},
        "LightAABBs": ctx.buffer(root),
    })

    got_codes = ctx.read(codes_buffer)
    got_indices = ctx.read(indices_buffer)

    want_cells = vtf.reference_morton_cells(positions, root_min, root_max)
    want_codes = vtf.morton_from_cells(want_cells)
    got_cells = vtf.cells_from_morton(got_codes)

    results.check(np.array_equal(got_indices, np.arange(count, dtype=np.uint32)),
                  "the index list starts as the identity")

    # The bit interleave must be exact. A wrong shift moves a light to a distant cell, and the check
    # below on cell distance would not catch that if the cells themselves were compared first.
    results.check(np.array_equal(vtf.morton_from_cells(got_cells), got_codes),
                  "the interleave round trips, so the code carries exactly the cell")

    exact = int((got_codes == want_codes).sum())

    # A light whose scaled coordinate lands on a cell boundary can round either way, because the GPU
    # may contract the multiply and the add into one instruction and numpy may not. A difference of one
    # cell there says nothing about the shader. A difference of more than one does.
    cell_error = np.abs(got_cells.astype(np.int64) - want_cells.astype(np.int64)).max(axis=1)
    off_by_more_than_one = int((cell_error > 1).sum())

    results.check(off_by_more_than_one == 0,
                  "every quantized cell matches the reference to within one cell",
                  f"{off_by_more_than_one} of {count} are further away")
    results.check(exact >= count - count // 1000,
                  "at least 99.9 percent of the codes match exactly",
                  f"{exact} of {count} match exactly")
    results.check(int(got_codes.max()) < (1 << 30),
                  "every code fits in 30 bits")

    return got_codes


def test_morton_locality(results: Results, positions, codes):
    """A Morton code is only useful when it keeps neighbours together. This compares the mean distance
    between lights that are next to each other after the sort against the mean distance between random
    pairs. The sorted distance must be far smaller."""
    print("\nMorton locality")

    order = np.argsort(codes, kind="stable")
    ordered = positions[order]

    neighbour = np.linalg.norm(np.diff(ordered, axis=0), axis=1).mean()

    rng = np.random.default_rng(3)
    shuffled = positions[rng.permutation(len(positions))]
    random_pair = np.linalg.norm(np.diff(shuffled, axis=0), axis=1).mean()

    results.check(neighbour < random_pair * 0.5,
                  "sorting by Morton code puts near lights next to each other",
                  f"neighbour {neighbour:.2f} vs random {random_pair:.2f}")
    return order, neighbour, random_pair


def test_bvh_build(ctx: vtf.VtfDevice, results: Results, lights, positions, ranges, order):
    """Every leaf node must hold the box of its own run of lights."""
    print("\nVtfBuildBVH")

    branching = 32
    count = len(lights)
    levels = 1
    capacity = branching
    while capacity < count and levels < 7:
        capacity *= branching
        levels += 1

    first_node_index = [0, 1, 33, 1057, 33825, 1082401, 34636833]
    node_count = first_node_index[min(levels, 6)] + (count + branching - 1) // branching + branching

    bvh = ctx.buffer(np.zeros(node_count, dtype=vtf.AABB_DTYPE))

    ctx.kernel("VtfBuildBVH", {"VTF_BUILD_STAGE": 0}).dispatch([count, 1, 1], {
        "PointLightBVH": bvh,
        "SpotLightBVH": ctx.buffer(np.zeros(64, dtype=vtf.AABB_DTYPE)),
        "PointLights": ctx.buffer(lights),
        "SpotLights": ctx.buffer(np.zeros(1, dtype=vtf.SPOT_LIGHT_DTYPE)),
        "BvhParams": {"PointLightLevels": levels, "SpotLightLevels": 0,
                      "ChildLevel": 0, "Padding": 0},
        "Counts": {"NumPointLights": count, "NumSpotLights": 0,
                   "NumDirectionalLights": 0, "Padding": 0},
        "PointLightIndices": ctx.buffer(order.astype(np.uint32)),
        "SpotLightIndices": ctx.zeros(1),
    })

    nodes = ctx.read(bvh, vtf.AABB_DTYPE)
    want_min, want_max = vtf.reference_bvh_leaves(positions.astype(np.float64),
                                                  ranges.astype(np.float64),
                                                  order, branching)

    leaf_base = first_node_index[levels - 1]
    leaf_count = len(want_min)
    got_min = nodes["Min"][leaf_base:leaf_base + leaf_count].astype(np.float64)
    got_max = nodes["Max"][leaf_base:leaf_base + leaf_count].astype(np.float64)

    results.check(np.allclose(got_min, want_min, atol=1e-3),
                  "every leaf node holds the lower corner of its lights",
                  f"worst error {np.abs(got_min - want_min).max():.5f}")
    results.check(np.allclose(got_max, want_max, atol=1e-3),
                  "every leaf node holds the upper corner of its lights",
                  f"worst error {np.abs(got_max - want_max).max():.5f}")

    return got_min, got_max, leaf_count


def test_assign_lights_to_clusters(ctx: vtf.VtfDevice, results: Results, lights, positions,
                                   ranges, order, codes):
    """The traversal must find exactly the lights whose sphere touches each cluster box.

    This is the check that matters most. It exercises the node stack, the wave aggregated atomic, and
    the ballot count, and every one of those held a defect. The reference is a brute force sphere
    against box test over every light, which shares no code with the shader.
    """
    print("\nVtfAssignLightsToClusters")

    branching = 32
    count = len(lights)

    levels = 1
    capacity = branching
    while capacity < count and levels < 7:
        capacity *= branching
        levels += 1

    first_node_index = [0, 1, 33, 1057, 33825, 1082401, 34636833]
    node_count = first_node_index[levels] + branching

    # Build the whole tree: the leaf level first, then one interior level at a time.
    bvh = ctx.buffer(np.zeros(node_count, dtype=vtf.AABB_DTYPE))
    empty_spot = ctx.buffer(np.zeros(1, dtype=vtf.SPOT_LIGHT_DTYPE))
    index_buffer = ctx.buffer(order.astype(np.uint32))

    def bvh_resources(child_level):
        return {
            "PointLightBVH": bvh,
            "SpotLightBVH": ctx.buffer(np.zeros(64, dtype=vtf.AABB_DTYPE)),
            "PointLights": ctx.buffer(lights),
            "SpotLights": empty_spot,
            "BvhParams": {"PointLightLevels": levels, "SpotLightLevels": 0,
                          "ChildLevel": child_level, "Padding": 0},
            "Counts": {"NumPointLights": count, "NumSpotLights": 0,
                       "NumDirectionalLights": 0, "Padding": 0},
            "PointLightIndices": index_buffer,
            "SpotLightIndices": ctx.zeros(1),
        }

    ctx.kernel("VtfBuildBVH", {"VTF_BUILD_STAGE": 0}).dispatch([count, 1, 1], bvh_resources(0))

    upper = ctx.kernel("VtfBuildBVH", {"VTF_BUILD_STAGE": 1})
    for child_level in range(levels - 1, 0, -1):
        node_total = branching ** child_level
        upper.dispatch([node_total, 1, 1], bvh_resources(child_level))

    # A grid of cluster boxes over the scene. The boxes are synthetic, so this test covers the
    # traversal and not the cluster AABB pass.
    grid = 4
    lower = positions.min(axis=0) - 10.0
    upper_corner = positions.max(axis=0) + 10.0
    size = (upper_corner - lower) / grid

    cluster_boxes = []
    for z in range(grid):
        for y in range(grid):
            for x in range(grid):
                origin = lower + np.array([x, y, z], dtype=np.float32) * size
                cluster_boxes.append((origin, origin + size))

    cluster_count = len(cluster_boxes)
    cluster_array = np.zeros(cluster_count, dtype=vtf.AABB_DTYPE)
    for index, (box_min, box_max) in enumerate(cluster_boxes):
        cluster_array["Min"][index] = box_min
        cluster_array["Max"][index] = box_max

    list_capacity = cluster_count * 1024
    point_list = ctx.zeros(list_capacity)
    point_grid = ctx.zeros(cluster_count * 2)

    for label, use_wave in (("wave", True), ("lds", False)):
        counter = ctx.zeros(1)
        point_list = ctx.zeros(list_capacity)
        point_grid = ctx.zeros(cluster_count * 2)

        kernel = ctx.kernel("VtfAssignLightsToClusters", {"VTF_USE_WAVE_OPS": use_wave})
        kernel.dispatch([cluster_count * 32, 1, 1], {
            "UniqueClusters": ctx.buffer(np.arange(cluster_count, dtype=np.uint32)),
            "ClusterAABBs": ctx.buffer(cluster_array),
            "BvhParams": {"PointLightLevels": levels, "SpotLightLevels": 0,
                          "ChildLevel": 0, "Padding": 0},
            "Counts": {"NumPointLights": count, "NumSpotLights": 0,
                       "NumDirectionalLights": 0, "Padding": 0},
            "PointLights": ctx.buffer(lights),
            "SpotLights": empty_spot,
            "PointLightBVH": bvh,
            "SpotLightBVH": ctx.buffer(np.zeros(64, dtype=vtf.AABB_DTYPE)),
            "PointLightIndices": index_buffer,
            "SpotLightIndices": ctx.zeros(1),
            "PointLightIndexList": point_list,
            "SpotLightIndexList": ctx.zeros(list_capacity),
            "PointLightGrid": point_grid,
            "SpotLightGrid": ctx.zeros(cluster_count * 2),
            "PointLightIndexCounter": counter,
            "SpotLightIndexCounter": ctx.zeros(1),
        })

        grid_data = ctx.read(point_grid).reshape(cluster_count, 2)
        list_data = ctx.read(point_list)
        total = int(ctx.read(counter)[0])

        # Brute force reference. The shader uses a squared distance test against the box, so the
        # reference does the same arithmetic in numpy over every light and every cluster.
        mismatched = 0
        overlapping = 0
        for index, (box_min, box_max) in enumerate(cluster_boxes):
            closest = np.clip(positions, box_min, box_max)
            distance_squared = ((positions - closest) ** 2).sum(axis=1)
            expected = set(np.nonzero(distance_squared <= ranges ** 2)[0].tolist())

            offset, found = int(grid_data[index][0]), int(grid_data[index][1])
            got = set(list_data[offset:offset + found].tolist())

            if got != expected:
                mismatched += 1
            overlapping += len(expected)

        results.check(mismatched == 0,
                      f"{label} arm assigns exactly the touching lights to all {cluster_count} clusters",
                      f"{mismatched} clusters disagree with the brute force reference")
        results.check(total == int(grid_data[:, 1].sum()),
                      f"{label} arm reserves exactly as many slots as it writes",
                      f"counter {total} against {int(grid_data[:, 1].sum())} written")
        if label == "wave":
            results.check(overlapping > 0,
                          "the scene actually puts lights in clusters, so the test can fail",
                          f"{overlapping} light and cluster pairs overlap")


def test_cluster_aabbs(ctx: vtf.VtfDevice, results: Results):
    """The cluster boxes must tile the frustum without a gap, and grow with depth."""
    print("\nVtfComputeClusterAABBs")

    grid = (24, 16, 24)
    cluster_count = grid[0] * grid[1] * grid[2]
    near, far = 0.1, 500.0
    # The exponential depth ratio that gives `grid[2]` slices between near and far.
    near_k = (far / near) ** (1.0 / grid[2])

    screen = (1920.0, 1080.0)
    cluster_pixels = (screen[0] / grid[0], screen[1] / grid[1])

    aspect = screen[0] / screen[1]
    fov_y = np.radians(60.0)
    focal = 1.0 / np.tan(fov_y * 0.5)
    projection = np.zeros((4, 4), dtype=np.float32)
    projection[0, 0] = focal / aspect
    projection[1, 1] = focal
    projection[2, 2] = far / (near - far)
    projection[2, 3] = near * far / (near - far)
    projection[3, 2] = -1.0
    inverse_projection = np.linalg.inv(projection.astype(np.float64)).astype(np.float32)

    identity = np.eye(4, dtype=np.float32)
    matrices = np.concatenate([
        identity.T.reshape(-1), projection.T.reshape(-1), identity.T.reshape(-1),
        identity.T.reshape(-1), inverse_projection.T.reshape(-1), identity.T.reshape(-1),
    ]).astype(np.float32)

    out = ctx.buffer(np.zeros(cluster_count, dtype=vtf.AABB_DTYPE))
    ctx.kernel("VtfComputeClusterAABBs").dispatch([cluster_count, 1, 1], {
        "Clusters": {
            "GridDim": grid,
            "ViewNear": near,
            "ClusterSizeInPixels": (int(cluster_pixels[0]), int(cluster_pixels[1])),
            "NearK": near_k,
            "LogGridDimY": 1.0 / np.log(near_k),
        },
        "Matrices": {"viewMatrix": identity, "projectionMatrix": projection,
                     "viewProjectionMatrix": projection, "inverseViewMatrix": identity,
                     "inverseProjectionMatrix": inverse_projection,
                     "inverseViewProjectionMatrix": inverse_projection},
        "ClusterAABBs": out,
    })

    boxes = ctx.read(out, vtf.AABB_DTYPE)
    lower = boxes["Min"].astype(np.float64)
    upper = boxes["Max"].astype(np.float64)

    results.check(np.isfinite(lower).all() and np.isfinite(upper).all(),
                  "every cluster box is finite")
    results.check((upper >= lower).all(),
                  "every cluster box has its corners the right way round",
                  f"{int((upper < lower).any(axis=1).sum())} boxes are inverted")

    # A cluster further from the eye must be at least as deep as one nearer to it, because the depth
    # slices grow by a constant ratio.
    depth = (upper[:, 2] - lower[:, 2]).reshape(grid[2], grid[1], grid[0])
    slice_depth = depth.mean(axis=(1, 2))
    results.check(np.all(np.diff(slice_depth) > 0),
                  "each depth slice is deeper than the one in front of it")


def main() -> int:
    results = Results()
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    test_radix_sort(ctx, results)
    sort_input, sort_chunked, sort_merged = test_merge_sort(ctx, results)
    lights, positions, ranges, root_min, root_max = test_reduce_light_aabbs(ctx, results)
    codes = test_morton_codes(ctx, results, lights, positions, root_min, root_max)
    order, neighbour, random_pair = test_morton_locality(results, positions, codes)
    leaf_min, leaf_max, leaf_count = test_bvh_build(ctx, results, lights, positions,
                                                    ranges, order)
    test_cluster_aabbs(ctx, results)
    test_assign_lights_to_clusters(ctx, results, lights, positions, ranges, order, codes)

    try:
        import vtf_visualize
        path = vtf_visualize.render(positions, ranges, codes, order, leaf_min, leaf_max,
                                    root_min, root_max, neighbour, random_pair,
                                    sort_input, sort_chunked, sort_merged)
        print(f"\nimage written: {path}")
    except ImportError as exc:
        print(f"\n(no image written: {exc})")

    print(f"\n{results.passed} passed, {results.failed} failed")
    return 1 if results.failed else 0


if __name__ == "__main__":
    sys.exit(main())
