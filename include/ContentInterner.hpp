#pragma once
#ifndef VELOX_SHADER_COOKER_CONTENT_INTERNER_HPP
#define VELOX_SHADER_COOKER_CONTENT_INTERNER_HPP
#include "ContentHash.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Collapses equal artifacts onto one entry, and records who mapped where. The name Interner
 * comes from the compiler world, where it is used to collapse equal strings onto one copy. This
 * is just a generalized interner of sorts, a kind of "hash table with provenance". 
 * 
 * and to be clear, provenance (literally) is the record of where something came from... so the
 * provenance of a shader variant is the set of entry points and parameters that produced it.
 *
 * Three rules control this class:
 * 
 * 1. Hashes only find buckets. A hash does not identify the data. 
 *    If hashes match, the class compares the bytes. If bytes are different, 
 *    the class keeps both items and records the collision.
 * 
 * 2. The class records all sources. A unique item keeps a list of its 
 *    original inputs. You can always trace an output back to its inputs.
 * 
 * 3. You can stop deduplication. The `Disable()` function gives every item 
 *    its own index. The build output must be correct and identical in both modes.
 */
namespace velox::cooker
{

/** @brief One artifact that mapped onto a unique entry. */
struct ProvenanceRecord
{
    std::string EntryPointName;
    std::string VariantDescription;
    uint32_t VariantIndex{ 0u };
};

/** @brief The result - a lookup outcome - of interning */
struct InternResult
{
    uint32_t Index{ 0u };
    bool WasNew{ false };
};

/**@brief Useful tracking metrics to monitor interning performance, and surface changes */
struct InternerStatistics
{
    uint32_t ArtifactsSeen{ 0u };
    uint32_t UniqueEntries{ 0u };
    /** Equal hash, unequal bytes. The byte comparison caught it and kept both. */
    uint32_t HashCollisions{ 0u };
    /** Byte comparisons that a hash hit forced. A rising count means a worse hash, not a bug. */
    uint32_t ByteComparisons{ 0u };
};

/**@brief Note that this is templated on the payload type: that affects how equality and hashing can be performed,
 * and is another way we give ourselves flexibilty with output formats. This could be changed to be SPIR-V, or GLSL,
 * or DXIL, or any other format we want to support. The interner doesn't care, it just needs to be able to hash and compare
 * the payload type. */
template<typename PayloadType>
class ContentInterner final
{
public:
    using HashFunction = ContentHashValue (*)(const PayloadType&) noexcept;

    ContentInterner(HashFunction hash_function, std::string_view hash_name) noexcept :
        hashFunction{ hash_function },
        hashName{ hash_name },
        dedupeEnabled{ true }
    {
    }

    /** Turns off collapsing. Every artifact then gets its own index, and the tables stay correct. */
    void Disable() noexcept
    {
        dedupeEnabled = false;
    }

    [[nodiscard]] bool IsEnabled() const noexcept
    {
        return dedupeEnabled;
    }

    /** @brief take that payload and shove it somewhere else (intern. get it) */
    InternResult Intern(PayloadType payload, ProvenanceRecord origin)
    {
        ++statistics.ArtifactsSeen;

        if (!dedupeEnabled)
        {
            return Append(std::forward<PayloadType>(payload), std::forward<ProvenanceRecord>(origin));
        }

        const ContentHashValue hash = hashFunction(payload);
        // use [] here *because* we want to create the bucket
        std::vector<uint32_t>& bucket = buckets[hash];

        for (uint32_t candidate : bucket)
        {
            ++statistics.ByteComparisons;
            if (uniqueEntries[candidate] == payload)
            {
                origins[candidate].push_back(std::move(origin));
                return InternResult{ candidate, false };
            }

            // Same bucket, different bytes. Both entries stay. Nothing here guesses.
            ++statistics.HashCollisions;
        }

        const InternResult appended = Append(std::move(payload), std::move(origin));
        bucket.push_back(appended.Index);
        return appended;
    }

    [[nodiscard]] std::span<const PayloadType> UniqueEntries() const noexcept
    {
        return uniqueEntries;
    }

    [[nodiscard]] std::span<const ProvenanceRecord> OriginsOf(uint32_t index) const noexcept
    {
        if (index >= origins.size())
        {
            return {};
        }

        return origins[index];
    }

    [[nodiscard]] const InternerStatistics& Statistics() const noexcept
    {
        return statistics;
    }

    [[nodiscard]] std::string_view HashName() const noexcept
    {
        return hashName;
    }

private:
    InternResult Append(PayloadType payload, ProvenanceRecord origin)
    {
        const uint32_t index = static_cast<uint32_t>(uniqueEntries.size());
        uniqueEntries.push_back(std::move(payload));
        origins.push_back(std::vector<ProvenanceRecord>{ std::move(origin) });
        statistics.UniqueEntries = static_cast<uint32_t>(uniqueEntries.size());
        return InternResult{ index, true };
    }

    HashFunction hashFunction;
    std::string_view hashName;
    bool dedupeEnabled;
    std::unordered_map<ContentHashValue, std::vector<uint32_t>> buckets;
    std::vector<PayloadType> uniqueEntries;
    std::vector<std::vector<ProvenanceRecord>> origins;
    InternerStatistics statistics;
};

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_CONTENT_INTERNER_HPP
