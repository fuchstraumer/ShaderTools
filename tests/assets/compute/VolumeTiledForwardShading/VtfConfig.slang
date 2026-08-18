implementing VolumeTiledForwardShading;

// Anything marked with `extern` is treated as a link-time constant by Slang, and is something
// our parser will actively search for. anything left as `static const` is effectively just 
// a module-level global constant. We still have those here though, because it centralizes 
// the configuration and makes it easier to manage or drive with permutations eventually.

// ---------------------------------------------------------------------------------------------
// Capability params that vary by device first
// ---------------------------------------------------------------------------------------------

// permutation parameter - expected range: false, true
extern static const bool VTF_USE_WAVE_OPS = true;

// permutation parameter - expected range: 16, 32, 64, 128
// this just the largest width that the variant is cooked for, since the upper bound influences
// some things that must be uniform constant across invocations. in most places, we use `WaveGetLaneCount()`
// to find the active width, since that's more robust and we refactored our code to adapt to that better
// This is the largest wave width that the variant is cooked for. It is not a request to the hardware.
//
// VtfAssignLightsToClustersBVH uses a wave ballot mask, and that's returned as a uint4 because
// it's a bitmask... with one bit per lane. In the default cause, we only ever need mask.x...
// but we caught ourselves once with this assumption, so now we jsut don't make it
extern static const uint VTF_WAVE_SIZE = 32u;

// not a permutation parameter - just the absolute narrowest size we can find reported for a device
// i think even on wgsl the default is still 32, so we may be able to clean this out or bump it up
internal static const uint VTF_MIN_WAVE_SIZE = 16u;

// ---------------------------------------------------------------------------------------------
// Data structure. This is not an axis. It is a property of CPU data structures, so it wasn't
// supposed to vary. Now with our level of reflection, it could, but we prob won't bother
// ---------------------------------------------------------------------------------------------

// okay, little backstory on this because it helps explain why 32 is so common as a shaping parameter:
// when the original author of volume tiled forward shading implemented it in 2017, they were mostly
// thinking about minimizing bank conflicts - conflicts in memory amongst the "warps" of threads reading
// LDS together in compute shaders. it can be a huge chokepoint, limiting perf if not handled carefully.
// so they built around that, before wave ops were available anywhere but CUDA (iirc). now, we have more
// flexibility and also pretty high support levels for wave ops, so while 32 is a fine default it's best
// if we don't screw over AMD (for example) which can have 64-lane waves. 
// unfortunately, the BVH is built as a 32-ary tree, so we can't just increase the wave width to match the
// hardware without changing the tree structure. for now we leave it, but because the wave size is expected
// to vary we split this constant (VTF_BVH_BRANCHING) away from the wave size. for now, it's not extern
// or driven as an axis, but declaring it here makes it much easier to drive it should we decide to someday
internal static const uint VTF_BVH_BRANCHING = 32u;
internal static const uint VTF_BVH_MAX_LEVELS = 7u;

// ---------------------------------------------------------------------------------------------
// Stage selectors. Each used to be a [SpecializationConstant] that a switch statement reads, 
// used to choose a stage of an algorithm (with uniform branching, or link-time specialization
// using Vulkan's facilities for that). Now we just do it at link (variant compile) time w Slang
// ---------------------------------------------------------------------------------------------

// permutation parameter - expected range: 0, 1
// 0 reduces from the light list. 1 reduces from the partial AABBs that pass 0 wrote.
extern static const uint VTF_REDUCTION_TYPE = 0u;
// permutation parameter - expected range: 0, 1
// 0 builds the leaf level. 1 builds an interior level.
extern static const uint VTF_BUILD_STAGE = 0u;
// permutation parameter - expected range: 0, 1
// 0 computes the merge path partitions. 1 performs the merge
extern static const uint VTF_SORT_PASS = 0u;

// ---------------------------------------------------------------------------------------------
// Thread counts. Each one sizes a groupshared array, so each one must be bound at cook time
// It might be fun to experiment with how different thread counts affect performance and memory usage.
// For now, this is mostly a torture test of the variant system lol
// ---------------------------------------------------------------------------------------------

// permutation parameter - expected range: 128, 256, 512
// 256 threads give two float3 arrays of 4096 bytes each. note that 512 threads give 16384 bytes,
// which is the whole WebGPU workgroup budget minspec requirement and leaves nothing for anything else.
extern static const uint VTF_REDUCE_LIGHTS_NUM_THREADS = 256u;

// permutation parameter - expected range: 128, 256
// The value must be a multiple of VTF_BVH_BRANCHING, because each node reduces one branching group.
// The old default was 512, which gives two float3 arrays of 8192 bytes each and fills the whole
// WebGPU workgroup budget. 256 threads build 8 nodes for each group and use 8192 bytes in total.
extern static const uint VTF_BUILD_BVH_NUM_THREADS = 256u;

// permutation parameter - expected range: 32, 64, 128
// One group processes one cluster. The value must be a multiple of VTF_BVH_BRANCHING, because each
// thread visits one child of the current node.
extern static const uint VTF_ASSIGN_LIGHTS_NUM_THREADS = 32u;

// permutation parameter - expected range: 128, 256
extern static const uint VTF_MERGE_SORT_NUM_THREADS = 256u;

// permutation parameter - expected range: 2, 4, 8
// The product of this value and VTF_MERGE_SORT_NUM_THREADS gives the element count of two groupshared
// arrays. Keep that product at or below 2048, or the arrays exceed the 16384-byte WebGPU limit.
extern static const uint VTF_MERGE_VALUES_PER_THREAD = 4u;

// permutation parameter - expected range: 128, 256, 512
// This value is also the chunk size that one radix sort pass covers.
extern static const uint VTF_RADIX_SORT_NUM_THREADS = 256u;

// permutation parameter - expected range: 256, 512, 1024
extern static const uint VTF_MORTON_NUM_THREADS = 1024u;

// permutation parameter - expected range: 256, 512, 1024
extern static const uint VTF_SMALL_ENTRYPOINT_NUM_THREADS = 1024u;

// ---------------------------------------------------------------------------------------------
// Parameters intended for tuning and expected to be used for tuning
// ---------------------------------------------------------------------------------------------

// permutation parameter - expected range: 256, 512, 1024
// The per-cluster light list holds this many entries in groupshared memory, once for point lights and
// once for spot lights. 1024 gives 8192 bytes, and the node stack adds 4096 more. 2048 gives 20480
// bytes in total, which WebGPU rejects.
extern static const uint VTF_MAX_LIGHTS_PER_CLUSTER = 1024u;

// permutation parameter - expected range: 256, 512, 1024
extern static const uint VTF_NODE_STACK_LIMIT = 1024u;

// permutation parameter - expected range: 10, 16, 21
// Bits for each axis of the Morton code. 10 bits give a 30-bit code, which agrees with VTF_RADIX_BITS
// of 30.
extern static const uint VTF_MORTON_BITS = 10u;

// permutation parameter - expected range: 30, 32
// The radix sort tests this many bits. 30 agrees with a 10-bit Morton code and removes two passes.
extern static const uint VTF_RADIX_BITS = 30u;

// permutation parameter - expected range: false, true
// true also writes the indirect draw arguments for the cluster debug pass. This replaces a uniform
// bool, which WebGPU cannot hold in a uniform buffer.
extern static const bool VTF_UPDATE_UNIQUE_CLUSTERS = true;

// permutation parameter - expected range: false, true
// true draws point light proxies, false draws spot light proxies. The GLSL held this as one
// specialization constant declared once in the vertex stage and again in the fragment stage, and the
// author had to keep the two equal by hand. One constant on one module cannot drift.
extern static const bool VTF_DEBUG_POINT_LIGHTS = true;

// not a permutation parameter - the cluster debug pass draws one box for each unique cluster as a line
// list. A box has 12 edges and each edge takes 2 vertices.
internal const static uint VTF_DEBUG_CLUSTER_VERTS = 24u;

// ---------------------------------------------------------------------------------------------
// Capacities. A size expression names these, so the cooker can give each buffer an element count.
// Note that this algorithm on desktop is capable of absolutely ridiculous amounts of lights, but
// for general use even this is probably overkill (if you have 8000 point lights, I hate you)
// ---------------------------------------------------------------------------------------------

extern static const uint VTF_MAX_POINT_LIGHTS = 8192u;
extern static const uint VTF_MAX_SPOT_LIGHTS = 4096u;
extern static const uint VTF_MAX_DIRECTIONAL_LIGHTS = 4u;

// permutation parameter - expected range: 16, 24, 32
extern static const uint VTF_CLUSTER_GRID_X = 24u;
// permutation parameter - expected range: 8, 16, 24
extern static const uint VTF_CLUSTER_GRID_Y = 16u;
// permutation parameter - expected range: 16, 24, 32
extern static const uint VTF_CLUSTER_GRID_Z = 24u;

// ---------------------------------------------------------------------------------------------
// Values that follow from the declarations above. None of these are an axis.
// ---------------------------------------------------------------------------------------------

internal const static uint VTF_CLUSTER_COUNT = VTF_CLUSTER_GRID_X * VTF_CLUSTER_GRID_Y * VTF_CLUSTER_GRID_Z;
// The wave path stores one partial result for each wave. The narrowest wave gives the largest wave
// count, so these extents hold at every width.
internal const static uint VTF_RADIX_SORT_MAX_WAVES =
    (VTF_RADIX_SORT_NUM_THREADS + VTF_MIN_WAVE_SIZE - 1u) / VTF_MIN_WAVE_SIZE;
// Number of 32-bit lanes that one countbits call covers, and the number of calls the ballot needs.
internal const static uint VTF_BALLOT_WORD_COUNT = (VTF_WAVE_SIZE + 31u) / 32u;
internal const static uint VTF_MERGE_VALUES_PER_GROUP =
    VTF_MERGE_SORT_NUM_THREADS * VTF_MERGE_VALUES_PER_THREAD;
// The largest quantized Morton coordinate, for one axis.
internal const static uint VTF_MORTON_COORD_SCALE = (1u << VTF_MORTON_BITS) - 1u;
internal const static float VTF_FLT_MAX = 3.402823466e+38f;
