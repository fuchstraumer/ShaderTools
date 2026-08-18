"""Support code for the VolumeTiledForwardShading verification scripts.

The scripts drive the real compute shaders on a real device and compare each result against a numpy
reference. They exist because these shaders have no python reference of their own, unlike OceanFft.

Two things here are worth reading before you write a new test.

**Binding.** Each VTF entry point takes `uniform ParameterBlock<T> resources`, which is an entry point
parameter and not a module-scope global. `ComputeKernel.dispatch(vars=...)` cannot reach an entry point
parameter. It raises nothing, the dispatch runs, and every resource reads as zero. Use `Dispatch`
below, which goes through `ShaderCursor.find_entry_point`, and which raises on a name that does not
exist.

**Permutation.** `LinkedKernel` builds the same synthetic module that the cooker builds: a one-line
Slang module holding `export static const <type> <NAME> = <value>;`, linked next to the real module.
The Slang linker matches it to the `extern static const` declaration by name. Passing `constants=` to
`LinkedKernel` therefore cooks one variant, and a test can compare two variants against each other.
"""

import pathlib

import numpy as np
import slangpy as spy

ASSET_ROOT = pathlib.Path(__file__).resolve().parents[1] / "assets"
VTF_DIR = ASSET_ROOT / "compute" / "VolumeTiledForwardShading"
VTF_MODULE = VTF_DIR / "VolumeTiledForwardShading.slang"

INCLUDE_PATHS = [
    ASSET_ROOT,
    ASSET_ROOT / "common",
    ASSET_ROOT / "materials",
    ASSET_ROOT / "render",
    VTF_DIR,
]

# Every buffer in these tests is read, written, and copied back, so one usage mask serves all of them.
BUFFER_USAGE = (
    spy.BufferUsage.shader_resource
    | spy.BufferUsage.unordered_access
    | spy.BufferUsage.copy_source
    | spy.BufferUsage.copy_destination
)

# Mirrors ddCommonTypes.slang. float3 Min, float Pad0, float3 Max, float Pad1.
AABB_DTYPE = np.dtype([("Min", np.float32, 3), ("Pad0", np.float32),
                       ("Max", np.float32, 3), ("Pad1", np.float32)])

# Mirrors VtfTypes.slang. Enabled is a uint and not a bool, because WGSL forbids bool in a storage
# buffer.
POINT_LIGHT_DTYPE = np.dtype([
    ("Position", np.float32, 4),
    ("PositionViewSpace", np.float32, 4),
    ("Color", np.float32, 3),
    ("Range", np.float32),
    ("Intensity", np.float32),
    ("Enabled", np.uint32),
    ("Padding", np.float32, 2),
])

SPOT_LIGHT_DTYPE = np.dtype([
    ("Position", np.float32, 4),
    ("PositionViewSpace", np.float32, 4),
    ("Direction", np.float32, 4),
    ("DirectionViewSpace", np.float32, 4),
    ("Color", np.float32, 3),
    ("SpotLightAngle", np.float32),
    ("Range", np.float32),
    ("Intensity", np.float32),
    ("Enabled", np.uint32),
    ("Padding", np.float32),
])

assert POINT_LIGHT_DTYPE.itemsize == 64, POINT_LIGHT_DTYPE.itemsize
assert SPOT_LIGHT_DTYPE.itemsize == 96, SPOT_LIGHT_DTYPE.itemsize
assert AABB_DTYPE.itemsize == 32, AABB_DTYPE.itemsize


class VtfDevice:
    """Holds the device and the loaded VTF module."""

    def __init__(self) -> None:
        self.device = spy.Device(
            compiler_options={"include_paths": [str(p) for p in INCLUDE_PATHS]}
        )
        self.module = self.device.load_module(str(VTF_MODULE))
        self.entry_points = {ep.name: ep for ep in self.module.entry_points}

    def buffer(self, data: np.ndarray) -> spy.Buffer:
        """Creates a buffer that holds `data`. The element stride comes from the array dtype.

        create_buffer takes a plain array and not a structured one, so a structured array travels as
        its own bytes. The element count and the stride still come from the dtype, so the buffer keeps
        the layout that the shader struct expects.
        """
        contiguous = np.ascontiguousarray(data)
        element_count = contiguous.shape[0]
        struct_size = contiguous.dtype.itemsize
        payload = contiguous.view(np.uint8).reshape(-1)

        return self.device.create_buffer(
            element_count=element_count,
            struct_size=struct_size,
            usage=BUFFER_USAGE,
            data=payload,
        )

    def zeros(self, count: int, dtype=np.uint32) -> spy.Buffer:
        return self.buffer(np.zeros(count, dtype=dtype))

    def read(self, buffer: spy.Buffer, dtype=np.uint32) -> np.ndarray:
        return buffer.to_numpy().view(dtype)

    def constant_module(self, name: str, value) -> spy.SlangModule:
        """Builds the one-line module that drives one permutation constant.

        This is the same text that `MakeExportedConstantSource` in src/PermutationSpace.cpp builds, so
        a test here exercises the mechanism the cooker uses.
        """
        if isinstance(value, bool):
            type_name, literal = "bool", "true" if value else "false"
        elif isinstance(value, int):
            type_name, literal = "uint", f"{value}"
        else:
            raise TypeError(f"axis {name} takes a bool or an int, not {type(value)}")

        source = f"export static const {type_name} {name} = {literal};\n"
        return self.device.load_module_from_source(f"{name}_{literal}", source)

    def kernel(self, entry_point: str, constants: dict | None = None) -> "LinkedKernel":
        return LinkedKernel(self, entry_point, constants or {})


class LinkedKernel:
    """One entry point, linked against a chosen set of permutation constants."""

    def __init__(self, ctx: VtfDevice, entry_point: str, constants: dict) -> None:
        self.ctx = ctx
        self.name = entry_point
        self.constants = dict(constants)

        components = [ctx.module]
        for axis_name, value in self.constants.items():
            components.append(ctx.constant_module(axis_name, value))
        components.append(ctx.entry_points[entry_point])

        link_options = spy.SlangLinkOptions()
        self.program = ctx.device.link_program(components[:-1], [components[-1]], link_options)
        self.pipeline = ctx.device.create_compute_pipeline(program=self.program)

    def dispatch(self, thread_count, resources: dict) -> None:
        """Binds `resources` into the entry point ParameterBlock and runs one dispatch."""
        encoder = self.ctx.device.create_command_encoder()
        with encoder.begin_compute_pass() as compute_pass:
            shader_object = compute_pass.bind_pipeline(self.pipeline)
            cursor = spy.ShaderCursor(shader_object).find_entry_point(0)["resources"]
            for field, value in resources.items():
                # A wrong name raises here rather than reading as zero on the device.
                cursor[field] = value
            compute_pass.dispatch(thread_count)
        self.ctx.device.submit_command_buffer(encoder.finish())
        self.ctx.device.wait()


# ---------------------------------------------------------------------------------------------
# numpy references. Each one mirrors one shader, and the two must agree.
# ---------------------------------------------------------------------------------------------


def reference_morton_cells(positions: np.ndarray, root_min: np.ndarray, root_max: np.ndarray,
                           bits: int = 10) -> np.ndarray:
    """Mirrors QuantizePosition in VtfComputeMortonCodes.

    Every step runs in float32, in the order the shader writes it: subtract, multiply by the
    reciprocal, saturate, scale, truncate. float64 would disagree with the shader for a light that
    lands on a cell boundary, and the disagreement would say nothing about the shader.
    """
    positions = positions.astype(np.float32)
    lower = root_min.astype(np.float32)
    upper = root_max.astype(np.float32)

    extent = np.maximum(upper - lower, np.float32(1.0e-6))
    inverse_range = (np.float32(1.0) / extent).astype(np.float32)
    normalized = np.clip((positions - lower) * inverse_range, np.float32(0.0), np.float32(1.0))
    scale = np.float32((1 << bits) - 1)
    return (normalized * scale).astype(np.uint32)


def morton_from_cells(cells: np.ndarray, bits: int = 10) -> np.ndarray:
    """Interleaves the bits of each axis. Bit i of x lands at 3i, y at 3i + 1, and z at 3i + 2."""
    codes = np.zeros(len(cells), dtype=np.uint32)
    for i in range(bits):
        mask = np.uint32(1 << i)
        shift = 2 * i
        codes |= (cells[:, 0] & mask) << shift
        codes |= (cells[:, 1] & mask) << (shift + 1)
        codes |= (cells[:, 2] & mask) << (shift + 2)
    return codes


def cells_from_morton(codes: np.ndarray, bits: int = 10) -> np.ndarray:
    """Takes the interleave apart again, so a mismatch can be read as a cell and not as a bit soup."""
    cells = np.zeros((len(codes), 3), dtype=np.uint32)
    for i in range(bits):
        shift = 3 * i
        for axis in range(3):
            cells[:, axis] |= ((codes >> np.uint32(shift + axis)) & np.uint32(1)) << np.uint32(i)
    return cells


def reference_morton_codes(positions: np.ndarray, root_min: np.ndarray, root_max: np.ndarray,
                           bits: int = 10) -> np.ndarray:
    return morton_from_cells(reference_morton_cells(positions, root_min, root_max, bits), bits)


def reference_light_aabb(positions: np.ndarray, ranges: np.ndarray):
    """Mirrors VtfReduceLightAABBs. The box that holds every light sphere."""
    lower = (positions - ranges[:, None]).min(axis=0)
    upper = (positions + ranges[:, None]).max(axis=0)
    return lower, upper


def reference_bvh_leaves(positions: np.ndarray, ranges: np.ndarray, order: np.ndarray,
                         branching: int = 32):
    """Mirrors the leaf level of VtfBuildBVH. Each node holds one run of `branching` lights."""
    sorted_pos = positions[order]
    sorted_range = ranges[order]
    node_count = (len(order) + branching - 1) // branching

    lower = np.full((node_count, 3), np.inf, dtype=np.float64)
    upper = np.full((node_count, 3), -np.inf, dtype=np.float64)
    for node in range(node_count):
        lo = node * branching
        hi = min(lo + branching, len(order))
        lower[node] = (sorted_pos[lo:hi] - sorted_range[lo:hi, None]).min(axis=0)
        upper[node] = (sorted_pos[lo:hi] + sorted_range[lo:hi, None]).max(axis=0)
    return lower, upper


def make_lights(count: int, seed: int = 11, spread: float = 40.0):
    """Random point lights in view space, in front of the eye."""
    rng = np.random.default_rng(seed)
    lights = np.zeros(count, dtype=POINT_LIGHT_DTYPE)

    positions = rng.uniform(-spread, spread, size=(count, 3)).astype(np.float32)
    # View space looks down -z, so push every light in front of the eye.
    positions[:, 2] = -rng.uniform(2.0, spread, size=count).astype(np.float32)
    ranges = rng.uniform(1.0, 8.0, size=count).astype(np.float32)

    lights["Position"][:, :3] = positions
    lights["Position"][:, 3] = 1.0
    lights["PositionViewSpace"][:, :3] = positions
    lights["PositionViewSpace"][:, 3] = 1.0
    lights["Color"] = rng.uniform(0.2, 1.0, size=(count, 3)).astype(np.float32)
    lights["Range"] = ranges
    lights["Intensity"] = 1.0
    lights["Enabled"] = 1
    return lights, positions, ranges
