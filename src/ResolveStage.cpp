#include "ResolveStage.hpp"
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "RawLibrary.hpp"
#include "ShaderDataSchema.hpp"
#include "SizeExpression.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <print>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace lodestone
{

namespace
{

    CookResult<uint32_t> EvaluateExtentArgument(const RawSizeAttribute& attribute,
                                                uint32_t argument_index,
                                                std::string_view binding_name,
                                                const ResolveContext& context)
    {
        if (argument_index >= attribute.Arguments.size())
        {
            std::println(stderr,
                         "[shader_cooker] [{}] on '{}' is missing argument {}",
                         ToString(attribute.Kind),
                         binding_name,
                         argument_index);
            return std::unexpected(CookError::SizeExpressionParseFailed);
        }

        const CookResult<int64_t> value =
            EvaluateSizeExpression(attribute.Arguments[argument_index], context.Symbols);
        if (!value)
        {
            return std::unexpected(value.error());
        }

        if (value.value() <= 0)
        {
            std::println(stderr,
                         "[shader_cooker] extent argument {} on '{}' evaluated to {}, which is not a "
                         "valid texture dimension",
                         argument_index,
                         binding_name,
                         value.value());
            return std::unexpected(CookError::SizeExpressionOutOfRange);
        }

        return static_cast<uint32_t>(value.value());
    }

    CookResult<void> ResolveExtent(const RawSizeAttribute& attribute,
                                   std::string_view binding_name,
                                   const ResolveContext& context,
                                   DerivedSize& derived)
    {
        const uint32_t argumentCount = ArgumentCountOf(attribute.Kind);
        std::array<uint32_t, 3u> axes{ 1u, 1u, 1u };

        for (uint32_t i = 0u; i < argumentCount && i < axes.size(); ++i)
        {
            const CookResult<uint32_t> value = EvaluateExtentArgument(attribute, i, binding_name, context);
            if (!value)
            {
                return std::unexpected(value.error());
            }

            axes.at(i) = value.value();
        }

        derived.ExtentX = axes[0];
        derived.ExtentY = axes[1];
        derived.ExtentZ = axes[2];
        derived.HasExtent = true;
        return {};
    }

    CookResult<void> ResolveElementCount(const RawSizeAttribute& attribute,
                                         std::string_view binding_name,
                                         const ResolveContext& context,
                                         DerivedSize& derived)
    {
        if (attribute.Arguments.empty())
        {
            std::println(stderr,
                         "[shader_cooker] [{}] on '{}' has no argument",
                         ToString(attribute.Kind),
                         binding_name);
            return std::unexpected(CookError::SizeExpressionParseFailed);
        }

        const CookResult<int64_t> value =
            EvaluateSizeExpression(attribute.Arguments.front(), context.Symbols);
        if (!value)
        {
            std::println(stderr,
                         "[shader_cooker] [{}] on '{}' did not evaluate",
                         ToString(attribute.Kind),
                         binding_name);
            return std::unexpected(value.error());
        }

        if (value.value() <= 0)
        {
            std::println(stderr,
                         "[shader_cooker] [{}] on '{}' evaluated to {}, which cannot size a buffer",
                         ToString(attribute.Kind),
                         binding_name,
                         value.value());
            return std::unexpected(CookError::SizeExpressionOutOfRange);
        }

        derived.Expression = attribute.Arguments.front();
        derived.ElementCount = static_cast<uint64_t>(value.value());
        derived.HasElementCount = true;
        return {};
    }

    /** Every annotation on one resource, evaluated. A resource with no annotation is not an error --
     * most resources are sized by the caller -- but a malformed one is, because the alternative is a
     * size that silently defaults to zero. */
    CookResult<DerivedSize> ResolveDerivedSize(std::span<const RawSizeAttribute> attributes,
                                               std::string_view binding_name,
                                               const ResolveContext& context)
    {
        DerivedSize derived;

        const RawSizeAttribute* extent2d = nullptr;
        const RawSizeAttribute* extent3d = nullptr;

        for (const RawSizeAttribute& attribute : attributes)
        {
            if (attribute.Kind == RawSizeAttributeKind::ElementCount)
            {
                if (CookResult<void> counted = ResolveElementCount(attribute, binding_name, context, derived);
                    !counted)
                {
                    return std::unexpected(counted.error());
                }
            }
            else if (attribute.Kind == RawSizeAttributeKind::Extent2d)
            {
                extent2d = &attribute;
            }
            else if (attribute.Kind == RawSizeAttributeKind::Extent3d)
            {
                extent3d = &attribute;
            }
        }

        if (extent2d != nullptr && extent3d != nullptr)
        {
            std::println(stderr,
                         "[shader_cooker] '{}' carries both [{}] and [{}]; only one may size a texture",
                         binding_name,
                         ToString(RawSizeAttributeKind::Extent2d),
                         ToString(RawSizeAttributeKind::Extent3d));
            return std::unexpected(CookError::ReflectionSizeUnresolved);
        }

        const RawSizeAttribute* extent = extent2d != nullptr ? extent2d : extent3d;
        if (extent == nullptr)
        {
            return derived;
        }

        if (CookResult<void> resolved = ResolveExtent(*extent, binding_name, context, derived); !resolved)
        {
            return std::unexpected(resolved.error());
        }

        return derived;
    }

    std::vector<RawSizeAttribute> AttributesOfBinding(const RawVariant& raw, size_t binding_index)
    {
        std::vector<RawSizeAttribute> attributes;

        for (const RawSizeAttribute& attribute : raw.SizeAttributes)
        {
            if (attribute.BindingIndex == binding_index)
            {
                attributes.push_back(attribute);
            }
        }

        return attributes;
    }

    CookResult<ReflectedBinding> ResolveBinding(const RawVariant& raw,
                                                size_t binding_index,
                                                const ResolveContext& context)
    {
        const RawBinding& rawBinding = raw.GlobalBindings[binding_index];

        ReflectedBinding binding;
        binding.Name = rawBinding.Name;
        binding.Kind = rawBinding.Kind;
        binding.ElementStride = rawBinding.ElementStride;
        binding.ByteSize = rawBinding.ByteSize;
        binding.ArrayCount = rawBinding.ArrayCount;
        binding.Shape = rawBinding.Shape;
        binding.SampleType = rawBinding.SampleType;
        binding.StorageFormat = rawBinding.StorageFormat;
        binding.StorageAccess = rawBinding.StorageAccess;
        binding.SamplerType = rawBinding.SamplerType;
        binding.UniformMembers = rawBinding.UniformMembers;

        if (const BoundPlacement* placement = GetBoundPlacement(rawBinding.Placement))
        {
            binding.Group = placement->Group;
            binding.Binding = placement->Binding;
        }

        CookResult<DerivedSize> derived =
            ResolveDerivedSize(AttributesOfBinding(raw, binding_index), binding.Name, context);
        if (!derived)
        {
            return std::unexpected(derived.error());
        }

        binding.Derived = std::move(derived.value());
        return binding;
    }

    CompiledEntryPoint ResolveEntryPoint(const RawEntryPoint& raw_entry_point)
    {
        CompiledEntryPoint entryPoint;
        entryPoint.Name = raw_entry_point.Name;
        entryPoint.VariantSuffix = raw_entry_point.VariantSuffix;
        entryPoint.Code = raw_entry_point.TargetText;
        entryPoint.Reflection.Name = raw_entry_point.Name;
        entryPoint.Reflection.Stage = raw_entry_point.Stage;
        entryPoint.Reflection.Workgroup = raw_entry_point.Workgroup;
        entryPoint.Reflection.UsedBindingIndices = raw_entry_point.UsedBindingIndices;
        entryPoint.Reflection.Raster = raw_entry_point.Raster;
        return entryPoint;
    }

} // namespace

ResolveContext MakeResolveContext(const PermutationAssignment& canonical,
                                  std::span<const ExternConstantDefault> extern_defaults)
{
    ResolveContext context;
    context.Symbols.reserve(canonical.size() + extern_defaults.size());

    for (const ExternConstantDefault& entry : extern_defaults)
    {
        context.Symbols.push_back(SizeSymbol{ .Name = entry.Name, .Value = entry.Value });
    }

    for (const PermutationBinding& binding : canonical)
    {
        context.Symbols.push_back(
            SizeSymbol{ .Name = binding.first->Name, .Value = PermutationValueToInt64(binding.second) });
    }

    return context;
}

CookResult<CompiledVariant> ResolveVariant(const RawVariant& raw, const ResolveContext& context)
{
    CompiledVariant variant;
    variant.VariantSuffix = raw.VariantSuffix;
    variant.VariantDescription = raw.VariantDescription;
    variant.VariantIndex = raw.VariantIndex;
    variant.GlobalBindings.reserve(raw.GlobalBindings.size());

    for (size_t i = 0u; i < raw.GlobalBindings.size(); ++i)
    {
        CookResult<ReflectedBinding> binding = ResolveBinding(raw, i, context);
        if (!binding)
        {
            return std::unexpected(binding.error());
        }

        variant.GlobalBindings.push_back(std::move(binding.value()));
    }

    variant.EntryPoints.reserve(raw.EntryPoints.size());
    for (const RawEntryPoint& rawEntryPoint : raw.EntryPoints)
    {
        variant.EntryPoints.push_back(ResolveEntryPoint(rawEntryPoint));
    }

    return variant;
}

} // namespace lodestone
