"""Verifies the space filling curves that order the lights before the BVH build.

Run it with:  py tests/scripts/test_vtf_curves.py

No shader runs here. These are the numpy references, and a reference that nobody checks is not a
second opinion. The Hilbert rules arrive from a paper, and a table copied by hand from a paper is
exactly the kind of data that holds a transcription error.

**What proves a curve is correct.** A round trip proves only that the two tables are inverses. Two
tables that hold the same transcription error still round trip. The property that separates a Hilbert
curve from any other numbering of the cube is this: index i and index i + 1 must sit in cells that
touch on a face. So the walk over every index of the cube must take steps of length 1, every time.
Morton fails that test on purpose, and the size of its worst step is the reason to consider Hilbert.

Source of the rules: David Walker, "Algorithms for Encoding and Decoding 3D Hilbert Orderings",
UTC Research Institute, University of Tennessee at Chattanooga, August 2023.
"""

import sys

import numpy as np

import vtf_support as vtf
from vtf_support import Results


def all_cells(bits: int) -> np.ndarray:
    """Every cell of a cube of side 2**bits, in x fastest order."""
    side = 1 << bits
    grid = np.indices((side, side, side)).reshape(3, -1).T
    return grid[:, [2, 1, 0]].astype(np.uint32)


def test_paper_examples(results: Results):
    """The two worked examples of the paper. These catch a table copied into the wrong column."""
    print("\nHilbert, the worked examples of the paper")

    # Section 3.1: location (3, 3, 1) at depth 2 encodes to octal 63, which is 51.
    encoded = vtf.hilbert_from_cells(np.array([[3, 3, 1]], dtype=np.uint32), bits=2)
    results.check(int(encoded[0]) == 51,
                  "(3, 3, 1) at depth 2 encodes to 51",
                  f"got {int(encoded[0])}")

    # Section 3.2: index 37 at depth 2 decodes to location (0, 3, 2).
    decoded = vtf.cells_from_hilbert(np.array([37], dtype=np.uint32), bits=2)
    results.check(tuple(int(v) for v in decoded[0]) == (0, 3, 2),
                  "index 37 at depth 2 decodes to (0, 3, 2)",
                  f"got {tuple(int(v) for v in decoded[0])}")


def test_hilbert_is_a_bijection(results: Results, bits: int):
    """Every cell must take one index, and every index must take one cell."""
    print(f"\nHilbert, depth {bits}, the map is one to one")

    cells = all_cells(bits)
    index = vtf.hilbert_from_cells(cells, bits)

    count = len(cells)
    results.check(len(np.unique(index)) == count,
                  "each cell takes its own index",
                  f"{len(np.unique(index))} distinct indices for {count} cells")
    results.check(int(index.min()) == 0 and int(index.max()) == count - 1,
                  "the indices fill the range with no gap",
                  f"range {int(index.min())} to {int(index.max())}, expected 0 to {count - 1}")

    round_trip = vtf.cells_from_hilbert(index, bits)
    results.check(np.array_equal(round_trip, cells),
                  "decode undoes encode for every cell",
                  f"{int((round_trip != cells).any(axis=1).sum())} cells disagree")


def test_hilbert_walk_is_continuous(results: Results, bits: int):
    """The defining property. Index i and index i + 1 must touch on a face."""
    print(f"\nHilbert, depth {bits}, the walk is continuous")

    side = 1 << bits
    walk = vtf.cells_from_hilbert(np.arange(side ** 3, dtype=np.uint32), bits).astype(np.int64)
    steps = np.abs(np.diff(walk, axis=0)).sum(axis=1)

    results.check(bool(np.all(steps == 1)),
                  "every step of the walk has length 1",
                  f"{int((steps != 1).sum())} steps do not, and the longest is {int(steps.max())}")

    results.check(tuple(walk[0]) == (0, 0, 0),
                  "the walk starts at the origin",
                  f"starts at {tuple(int(v) for v in walk[0])}")


def test_anisotropic_morton_agrees_at_equal_bits(results: Results, bits: int):
    """The general interleave must reproduce the shipped one when every axis holds the same bits.

    Without this the two could drift apart, and a measurement that compares an anisotropic budget
    against the isotropic one would then compare two different interleaves as well.
    """
    print(f"\nMorton, depth {bits}, the general interleave against the shipped one")

    cells = all_cells(bits)
    shipped = vtf.morton_from_cells(cells, bits)
    general = vtf.morton_from_cells_anisotropic(cells, (bits, bits, bits))

    results.check(np.array_equal(shipped, general),
                  "an equal bit budget gives the shipped Morton code",
                  f"{int((shipped != general).sum())} cells disagree")


def test_morton_is_not_continuous(results: Results, bits: int):
    """The control. Morton must fail the test above, or the test proves nothing about Hilbert.

    A check that both curves pass would mean the check cannot tell them apart. This one names the size
    of the worst Morton jump, which is the number the Hilbert work is meant to remove.
    """
    print(f"\nMorton, depth {bits}, the control arm")

    side = 1 << bits
    walk = vtf.cells_from_morton(np.arange(side ** 3, dtype=np.uint32), bits).astype(np.int64)
    steps = np.abs(np.diff(walk, axis=0)).sum(axis=1)

    results.check(bool(np.any(steps > 1)),
                  "Morton takes steps longer than 1, so the continuity check can fail",
                  "Morton passed a test that only Hilbert must pass")
    print(f"        Morton: longest step {int(steps.max())}, "
          f"{int((steps > 1).sum())} of {len(steps)} steps jump")


def main() -> int:
    results = Results()

    test_paper_examples(results)
    for bits in (2, 3, 4):
        test_hilbert_is_a_bijection(results, bits)
        test_hilbert_walk_is_continuous(results, bits)
        test_anisotropic_morton_agrees_at_equal_bits(results, bits)
    test_morton_is_not_continuous(results, 4)

    print(f"\n{results.passed} passed, {results.failed} failed")
    return 1 if results.failed else 0


if __name__ == "__main__":
    sys.exit(main())
