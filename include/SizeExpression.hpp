#pragma once
#ifndef VELOX_SHADER_COOKER_SIZE_EXPRESSION_HPP
#define VELOX_SHADER_COOKER_SIZE_EXPRESSION_HPP
#include "CookerErrors.hpp"
#include <cstdint>
#include <span>
#include <string_view>

/** Evaluates the integer expression carried by a `[vx_element_count(...)]` attribute.
 *
 * The expression travels as a string because Slang will not give us the value any other way. An
 * attribute's integer argument folds at compile time, but the permutation constants are
 * `extern const static` and fold at link time, so `[vx_size(IFFT_SIZE * 4)]` fails to compile. A
 * string argument passes through untouched, and the cooker already holds the axis values, so the
 * cooker does the arithmetic per variant.
 *
 * Nothing here knows about Slang or about the permutation types. It takes a string and a symbol
 * table, and it is therefore testable on its own. */
namespace velox::cooker
{

struct SizeSymbol
{
    std::string_view Name;
    int64_t Value{ 0 };
};

/** Grammar, lowest precedence first:
 *
 *     shift      := sum (( '<<' | '>>' ) sum)*
 *     sum        := product (( '+' | '-' ) product)*
 *     product    := unary (( '*' | '/' | '%' ) unary)*
 *     unary      := '-' unary | primary
 *     primary    := integer | identifier | '(' shift ')'
 *
 * Integers are decimal or `0x` hexadecimal, with an optional `u` or `U` suffix so an expression can
 * be copied out of Slang source unchanged. */
CookResult<int64_t> EvaluateSizeExpression(std::string_view expression,
                                           std::span<const SizeSymbol> symbols);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_SIZE_EXPRESSION_HPP
