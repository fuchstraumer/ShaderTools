#pragma once
#ifndef LODESTONE_SHADER_COOKER_SLANG_COMPILER_HPP
#define LODESTONE_SHADER_COOKER_SLANG_COMPILER_HPP
#include "CookerErrors.hpp"
#include "Diagnostics.hpp"
#include "permute/PermutationSpace.hpp"
#include "RawLibrary.hpp"
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

    /** The sink takes every compiler message, and it must outlive this object. It is a parameter
     * rather than a field of the create info because a sink cannot be absent: a compiler that has
     * nowhere to report is a compiler that fails in silence. */
    CookResult<void> Initialize(const SlangCompilerCreateInfo& create_info, DiagnosticSink& sink);
    /** Stage 3, once for each module. Returns the module facts stage 4 needs and only Slang can
     * supply, the defaults of the extern constants no axis drives among them. A size expression may
     * name one, so call this before the first `CompileVariantRaw`. */
    CookResult<RawModule> PrepareRawModule(const PermutationSpace& space);

    /** Stage 3, once for each variant. Links, generates the target text, and reads reflection. Every
     * `[vx_*]` argument comes back as the string the author wrote, because evaluating one is stage
     * 4's job. */
    CookResult<RawVariant> CompileVariantRaw(const VariantDescriptor& descriptor);

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
