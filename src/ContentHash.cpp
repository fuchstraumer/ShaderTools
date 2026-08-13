#include "ContentHash.hpp"

namespace velox::cooker
{

namespace
{

    constexpr ContentHashValue k_Fnv1aOffsetBasis = 14695981039346656037ull;
    constexpr ContentHashValue k_Fnv1aPrime = 1099511628211ull;

} // namespace

ContentHashValue HashFnv1a64(std::string_view bytes) noexcept
{
    ContentHashValue hash = k_Fnv1aOffsetBasis;

    for (char character : bytes)
    {
        hash ^= static_cast<ContentHashValue>(static_cast<unsigned char>(character));
        hash *= k_Fnv1aPrime;
    }

    return hash;
}

ContentHashValue CombineHash(ContentHashValue seed, uint64_t value) noexcept
{
    for (uint32_t byteIndex = 0u; byteIndex < 8u; ++byteIndex)
    {
        seed ^= (value >> (byteIndex * 8u)) & 0xFFull;
        seed *= k_Fnv1aPrime;
    }

    return seed;
}

ContentHashFunction DefaultContentHashFunction() noexcept
{
    return ContentHashFunction{ "fnv1a-64", &HashFnv1a64 };
}

} // namespace velox::cooker
