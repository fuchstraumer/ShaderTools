#pragma once
#ifndef LODESTONE_SHADER_COOKER_SLANG_COMPILER_HPP
#define LODESTONE_SHADER_COOKER_SLANG_COMPILER_HPP
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "RawLibrary.hpp"
#include "ShaderDataSchema.hpp"
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

/** Owns every interaction with Slang. Nothing in this header names a Slang type, so the rest of the
 * cooker links against the data schema rather than against the compiler. */
namespace lodestone
{

struct SlangCompilerCreateInfo
{
    std::filesystem::path ModulePath;
    std::filesystem::path ModuleCacheDirectory;
    uint32_t OptimizationLevel{ 0u };
    bool MultithreadEntryPointCodegen{ true };
};

class SlangCompiler final
{
public:
    SlangCompiler() noexcept;
    ~SlangCompiler();
    SlangCompiler(const SlangCompiler&) = delete;
    SlangCompiler& operator=(const SlangCompiler&) = delete;
    SlangCompiler(SlangCompiler&&) noexcept;
    SlangCompiler& operator=(SlangCompiler&&) noexcept;

    CookResult<void> Initialize(const SlangCompilerCreateInfo& create_info);
    /** Defaults of the extern constants no axis drives. A size expression may name them, so they must
     * be resolved before the first CompileVariant call. */
    CookResult<void> ResolveExternConstantDefaults(const PermutationSpace& space);

    /** Stage 3. Links, generates the target text, and reads reflection. Every `[vx_*]` argument comes
     * back as the string the author wrote, because evaluating one is stage 4's job. */
    CookResult<RawVariant> CompileVariantRaw(const VariantDescriptor& descriptor);

    /** Stage 4. Evaluates each size expression against the variant's axis values.
     *
     * This is still a member of the compiler, and it must not be. It names no Slang type already, and
     * phase D step D5 moves it into `ResolveStage.cpp` where a test can reach it without a compiler.
     * Until then it lives here so the split can be proved one commit at a time. */
    CookResult<CompiledVariant> ResolveVariant(const RawVariant& raw, const VariantDescriptor& descriptor);

    /** Stage 3 then stage 4, in one call. Phase D step D5 deletes this. */
    CookResult<CompiledVariant> CompileVariant(const VariantDescriptor& descriptor);

    /** Everything stage 4 needs that only Slang can supply. Filled by `ResolveExternConstantDefaults`. */
    std::span<const ExternConstantDefault> GetExternConstantDefaults() const noexcept;

    std::string_view GetModuleName() const noexcept;
    std::span<const std::string> GetEntryPointNames() const noexcept;
    /** Every source file the module pulled in, transitively, in Slang's dependency order. */
    std::span<const std::string> GetModuleSourceTexts() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_SLANG_COMPILER_HPP
