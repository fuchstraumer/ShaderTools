#include "RawLibrary.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace lodestone
{

const BoundPlacement* GetBoundPlacement(const RawPlacement& placement) noexcept
{
    return std::get_if<BoundPlacement>(&placement);
}

std::string_view ToString(RawSizeAttributeKind kind) noexcept
{
    switch (kind)
    {
    case RawSizeAttributeKind::ElementCount:
        return "vx_element_count";
    case RawSizeAttributeKind::Extent2d:
        return "vx_extent_2d";
    case RawSizeAttributeKind::Extent3d:
        return "vx_extent_3d";
    case RawSizeAttributeKind::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

uint32_t ArgumentCountOf(RawSizeAttributeKind kind) noexcept
{
    switch (kind)
    {
    case RawSizeAttributeKind::ElementCount:
        return 1u;
    case RawSizeAttributeKind::Extent2d:
        return 2u;
    case RawSizeAttributeKind::Extent3d:
        return 3u;
    case RawSizeAttributeKind::Invalid:
        return 0u;
    }

    return 0u;
}

bool RawPlacementLess(const RawPlacement& lhs, const RawPlacement& rhs) noexcept
{
    const BoundPlacement* leftPlacement = GetBoundPlacement(lhs);
    const BoundPlacement* rightPlacement = GetBoundPlacement(rhs);

    if (leftPlacement == nullptr || rightPlacement == nullptr)
    {
        return leftPlacement != nullptr;
    }

    if (leftPlacement->Group != rightPlacement->Group)
    {
        return leftPlacement->Group < rightPlacement->Group;
    }

    return leftPlacement->Binding < rightPlacement->Binding;
}

} // namespace lodestone
