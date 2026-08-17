#include "TargetProfile.hpp"
#include "ShaderDataSchema.hpp"
#include "WgslBindingScanner.hpp"

#include <array>
#include <span>
#include <string_view>
#include <vector>

namespace lodestone
{

namespace
{

    /**@brief The WGSL second opinion. First uses `ScanWgslBindings` to extract the bindings declared in the
     * WGSL source text, returned in `declared`. CompareBindings then uses the reflected binding information
     * to validate they match on the data values they both store (which isn't everything, to be clear) */
    class WgslReflectionValidator final : public ResolvedLibraryValidator
    {
    public:
        [[nodiscard]] BindingComparison ValidateEntryPoint(
            std::string_view target_text, std::span<const ReflectedBinding> used) const override
        {
            const std::vector<WgslDeclaredBinding> declared = ScanWgslBindings(target_text);
            return CompareBindings(declared, used);
        }
    };

    const WgslReflectionValidator k_WgslValidator;

    constexpr std::string_view k_WgslName = "wgsl";

    const std::array<TargetProfile, 1u> k_TargetProfiles{ TargetProfile{
        .Name = k_WgslName, .Access = AccessModel::Bound, .Validator = &k_WgslValidator } };

    constexpr std::array<std::string_view, 1u> k_TargetProfileNames{ k_WgslName };

} // namespace

std::string_view ToString(AccessModel model) noexcept
{
    switch (model)
    {
    case AccessModel::Bound:
        return "bound";
    case AccessModel::Indexed:
        return "indexed";
    case AccessModel::Pointer:
        return "pointer";
    case AccessModel::Invalid:
        [[fallthrough]];
    default:
        return "invalid";
    }
}

const TargetProfile* FindTargetProfile(std::string_view name) noexcept
{
    for (const TargetProfile& profile : k_TargetProfiles)
    {
        if (profile.Name == name)
        {
            return &profile;
        }
    }

    return nullptr;
}

std::span<const std::string_view> GetTargetProfileNames() noexcept
{
    return k_TargetProfileNames;
}

} // namespace lodestone
