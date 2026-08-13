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

} // namespace velox::cooker
