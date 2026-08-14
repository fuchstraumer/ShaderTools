#include "ShaderDataSchema.hpp"
#include <algorithm>
#include <format>

namespace velox::cooker
{

std::string_view ToString(BindingKind kind) noexcept
{
    switch (kind)
    {
    case BindingKind::UniformBuffer:
        return "UniformBuffer";
    case BindingKind::StorageBuffer:
        return "StorageBuffer";
    case BindingKind::ReadOnlyStorageBuffer:
        return "ReadOnlyStorageBuffer";
    case BindingKind::SampledTexture:
        return "SampledTexture";
    case BindingKind::StorageTexture:
        return "StorageTexture";
    case BindingKind::Sampler:
        return "Sampler";
    case BindingKind::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

std::string_view ToString(ResourceShape shape) noexcept
{
    switch (shape)
    {
    case ResourceShape::Buffer:
        return "Buffer";
    case ResourceShape::Texture1D:
        return "Texture1D";
    case ResourceShape::Texture2D:
        return "Texture2D";
    case ResourceShape::Texture2DArray:
        return "Texture2DArray";
    case ResourceShape::Texture3D:
        return "Texture3D";
    case ResourceShape::TextureCube:
        return "TextureCube";
    case ResourceShape::TextureCubeArray:
        return "TextureCubeArray";
    case ResourceShape::Texture2DMultisample:
        return "Texture2DMultisample";
    case ResourceShape::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

std::string_view ToString(TextureSampleType sample_type) noexcept
{
    switch (sample_type)
    {
    case TextureSampleType::Float:
        return "Float";
    case TextureSampleType::UnfilterableFloat:
        return "UnfilterableFloat";
    case TextureSampleType::Depth:
        return "Depth";
    case TextureSampleType::SignedInteger:
        return "SignedInteger";
    case TextureSampleType::UnsignedInteger:
        return "UnsignedInteger";
    case TextureSampleType::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

std::string_view ToString(ShaderStageKind stage) noexcept
{
    switch (stage)
    {
    case ShaderStageKind::Vertex:
        return "Vertex";
    case ShaderStageKind::Fragment:
        return "Fragment";
    case ShaderStageKind::Compute:
        return "Compute";
    case ShaderStageKind::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

std::string_view ToString(VertexScalarType scalar_type) noexcept
{
    switch (scalar_type)
    {
    case VertexScalarType::Float16:
        return "f16";
    case VertexScalarType::Float32:
        return "f32";
    case VertexScalarType::SignedInteger32:
        return "i32";
    case VertexScalarType::UnsignedInteger32:
        return "u32";
    case VertexScalarType::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

std::string DescribeRasterState(const ReflectedRasterState& raster)
{
    std::string description;

    for (const ReflectedVertexInput& input : raster.VertexInputs)
    {
        description += std::format("      @location({}) {}{} : {}x{}\n",
                                   input.Location,
                                   input.SemanticName,
                                   input.SemanticIndex,
                                   ToString(input.ScalarType),
                                   input.ComponentCount);
    }

    for (const ReflectedColorTarget& target : raster.ColorTargets)
    {
        description += std::format("      target {} : {}x{} (format stays with the caller)\n",
                                   target.Location,
                                   ToString(target.ScalarType),
                                   target.ComponentCount);
    }

    if (raster.WritesFragDepth)
    {
        description += "      writes SV_Depth\n";
    }

    return description;
}

bool SameBindingLocation(const ReflectedBinding& lhs, const ReflectedBinding& rhs) noexcept
{
    return lhs.Group == rhs.Group && lhs.Binding == rhs.Binding;
}

void SortBindingsByLocation(std::span<ReflectedBinding> bindings) noexcept
{
    std::ranges::sort(bindings,
                      [](const ReflectedBinding& lhs, const ReflectedBinding& rhs)
                      {
                          if (lhs.Group != rhs.Group)
                          {
                              return lhs.Group < rhs.Group;
                          }
                          return lhs.Binding < rhs.Binding;
                      });
}

std::string DescribeBinding(const ReflectedBinding& binding)
{
    std::string description = std::format("@group({}) @binding({}) {} : {}",
                                          binding.Group,
                                          binding.Binding,
                                          binding.Name,
                                          ToString(binding.Kind));

    if (binding.Shape != ResourceShape::Invalid)
    {
        description += std::format(" {}", ToString(binding.Shape));
    }

    if (binding.ElementStride != 0u)
    {
        description += std::format(" stride={}", binding.ElementStride);
    }

    if (binding.ByteSize != 0u)
    {
        description += std::format(" bytes={}", binding.ByteSize);
    }

    if (binding.SampleType != TextureSampleType::Invalid)
    {
        description += std::format(" sample={}", ToString(binding.SampleType));
    }

    if (binding.ArrayCount > 1u)
    {
        description += std::format(" array={}", binding.ArrayCount);
    }

    if (binding.Derived.HasElementCount)
    {
        description += std::format(" count={} [{}]",
                                   binding.Derived.ElementCount,
                                   binding.Derived.Expression);
    }

    if (binding.Derived.HasExtent)
    {
        description += std::format(" extent={}x{}x{}",
                                   binding.Derived.ExtentX,
                                   binding.Derived.ExtentY,
                                   binding.Derived.ExtentZ);
    }

    return description;
}

std::string DescribeUniformMembers(const ReflectedBinding& binding)
{
    std::string description;

    for (const ReflectedUniformMember& member : binding.UniformMembers)
    {
        description += std::format("[shader_cooker]       +{} {} ({} bytes{})\n",
                                   member.Offset,
                                   member.Name,
                                   member.Size,
                                   member.ArrayCount > 1u ? std::format(", array={}", member.ArrayCount)
                                                          : std::string{});
    }

    return description;
}

} // namespace velox::cooker
