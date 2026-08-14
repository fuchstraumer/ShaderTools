#pragma once
#ifndef LODESTONE_GRAPH_RESOURCE_FLAGS_HPP
#define LODESTONE_GRAPH_RESOURCE_FLAGS_HPP
#include "EnumClassUtils.hpp"
#include <cstdint>

/**
 * @brief Velox's own vocabulary for resource creation parameters, deliberately independent of any
 * WebGPU header. This header is included by a lot of code, and would yank WebGPU and Dawn's headers
 * into every related TU. The conversion table is in ResourceFlagConversions.hpp, which is included
 * only by source files that actually manipulate resources.
 */
namespace lodestone
{

/** @brief How a buffer may be used over its lifetime, declared at creation. Distinct from BufferBinding
 * in ResourceUsage.hpp, which describes what one subpass does with an already-created buffer - this is
 * the wider capability the resource is built with, and Compile() checks the latter against the former.
 */
enum class BufferUsageFlags : uint16_t
{
    Invalid = 0,
    /** @brief Buffer may be mapped for CPU reads. Readback buffers only. Implies slow CPU-visible shared RAM */
    MapRead = 1 << 0,
    /** @brief Buffer may be mapped for CPU writes. */
    MapWrite = 1 << 1,
    /** @brief Buffer may be the source of a copy *command* (used to copy buffers on GPU timeline) */
    CopySource = 1 << 2,
    /** @brief Buffer may be the destination of a copy or a queue write (on GPU timeline) */
    CopyDestination = 1 << 3,
    /** @brief Buffer may be bound as index data for indexed draws. */
    Index = 1 << 4,
    /** @brief Buffer may be bound as vertex data. */
    Vertex = 1 << 5,
    /** @brief Buffer may be bound as a uniform block. This flag is often exclusive. */
    Uniform = 1 << 6,
    /** @brief Buffer may be bound as a storage buffer */
    Storage = 1 << 7,
    /** @brief Buffer may supply indirect draw or dispatch parameters. */
    Indirect = 1 << 8,
    /** @brief Buffer may receive resolved query set results. */
    QueryResolve = 1 << 9,
    // AllCpuAccess and AllGpuTransfer are distinct because WebGPU clearly separates these concepts
    AllCpuAccess = MapRead | MapWrite,
    AllGpuTransfer = CopySource | CopyDestination,
    AllGeometry = Index | Vertex,
    AllShaderBinding = Uniform | Storage
};

MAKE_ENUM_CLASS_FLAGS(BufferUsageFlags);

/**@brief How a texture may be used over its lifetime, declared at creation.
 * @note Sampled and Storage are named for what the shader does rather than for the binding type
 */
enum class TextureUsageFlags : uint16_t
{
    Invalid = 0,
    /** @brief Texture may be the source of a copy command. */
    CopySource = 1 << 0,
    /** @brief Texture may be the destination of a copy or a queue write. */
    CopyDestination = 1 << 1,
    /** @brief Texture may be sampled, with filtering, through a sampler. */
    Sampled = 1 << 2,
    /** @brief Texture may be bound as a storage texture for direct loads and stores.
     * @note This has a restricted format list and is just raw load/store */
    Storage = 1 << 3,
    /** @brief Texture may be bound as a color, depth, or stencil attachment. */
    RenderAttachment = 1 << 4,

    AllTransfer = CopySource | CopyDestination,
    AllShaderBinding = Sampled | Storage
};

MAKE_ENUM_CLASS_FLAGS(TextureUsageFlags);

/** @brief Velox specifically excludes many edge case formats: grow this list as needed. */
enum class TextureFormat : uint8_t
{
    Invalid = 0,

    R8Unorm,
    Rg8Unorm,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Bgra8Unorm,
    Bgra8UnormSrgb,

    R16Float,
    Rg16Float,
    Rgba16Float,

    R32Float,
    Rg32Float,
    Rgba32Float,

    R32Uint,
    Rg32Uint,
    Rgba32Uint,

    Rgb10A2Unorm,
    Rg11B10Ufloat,

    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float
};

/** @brief The dimensionality a texture is *allocated* with. Array textures, for example 
 *  are allocated as 2D textures with multiple array layers, not as 3D textures. */
enum class TextureDimension : uint8_t
{
    Invalid = 0,
    Texture1D,
    Texture2D,
    Texture3D
};

/**@brief How a texture is interpreted by a shader binding. Reflected from the shader, as the CPU
 * should not care about the view dimension. */
enum class TextureViewDimension : uint8_t
{
    Invalid = 0,
    View1D,
    View2D,
    View2DArray,
    ViewCube,
    ViewCubeArray,
    View3D
};

} // namespace lodestone

#endif // !LODESTONE_RESOURCE_FLAGS_HPP
