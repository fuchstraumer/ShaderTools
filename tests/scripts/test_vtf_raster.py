"""Verifies that the raster path of the harness works, with the smallest pass that VTF holds.

Run it with:  py tests/scripts/test_vtf_raster.py

This script proves the harness and not the technique. The compute scripts already drive the shaders
that do the work. Nothing before this one drove a vertex stage, a fragment stage, a render target, or
a texture. Each raster pass in VolumeTiledForwardShading needs all four, so this is the first step.

VtfDebugTexture is the pass under test, because it is the smallest one. It draws three vertices with
no vertex buffer, reads one texture, and writes one color. So a failure has few places to hide.

**The check.** The pass reads a texture and draws it over the whole viewport. The render target is the
same size as the source texture and the sampler takes one texel. Therefore each output texel must
equal its source texel exactly. This is not a tolerance. An exact comparison works here because the
color format is float and the filter mixes nothing.

**The control.** The same pass with no vertices must leave the clear color. A check that passes on
both arms measures the clear and not the draw.
"""

import sys

import numpy as np

import vtf_support as vtf
from vtf_support import Results

SIZE = 64


def test_fullscreen_blit(ctx: vtf.VtfDevice, results: Results):
    """VtfDebugTexture must copy the source texture to the render target texel for texel."""
    print("\nVtfDebugTexture")

    rng = np.random.default_rng(3)
    # Noise, and not a ramp or a flat color. A pass that samples the wrong texel still gives a
    # plausible image from a ramp. Noise makes any wrong texel a large error.
    source = rng.uniform(0.0, 1.0, size=(SIZE, SIZE, 4)).astype(np.float32)

    pipeline = ctx.raster("VtfDebugTextureVS", "VtfDebugTextureFS")
    resources = {"DebugTexture": ctx.texture(source), "DebugSampler": ctx.point_sampler()}

    drawn = pipeline.draw(ctx.render_target(SIZE, SIZE), 3, resources)

    results.check(drawn.shape == source.shape,
                  "the render target reads back at the size it was made",
                  f"{drawn.shape} against {source.shape}")

    largest_error = float(np.abs(drawn - source).max())
    results.check(largest_error == 0.0,
                  "each output texel equals its source texel",
                  f"largest error {largest_error}")

    # The clear color is zero, so a pass that covers only part of the viewport leaves black there.
    # This check names that failure directly, because the error above states a number and not a place.
    covered = np.count_nonzero(drawn.any(axis=2))
    results.check(covered == SIZE * SIZE,
                  "the triangle covers every pixel of the viewport",
                  f"{covered} pixels of {SIZE * SIZE} hold a value")

    # The control. No vertices means no fragment, so the target must hold the clear color alone. A
    # check that passes here as well would be measuring the clear, and would prove nothing.
    empty = pipeline.draw(ctx.render_target(SIZE, SIZE), 0, resources)
    results.check(bool(np.all(empty == 0.0)),
                  "a draw of no vertices leaves the clear color",
                  f"largest value {float(np.abs(empty).max())}")


def main() -> int:
    results = Results()
    print("Loading VolumeTiledForwardShading ...")
    ctx = vtf.VtfDevice()
    print(f"device: {ctx.device.info.adapter_name} ({ctx.device.info.api_name})")

    test_fullscreen_blit(ctx, results)

    print(f"\n{results.passed} passed, {results.failed} failed")
    return 1 if results.failed else 0


if __name__ == "__main__":
    sys.exit(main())
