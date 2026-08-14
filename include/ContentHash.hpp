#pragma once
#ifndef LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
#define LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
#include <cstdint>
#include <string_view>

/**
 * The hash the interner uses to find candidates.
 *
 * A hash never decides that two artifacts are equal. The interner compares the bytes on every hash
 * hit. A collision is a normal outcome: the interner keeps both entries, counts the event, and
 * reports it. A weaker hash therefore costs one extra comparison. It cannot ship the wrong shader.
 * todo-ship: We should still replace this with a stronger hash, because while we handle
 * collisions we can avoid them and options are *very* easy to install. xxHash recommended:
 * check mobile performance though
 *
 * If changing hash functions, make sure it has a new name as the name reaches the output
 */
namespace lodestone
{

using ContentHashValue = uint64_t;

/**@brief A hash function, plus the name that identifies it in the report and in any stored output.
 * @note The name should be unique, as it is the only way to identify the function in a stored manifest. */
struct ContentHashFunction
{
    std::string_view Name;
    ContentHashValue (*Hash)(std::string_view bytes) noexcept;
};

ContentHashValue HashFnv1a64(std::string_view bytes) noexcept;

/**@brief Mixes one more value into a running hash. A layout hashes field by field, so it needs this. */
ContentHashValue CombineHash(ContentHashValue seed, uint64_t value) noexcept;

ContentHashFunction DefaultContentHashFunction() noexcept;

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
