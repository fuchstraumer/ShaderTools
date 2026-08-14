#pragma once
#ifndef VELOX_SHADER_COOKER_CONTENT_HASH_HPP
#define VELOX_SHADER_COOKER_CONTENT_HASH_HPP
#include <cstdint>
#include <string_view>

/**
 * The hash the interner uses to find candidates.
 *
 * A hash never decides that two artifacts are equal. The interner compares the bytes on every hash
 * hit. A collision is a normal outcome: the interner keeps both entries, counts the event, and
 * reports it. A weaker hash therefore costs one extra comparison. It cannot ship the wrong shader.
 *
 * That property is why FNV-1a is enough today. It needs ten lines and no dependency, and the cook
 * spends under one millisecond in it. A faster function saves time that the Slang compile makes
 * irrelevant.
 * 
 * todo-ship: We should still replace this with a stronger hash, because while we handle
 * collisions we can avoid them and options are *very* easy to install. xxHash recommended:
 * check mobile performance though
 *
 * Two things would change the answer. A hash that a manifest stores must survive across builds, and
 * a much larger variant count makes distribution matter. Swap the function then: pick a different
 * `ContentHashFunction`, and give it a new `Name`. The name reaches the dedup report, so a stored
 * hash can never be read back by a build that used a different function.
 */
namespace velox::cooker
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

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_CONTENT_HASH_HPP
