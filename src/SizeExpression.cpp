#include "SizeExpression.hpp"
#include <cctype>
#include <charconv>
#include <print>

namespace velox::cooker
{

namespace
{

    bool IsIdentifierStart(char character) noexcept
    {
        return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    bool IsIdentifierCharacter(char character) noexcept
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    /** Recursive descent over a fixed grammar. Every failure returns an error rather than a default,
     * because a size that silently evaluates to zero allocates a zero-byte buffer and fails much
     * later, somewhere unrelated. */
    class ExpressionParser final
    {
    public:
        ExpressionParser(std::string_view expression, std::span<const SizeSymbol> symbols) noexcept :
            text{ expression },
            symbolTable{ symbols },
            cursor{ 0u }
        {
        }

        CookResult<int64_t> ParseComplete()
        {
            const CookResult<int64_t> value = ParseShift();
            if (!value)
            {
                return value;
            }

            SkipWhitespace();
            if (cursor != text.size())
            {
                std::println(stderr,
                             "[shader_cooker] size expression '{}' has trailing text at offset {}",
                             text,
                             cursor);
                return std::unexpected(CookError::SizeExpressionParseFailed);
            }

            return value;
        }

    private:
        void SkipWhitespace() noexcept
        {
            while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
            {
                ++cursor;
            }
        }

        bool ConsumeOperator(std::string_view op) noexcept
        {
            SkipWhitespace();
            if (text.compare(cursor, op.size(), op) != 0)
            {
                return false;
            }

            cursor += op.size();
            return true;
        }

        /** `<` and `>` only ever appear as part of a shift here, so a single character is enough to
         * tell the two shifts apart from anything else. */
        bool PeekIsShift() noexcept
        {
            SkipWhitespace();
            return text.compare(cursor, 2u, "<<") == 0 || text.compare(cursor, 2u, ">>") == 0;
        }

        CookResult<int64_t> ParseShift()
        {
            CookResult<int64_t> left = ParseSum();
            if (!left)
            {
                return left;
            }

            while (PeekIsShift())
            {
                const bool shiftLeft = ConsumeOperator("<<");
                if (!shiftLeft && !ConsumeOperator(">>"))
                {
                    break;
                }

                const CookResult<int64_t> right = ParseSum();
                if (!right)
                {
                    return right;
                }

                if (right.value() < 0 || right.value() >= 64)
                {
                    std::println(stderr,
                                 "[shader_cooker] size expression '{}' shifts by {}, which is out of "
                                 "range",
                                 text,
                                 right.value());
                    return std::unexpected(CookError::SizeExpressionOutOfRange);
                }

                left = shiftLeft ? (left.value() << right.value()) : (left.value() >> right.value());
            }

            return left;
        }

        CookResult<int64_t> ParseSum()
        {
            CookResult<int64_t> left = ParseProduct();
            if (!left)
            {
                return left;
            }

            while (true)
            {
                SkipWhitespace();
                const bool isAdd = ConsumeOperator("+");
                const bool isSubtract = !isAdd && ConsumeOperator("-");
                if (!isAdd && !isSubtract)
                {
                    break;
                }

                const CookResult<int64_t> right = ParseProduct();
                if (!right)
                {
                    return right;
                }

                left = isAdd ? (left.value() + right.value()) : (left.value() - right.value());
            }

            return left;
        }

        CookResult<int64_t> ParseProduct()
        {
            CookResult<int64_t> left = ParseUnary();
            if (!left)
            {
                return left;
            }

            while (true)
            {
                SkipWhitespace();
                const bool isMultiply = ConsumeOperator("*");
                const bool isDivide = !isMultiply && ConsumeOperator("/");
                const bool isModulo = !isMultiply && !isDivide && ConsumeOperator("%");
                if (!isMultiply && !isDivide && !isModulo)
                {
                    break;
                }

                const CookResult<int64_t> right = ParseUnary();
                if (!right)
                {
                    return right;
                }

                if ((isDivide || isModulo) && right.value() == 0)
                {
                    std::println(stderr, "[shader_cooker] size expression '{}' divides by zero", text);
                    return std::unexpected(CookError::SizeExpressionDivideByZero);
                }

                if (isMultiply)
                {
                    left = left.value() * right.value();
                }
                else if (isDivide)
                {
                    left = left.value() / right.value();
                }
                else
                {
                    left = left.value() % right.value();
                }
            }

            return left;
        }

        CookResult<int64_t> ParseUnary()
        {
            SkipWhitespace();
            if (ConsumeOperator("-"))
            {
                const CookResult<int64_t> operand = ParseUnary();
                if (!operand)
                {
                    return operand;
                }

                return -operand.value();
            }

            return ParsePrimary();
        }

        CookResult<int64_t> ParsePrimary()
        {
            SkipWhitespace();
            if (cursor >= text.size())
            {
                std::println(stderr, "[shader_cooker] size expression '{}' ends early", text);
                return std::unexpected(CookError::SizeExpressionParseFailed);
            }

            if (ConsumeOperator("("))
            {
                const CookResult<int64_t> inner = ParseShift();
                if (!inner)
                {
                    return inner;
                }

                if (!ConsumeOperator(")"))
                {
                    std::println(stderr,
                                 "[shader_cooker] size expression '{}' is missing a closing parenthesis",
                                 text);
                    return std::unexpected(CookError::SizeExpressionParseFailed);
                }

                return inner;
            }

            if (std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
            {
                return ParseInteger();
            }

            if (IsIdentifierStart(text[cursor]))
            {
                return ParseIdentifier();
            }

            std::println(stderr,
                         "[shader_cooker] size expression '{}' has an unexpected character '{}' at "
                         "offset {}",
                         text,
                         text[cursor],
                         cursor);
            return std::unexpected(CookError::SizeExpressionParseFailed);
        }

        CookResult<int64_t> ParseInteger()
        {
            int base = 10;
            size_t digitsBegin = cursor;

            if (text.compare(cursor, 2u, "0x") == 0 || text.compare(cursor, 2u, "0X") == 0)
            {
                base = 16;
                digitsBegin = cursor + 2u;
            }

            size_t digitsEnd = digitsBegin;
            while (digitsEnd < text.size() &&
                   std::isalnum(static_cast<unsigned char>(text[digitsEnd])) != 0)
            {
                ++digitsEnd;
            }

            // A trailing `u` or `U` lets an expression be pasted out of Slang source unchanged.
            size_t valueEnd = digitsEnd;
            if (valueEnd > digitsBegin && base == 10 &&
                (text[valueEnd - 1u] == 'u' || text[valueEnd - 1u] == 'U'))
            {
                --valueEnd;
            }

            int64_t value = 0;
            const char* first = text.data() + digitsBegin;
            const char* last = text.data() + valueEnd;
            const std::from_chars_result result = std::from_chars(first, last, value, base);

            if (result.ec != std::errc{} || result.ptr != last || valueEnd == digitsBegin)
            {
                std::println(stderr,
                             "[shader_cooker] size expression '{}' has a malformed integer at offset {}",
                             text,
                             cursor);
                return std::unexpected(CookError::SizeExpressionParseFailed);
            }

            cursor = digitsEnd;
            return value;
        }

        CookResult<int64_t> ParseIdentifier()
        {
            const size_t nameBegin = cursor;
            while (cursor < text.size() && IsIdentifierCharacter(text[cursor]))
            {
                ++cursor;
            }

            const std::string_view name = text.substr(nameBegin, cursor - nameBegin);
            for (const SizeSymbol& symbol : symbolTable)
            {
                if (symbol.Name == name)
                {
                    return symbol.Value;
                }
            }

            std::println(stderr,
                         "[shader_cooker] size expression '{}' names '{}', which is not a permutation "
                         "constant of this module",
                         text,
                         name);
            return std::unexpected(CookError::SizeExpressionUnknownSymbol);
        }

        std::string_view text;
        std::span<const SizeSymbol> symbolTable;
        size_t cursor;
    };

} // namespace

CookResult<int64_t> EvaluateSizeExpression(std::string_view expression,
                                           std::span<const SizeSymbol> symbols)
{
    if (expression.empty())
    {
        std::println(stderr, "[shader_cooker] size expression is empty");
        return std::unexpected(CookError::SizeExpressionParseFailed);
    }

    ExpressionParser parser{ expression, symbols };
    return parser.ParseComplete();
}

} // namespace velox::cooker
