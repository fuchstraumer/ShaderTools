#pragma once
#ifndef LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
#define LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

struct XXH3_state_s;

/**
 * The hash the interner uses to find candidates.
 *
 * A hash never decides that two artifacts are equal. The interner compares the bytes on every hash
 * hit. A collision is a normal outcome: the interner keeps both entries, counts the event, and
 * reports it. A weaker hash therefore costs one extra comparison. It cannot ship the wrong shader.
 *
 * The name reaches the output, so a new hash needs a new name. Take it from `k_HashName` and never
 * spell it out again: the two spellings drifted apart once.
 *
 * todo-ship: measure this on mobile
 */
namespace lodestone
{

using ContentHashValue = uint64_t;
inline constexpr std::string_view k_HashName{ "xxHash3_64" };
ContentHashValue HashBytes(std::span<const std::byte> values) noexcept;

/**@brief xxHash3 has a "streaming" hashing API that you can open with `XXH3_createState` and update
 * incrementally, so we can use RAII to hide the fiddly bits and make it intuitive for users: create a
 * streaming hash object, add your values to it in a {} block, and then finalize to close it and get your
 * result (freeing memory while at it)
 */
struct StreamingHash
{
    StreamingHash();
    ~StreamingHash();
    // no rule of 5, create this locally and use it inline or don't use it
    StreamingHash(const StreamingHash&) = delete;
    StreamingHash& operator=(const StreamingHash&) = delete;
    StreamingHash(StreamingHash&&) = delete;
    StreamingHash& operator=(StreamingHash&&) = delete;
    // this would've been easier with templates, but that's silly for this use case
    // and means header bloat. booooo
    void Append(std::span<const std::byte> values) noexcept;
    void Append(std::string_view bytes) noexcept;
    void Append(uint64_t value) noexcept;
    void Append(int64_t value) noexcept;
    void Append(uint32_t value) noexcept;
    void Append(int32_t value) noexcept;
    void Append(std::span<const uint64_t> values) noexcept;
    void Append(std::span<const int64_t> values) noexcept;
    void Append(std::span<const uint32_t> values) noexcept;
    void Append(std::span<const int32_t> values) noexcept;
    // resets the internal state without realloc: this is just some memsets()
    // works great with our preserved thread-local state bc we avoid aligned alloc cost
    void Reset() noexcept;
    ContentHashValue Finalize() const noexcept;
private:
    XXH3_state_s* hashState{ nullptr };
};

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_CONTENT_HASH_HPP
