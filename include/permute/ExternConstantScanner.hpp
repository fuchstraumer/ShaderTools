#pragma once
#ifndef LODESTONE_EXTERN_CONSTANT_SCANNER_HPP
#define LODESTONE_EXTERN_CONSTANT_SCANNER_HPP
#include <string_view>
#include <vector>

/** Reads `extern static const` declarations out of Slang source text.
 *
 * Text matching, and not reflection. A link-time constant never reaches a program layout, so Slang
 * cannot report one. The source text is the only place the declaration exists.
 *
 * The scan is one pass for each line. A declaration must hold the word `extern` */
namespace lodestone
{

/** One `extern ... <Name> = <ValueText>;` line. Both fields point into the source text that the scan
 * read, so the source must outlive the result. `ValueText` is the text between `=` and `;`, with the
 * spaces kept. */
struct ExternConstantDeclaration
{
    std::string_view Name;
    std::string_view ValueText;
};

/** Every declaration in `source`, in the order the lines give them. */
[[nodiscard]] std::vector<ExternConstantDeclaration> ScanExternConstants(std::string_view source);

/** True when one line of `source` declares an extern constant called `name`.
 *
 * The name must be the one the line declares. A name that appears in the default of a different
 * constant is a use, and this returns false for it. A line with no `=` still declares a name, so this
 * finds a declaration that `ScanExternConstants` does not. */
[[nodiscard]] bool DeclaresExternConstantNamed(std::string_view source, std::string_view name);

/** Removes the spaces at each end of `text`. */
[[nodiscard]] std::string_view TrimWhitespace(std::string_view text) noexcept;

} // namespace lodestone

#endif // !LODESTONE_EXTERN_CONSTANT_SCANNER_HPP
