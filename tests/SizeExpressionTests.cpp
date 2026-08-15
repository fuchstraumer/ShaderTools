#include "SizeExpression.hpp"
#include "TestHarness.hpp"
#include <array>

// The cooker evaluates `[vx_element_count("...")]` itself, because Slang folds attribute integer
// arguments at compile time and the permutation constants only fold at link time. That makes this
// parser the one place where a shader's declared size can drift from the buffer the graph creates,
// so it is worth more test surface than its size suggests.

using velox::cooker::CookError;
using velox::cooker::EvaluateSizeExpression;
using velox::cooker::SizeSymbol;

namespace
{

constexpr std::array<SizeSymbol, 4> k_Symbols{ SizeSymbol{ "IFFT_SIZE", 512 },
                                               SizeSymbol{ "IFFT_NUM_WAVE_CASCADES", 4 },
                                               SizeSymbol{ "IFFT_WAVE_SIZE", 32 },
                                               SizeSymbol{ "IFFT_USE_WAVE_OPS", 1 } };

void CheckValue(velox::tests::TestRunner& runner,
                std::string_view expression,
                int64_t expected,
                std::string_view description)
{
    const auto result = EvaluateSizeExpression(expression, k_Symbols);
    runner.Check(result.has_value() && result.value() == expected, description);
}

void CheckError(velox::tests::TestRunner& runner,
                std::string_view expression,
                CookError expected,
                std::string_view description)
{
    const auto result = EvaluateSizeExpression(expression, k_Symbols);
    runner.Check(!result.has_value() && result.error() == expected, description);
}

} // namespace

int main()
{
    velox::tests::TestRunner runner{ "SizeExpressionTests" };

    runner.BeginSection("literals");
    CheckValue(runner, "1", 1, "single digit");
    CheckValue(runner, "4096", 4096, "multi digit");
    CheckValue(runner, "512u", 512, "unsigned suffix, as Slang source writes it");
    CheckValue(runner, "0x100", 256, "hexadecimal");
    CheckValue(runner, "  64  ", 64, "surrounding whitespace");

    runner.BeginSection("symbols");
    CheckValue(runner, "IFFT_SIZE", 512, "bare symbol");
    CheckValue(runner, "IFFT_SIZE * IFFT_SIZE", 262144, "the square case the ocean demo needs");
    CheckValue(runner,
               "IFFT_SIZE * IFFT_SIZE * IFFT_NUM_WAVE_CASCADES",
               1048576,
               "cascaded texture element count");

    runner.BeginSection("precedence");
    CheckValue(runner, "2 + 3 * 4", 14, "multiply binds tighter than add");
    CheckValue(runner, "(2 + 3) * 4", 20, "parentheses override");
    CheckValue(runner, "16 / 4 / 2", 2, "divide is left associative");
    CheckValue(runner, "10 - 3 - 2", 5, "subtract is left associative");
    CheckValue(runner, "1 << 4", 16, "shift left");
    CheckValue(runner, "1024 >> 2", 256, "shift right");
    CheckValue(runner, "1 << 2 + 1", 8, "shift is lower precedence than add");
    CheckValue(runner, "17 % 5", 2, "modulo");
    CheckValue(runner, "-4 + 10", 6, "leading negation");
    CheckValue(runner, "IFFT_SIZE / IFFT_WAVE_SIZE", 16, "workgroup count style expression");

    runner.BeginSection("rejections");
    CheckError(runner, "", CookError::SizeExpressionParseFailed, "empty expression");
    CheckError(runner, "IFFT_SIZ", CookError::SizeExpressionUnknownSymbol, "typo in a symbol name");
    CheckError(runner, "FFT_SIZE", CookError::SizeExpressionUnknownSymbol, "the historical typo");
    CheckError(runner, "4 / 0", CookError::SizeExpressionDivideByZero, "divide by zero");
    CheckError(runner, "4 % 0", CookError::SizeExpressionDivideByZero, "modulo by zero");
    CheckError(runner, "1 << 99", CookError::SizeExpressionOutOfRange, "shift past the word");
    CheckError(runner, "(2 + 3", CookError::SizeExpressionParseFailed, "unclosed parenthesis");
    CheckError(runner, "2 +", CookError::SizeExpressionParseFailed, "dangling operator");
    CheckError(runner, "2 3", CookError::SizeExpressionParseFailed, "trailing text");
    CheckError(runner, "$", CookError::SizeExpressionParseFailed, "unexpected character");

    return runner.Report();
}
