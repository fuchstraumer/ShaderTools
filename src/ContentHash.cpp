#include "ContentHash.hpp"

#include <cstdint>
#include <string_view>
#include <xxhash.h>

namespace lodestone
{

namespace
{

    constexpr ContentHashValue k_Fnv1aOffsetBasis = 14695981039346656037ull;
    constexpr ContentHashValue k_Fnv1aPrime = 1099511628211ull;

} // namespace

constexpr bool k_UseXXHash3 = true;

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

ContentHashValue HashXXHash3(std::string_view bytes) noexcept
{
    return XXH3_64bits(bytes.data(), bytes.size());
}

ContentHashValue CombineHash(ContentHashValue seed, uint64_t value) noexcept
{
    if (!k_UseXXHash3)
    {
        for (uint32_t byteIndex = 0u; byteIndex < 8u; ++byteIndex)
        {
            seed ^= (value >> (byteIndex * 8u)) & 0xFFull;
            seed *= k_Fnv1aPrime;
        }

        return seed;
    }
    else
    {
        return XXH3_64bits_withSeed(&value, sizeof(value), seed);
    }

}

ContentHashFunction DefaultContentHashFunction() noexcept
{
    return ContentHashFunction{ "xxhash3_64", &HashXXHash3 };
}

} // namespace lodestone
