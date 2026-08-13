#pragma once
#ifndef VELOX_SHADER_COOKER_SLANG_COMPILER_HPP
#define VELOX_SHADER_COOKER_SLANG_COMPILER_HPP
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

/** Owns every interaction with Slang. Nothing in this header names a Slang type, so the rest of the
 * cooker links against the data schema rather than against the compiler. */
namespace velox::cooker
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
    CookResult<CompiledVariant> CompileVariant(const VariantDescriptor& descriptor);

    std::string_view GetModuleName() const noexcept;
    std::span<const std::string> GetEntryPointNames() const noexcept;
    /** Every source file the module pulled in, transitively, in Slang's dependency order. */
    std::span<const std::string> GetModuleSourceTexts() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_SLANG_COMPILER_HPP
