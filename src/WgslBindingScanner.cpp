#include "WgslBindingScanner.hpp"
#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

namespace velox::cooker
{

namespace
{

    constexpr std::string_view k_GroupAttribute = "group";
    constexpr std::string_view k_BindingAttribute = "binding";
    constexpr std::string_view k_VarKeyword = "var";

    bool IsIdentifierCharacter(char character) noexcept
    {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_';
    }

    bool IsWhitespace(char character) noexcept
    {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n';
    }

    size_t SkipWhitespace(std::string_view text, size_t offset) noexcept
    {
        while (offset < text.size() && IsWhitespace(text[offset]))
        {
            ++offset;
        }

        return offset;
    }

    size_t ReadIdentifier(std::string_view text, size_t offset, std::string_view& out_identifier) noexcept
    {
        const size_t start = offset;
        while (offset < text.size() && IsIdentifierCharacter(text[offset]))
        {
            ++offset;
        }

        out_identifier = text.substr(start, offset - start);
        return offset;
    }

    size_t ReadParenthesizedUint(std::string_view text, size_t offset, std::optional<uint32_t>& out_value)
    {
        out_value.reset();
        offset = SkipWhitespace(text, offset);
        if (offset >= text.size() || text[offset] != '(')
        {
            return offset;
        }

        ++offset;
        offset = SkipWhitespace(text, offset);

        const size_t start = offset;
        while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
        {
            ++offset;
        }

        uint32_t parsed = 0u;
        const std::from_chars_result result =
            std::from_chars(text.data() + start, text.data() + offset, parsed);
        if (result.ec == std::errc{})
        {
            out_value = parsed;
        }

        offset = SkipWhitespace(text, offset);
        if (offset < text.size() && text[offset] == ')')
        {
            ++offset;
        }

        return offset;
    }

    size_t SkipTemplateArguments(std::string_view text, size_t offset) noexcept
    {
        offset = SkipWhitespace(text, offset);
        if (offset >= text.size() || text[offset] != '<')
        {
            return offset;
        }

        int32_t depth = 0;
        while (offset < text.size())
        {
            if (text[offset] == '<')
            {
                ++depth;
            }
            else if (text[offset] == '>')
            {
                --depth;
                if (depth == 0)
                {
                    return offset + 1u;
                }
            }
            ++offset;
        }

        return offset;
    }

} // namespace

std::vector<WgslDeclaredBinding> ScanWgslBindings(std::string_view wgsl)
{
    std::vector<WgslDeclaredBinding> declared;
    declared.reserve(16u);

    size_t offset = 0u;
    std::optional<uint32_t> pendingGroup;
    std::optional<uint32_t> pendingBinding;

    while (offset < wgsl.size())
    {
        if (wgsl[offset] == '@')
        {
            std::string_view attributeName;
            offset = ReadIdentifier(wgsl, offset + 1u, attributeName);

            if (attributeName == k_GroupAttribute)
            {
                offset = ReadParenthesizedUint(wgsl, offset, pendingGroup);
            }
            else if (attributeName == k_BindingAttribute)
            {
                offset = ReadParenthesizedUint(wgsl, offset, pendingBinding);
            }

            continue;
        }

        if (!IsIdentifierCharacter(wgsl[offset]))
        {
            ++offset;
            continue;
        }

        std::string_view identifier;
        const size_t afterIdentifier = ReadIdentifier(wgsl, offset, identifier);

        if (identifier == k_VarKeyword && pendingGroup.has_value() && pendingBinding.has_value())
        {
            const size_t afterTemplate = SkipTemplateArguments(wgsl, afterIdentifier);
            const size_t nameStart = SkipWhitespace(wgsl, afterTemplate);

            std::string_view variableName;
            offset = ReadIdentifier(wgsl, nameStart, variableName);

            if (!variableName.empty())
            {
                declared.emplace_back(WgslDeclaredBinding{ std::string{ variableName },
                                                           pendingGroup.value(),
                                                           pendingBinding.value() });
            }

            pendingGroup.reset();
            pendingBinding.reset();
            continue;
        }

        offset = afterIdentifier;
    }

    return declared;
}

std::string_view StripSlangNameMangling(std::string_view mangled_name) noexcept
{
    size_t end = mangled_name.size();
    while (end > 0u && mangled_name[end - 1u] >= '0' && mangled_name[end - 1u] <= '9')
    {
        --end;
    }

    if (end > 0u && end < mangled_name.size() && mangled_name[end - 1u] == '_')
    {
        return mangled_name.substr(0u, end - 1u);
    }

    return mangled_name;
}

BindingComparison CompareBindings(std::span<const WgslDeclaredBinding> declared,
                                  std::span<const ReflectedBinding> reflected)
{
    BindingComparison comparison;
    comparison.Matches = true;

    for (const WgslDeclaredBinding& declaredBinding : declared)
    {
        const std::string_view declaredName = StripSlangNameMangling(declaredBinding.Name);

        const ReflectedBinding* match = nullptr;
        for (const ReflectedBinding& reflectedBinding : reflected)
        {
            if (reflectedBinding.Group == declaredBinding.Group &&
                reflectedBinding.Binding == declaredBinding.Binding)
            {
                match = &reflectedBinding;
                break;
            }
        }

        if (match == nullptr)
        {
            comparison.Matches = false;
            comparison.Report += std::format("  wgsl declares @group({}) @binding({}) {} : reflection has "
                                             "no binding at that location\n",
                                             declaredBinding.Group,
                                             declaredBinding.Binding,
                                             declaredName);
            continue;
        }

        if (match->Name != declaredName)
        {
            comparison.Matches = false;
            comparison.Report += std::format("  @group({}) @binding({}) name mismatch: wgsl '{}' vs "
                                             "reflection '{}'\n",
                                             declaredBinding.Group,
                                             declaredBinding.Binding,
                                             declaredName,
                                             match->Name);
        }
    }

    for (const ReflectedBinding& reflectedBinding : reflected)
    {
        const std::span<const WgslDeclaredBinding>::iterator found =
            std::ranges::find_if(declared,
                                 [&reflectedBinding](const WgslDeclaredBinding& candidate)
                                 {
                                     return candidate.Group == reflectedBinding.Group &&
                                            candidate.Binding == reflectedBinding.Binding;
                                 });

        if (found == declared.end())
        {
            comparison.Matches = false;
            comparison.Report += std::format("  reflection reports {} : no such binding in emitted wgsl\n",
                                             DescribeBinding(reflectedBinding));
        }
    }

    return comparison;
}

} // namespace velox::cooker
