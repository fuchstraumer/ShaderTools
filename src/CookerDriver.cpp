#include "CookerDriver.hpp"
#include "CookedLibrary.hpp"
#include "CookerErrors.hpp"
#include "CookerOptions.hpp"
#include "DedupeReport.hpp"
#include "OutputSink.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryEmitter.hpp"
#include "ShaderManifestEmitter.hpp"
#include "SlangCompiler.hpp"
#include "WgslBindingScanner.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>


namespace lodestone
{

namespace
{

    std::expected<std::filesystem::path, std::error_code> EnsureModuleCacheDirectory(
        const std::filesystem::path& cache_directory)
    {
        std::error_code filesystemError;

        if (!std::filesystem::exists(cache_directory, filesystemError))
        {
            std::filesystem::create_directories(cache_directory, filesystemError);
        }

        // transform to canonical before returning (and since we know it exists), since it can be a little
        // more robust
        std::filesystem::path canonicalCacheDirectory =
            std::filesystem::canonical(cache_directory, filesystemError);

        if (filesystemError)
        {
            return std::unexpected(filesystemError);
        }

        return canonicalCacheDirectory;
    }

    void ReportEntryPointReflection(const CompiledEntryPoint& entry_point)
    {
        std::println(stderr,
                     "[shader_cooker]   {}{} [{}] workgroup {}x{}x{}",
                     entry_point.Name,
                     entry_point.VariantSuffix,
                     ToString(entry_point.Reflection.Stage),
                     entry_point.Reflection.Workgroup.X,
                     entry_point.Reflection.Workgroup.Y,
                     entry_point.Reflection.Workgroup.Z);

        for (const ReflectedBinding& binding : entry_point.Reflection.Bindings)
        {
            std::println(stderr,
                         "[shader_cooker]     {} usedBy=0x{:x}",
                         DescribeBinding(binding),
                         binding.EntryPointUsageMask);

            const std::string members = DescribeUniformMembers(binding);
            if (!members.empty())
            {
                std::print(stderr, "{}", members);
            }
        }

        const std::string raster = DescribeRasterState(entry_point.Reflection.Raster);
        if (!raster.empty())
        {
            std::print(stderr, "{}", raster);
        }
    }

    /** Slang emits only the bindings an entry point actually references, so the WGSL for one entry
     * point is compared against the subset of program-scope bindings that entry point uses. */
    std::vector<ReflectedBinding> SelectBindingsUsedByEntryPoint(const EntryPointReflection& reflection)
    {
        std::vector<ReflectedBinding> used;
        used.reserve(reflection.Bindings.size());

        for (const ReflectedBinding& binding : reflection.Bindings)
        {
            if (binding.EntryPointUsageMask != 0u)
            {
                used.push_back(binding);
            }
        }

        return used;
    }

    void ReportUnreferencedBindings(const CompiledVariant& variant)
    {
        if (variant.EntryPoints.empty())
        {
            return;
        }

        // Every entry point holds the same set of bindings, so we can just check the first one.
        // What varies is the EntryPointUsageMask, which is set for each binding the entry point actually
        // uses. We will use the first bindings list to find the bindings, but check to see if any point
        // actually uses it
        const std::vector<ReflectedBinding>& bindings = variant.EntryPoints.front().Reflection.Bindings;
        const size_t declaredBindingCount = bindings.size();
        for (size_t bindingIndex = 0u; bindingIndex < declaredBindingCount; ++bindingIndex)
        {
            const bool referenced = std::ranges::any_of(
                variant.EntryPoints,
                [bindingIndex](const CompiledEntryPoint& entry_point)
                {
                    return entry_point.Reflection.Bindings[bindingIndex].EntryPointUsageMask != 0u;
                });
            if (!referenced)
            {
                std::println(
                    stderr,
                    "[shader_cooker] unreferenced binding in [{}]: {} is declared but no entrypoint reads it",
                    variant.VariantDescription,
                    DescribeBinding(bindings[bindingIndex]));
            }
        }
    }

    uint32_t ValidateVariantReflection(const CompiledVariant& variant)
    {
        uint32_t mismatchCount = 0u;

        for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
        {
            const std::vector<WgslDeclaredBinding> declared = ScanWgslBindings(entryPoint.Code);
            const std::vector<ReflectedBinding> used = SelectBindingsUsedByEntryPoint(entryPoint.Reflection);
            const BindingComparison comparison = CompareBindings(declared, used);

            if (!comparison.Matches)
            {
                ++mismatchCount;
                std::println(stderr,
                             "[shader_cooker] REFLECTION MISMATCH in {}{} ({}):\n{}",
                             entryPoint.Name,
                             entryPoint.VariantSuffix,
                             variant.VariantDescription,
                             comparison.Report);
            }
        }

        return mismatchCount;
    }

    /** Replays every variant through the finished tables and compares the result against the text the
     * compiler produced. This is the one check that makes a wrong shader impossible to ship: an index
     * mistake, a table hole, or a bad collapse all show up here, and all of them fail the cook. */
    CookResult<void> VerifyLibraryRoundTrip(const CookedModule& module,
                                            std::span<const CompiledVariant> compiled)
    {
        if (module.Variants.size() != compiled.size())
        {
            std::println(stderr,
                         "[shader_cooker] module {} holds {} variants but the cook produced {}",
                         module.Name,
                         module.Variants.size(),
                         compiled.size());
            return std::unexpected(CookError::LibraryRoundTripFailed);
        }

        uint32_t mismatches = 0u;
        for (const LibraryVariant& variant : module.Variants)
        {
            const CompiledVariant* origin = nullptr;
            for (const CompiledVariant& candidate : compiled)
            {
                if (candidate.VariantIndex == variant.Index)
                {
                    origin = &candidate;
                    break;
                }
            }

            if (origin == nullptr)
            {
                std::println(stderr,
                             "[shader_cooker] variant index {} is in the library but not in the cook",
                             variant.Index);
                ++mismatches;
                continue;
            }

            for (size_t i = 0u; i < origin->EntryPoints.size(); ++i)
            {
                if (ResolveSource(module, variant, i) != origin->EntryPoints[i].Code)
                {
                    std::println(stderr,
                                 "[shader_cooker] ROUND TRIP FAILED for {} [{}]: the table returns "
                                 "different text than the compiler produced",
                                 origin->EntryPoints[i].Name,
                                 variant.Description);
                    ++mismatches;
                }
            }
        }

        if (mismatches != 0u)
        {
            return std::unexpected(CookError::LibraryRoundTripFailed);
        }

        return {};
    }

    CookResult<void> EmitLibraryModules(std::string_view header_stem,
                                        std::string_view header_name,
                                        const std::vector<CookedModule>& modules,
                                        OutputSink& sink,
                                        CookStatistics& statistics)
    {
        for (const CookedModule& module : modules)
        {
            const std::string sourceName = MakeModuleSourceFileName(header_stem, module.Name);
            const std::string source = EmitShaderLibraryModuleSource(module, header_name);

            if (CookResult<void> sourceResult = sink.WriteArtifact(sourceName, source); !sourceResult)
            {
                return sourceResult;
            }

            statistics.GeneratedSourceBytes += source.size();
            std::println(stderr,
                         "[shader_cooker] wrote {} ({} unique sources, {} layouts, {} KiB)",
                         sourceName,
                         module.Sources.size(),
                         module.Layouts.size(),
                         source.size() / 1024u);

            const std::string manifest = EmitShaderManifest(module);

            if (CookResult<void> manifestCheck = VerifyManifestRoundTrip(module, manifest); !manifestCheck)
            {
                return manifestCheck;
            }

            if (CookResult<void> manifestResult =
                    sink.WriteArtifact(MakeManifestFileName(module.Name), manifest);
                !manifestResult)
            {
                return manifestResult;
            }
        }

        return {};
    }

    /** Writes the header and one source file for each module. The header name comes from the sink, so
     * the generated source includes exactly the file the user asked for.
     * todo: For writing files, we can accumulate output we want to write into a buffer, and only validate
     * things once. Validate directory when opening the stream, validate write success of coalesced writes
     * (cleans up control flow)*/
    CookResult<void> EmitLibraryArtifacts(const CookedLibrary& library,
                                          OutputSink& sink,
                                          CookStatistics& statistics)
    {
        const std::string headerName{ sink.PrimaryName() };
        const std::filesystem::path headerPath{ headerName };
        const std::string headerStem = headerPath.stem().string();

        const std::string header = EmitShaderLibraryHeader(library);
        if (auto headerResult = sink.Write(header); !headerResult)
        {
            return headerResult;
        }

        if (auto emitLibraryResult =
                EmitLibraryModules(headerStem, headerName, library.Modules, sink, statistics);
            !emitLibraryResult)
        {
            return emitLibraryResult;
        }

        const std::string report = GenerateDedupeReport(library);
        if (auto reportResult = sink.WriteArtifact("ShaderLibrary.dedupe.txt", report); !reportResult)
        {
            return reportResult;
        }

        return {};
    }

    CookResult<void> CookModule(const CookerOptions& options,
                                const std::filesystem::path& module_path,
                                std::vector<CompiledVariant>& out_variants,
                                CookedLibrary& out_library,
                                CookStatistics& statistics)
    {
        SlangCompilerCreateInfo createInfo;
        createInfo.ModulePath = module_path;
        createInfo.ModuleCacheDirectory = options.ModuleCacheDirectory;
        createInfo.OptimizationLevel = options.OptimizationLevel;
        createInfo.MultithreadEntryPointCodegen = options.MultithreadEntryPointCodegen;

        SlangCompiler compiler;
        if (auto initializeResult = compiler.Initialize(createInfo); !initializeResult)
        {
            return initializeResult;
        }

        const std::string_view moduleName = compiler.GetModuleName();
        const std::span<const std::string> entryPointNames = compiler.GetEntryPointNames();
        std::println(
            stderr, "[shader_cooker] module {} declares {} entrypoints", moduleName, entryPointNames.size());

        const PermutationSpace* space = FindPermutationSpaceForModule(moduleName);

        const CookResult<void> axisResult =
            VerifyAxisNamesAreDeclared(*space, compiler.GetModuleSourceTexts(), moduleName);
        if (!axisResult)
        {
            return std::unexpected(axisResult.error());
        }

        ReportUndrivenExternConstants(*space, compiler.GetModuleSourceTexts(), moduleName);

        const CookResult<void> defaultsResult = compiler.ResolveExternConstantDefaults(*space);
        if (!defaultsResult)
        {
            return std::unexpected(defaultsResult.error());
        }

        const CookResult<VariantSet> variantSet = EnumerateVariants(*space);
        if (!variantSet)
        {
            return std::unexpected(variantSet.error());
        }

        std::println(stderr,
                     "[shader_cooker] module {} expands to {} variants over an index space of {}",
                     moduleName,
                     variantSet.value().Variants.size(),
                     variantSet.value().SpaceSize);

        CookedModule cookedModule;
        if (!options.DedupeEnabled)
        {
            cookedModule.SourceInterner.Disable();
            cookedModule.LayoutInterner.Disable();
        }
        cookedModule.Name = moduleName;
        cookedModule.Space = space;
        cookedModule.SpaceSize = variantSet.value().SpaceSize;

        std::vector<CompiledVariant> moduleVariants;
        moduleVariants.reserve(variantSet.value().Variants.size());

        for (const VariantDescriptor& descriptor : variantSet.value().Variants)
        {
            CookResult<CompiledVariant> variantResult = compiler.CompileVariant(descriptor);
            if (!variantResult)
            {
                std::println(stderr,
                             "[shader_cooker] variant [{}] failed: {}",
                             DescribeAssignment(descriptor.Canonical),
                             ToString(variantResult.error()));
                return std::unexpected(variantResult.error());
            }

            CompiledVariant& variant = variantResult.value();
            ++statistics.VariantsCompiled;
            statistics.EntryPointsCompiled += static_cast<uint32_t>(variant.EntryPoints.size());

            for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
            {
                statistics.TotalWgslBytes += entryPoint.Code.size();
            }

            if (options.ReportReflection)
            {
                std::println(stderr, "[shader_cooker] variant [{}]", variant.VariantDescription);
                for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
                {
                    ReportEntryPointReflection(entryPoint);
                }
            }

            if (options.ValidateReflectionAgainstWgsl)
            {
                statistics.ReflectionMismatches += ValidateVariantReflection(variant);
            }

            if (options.ReportReflection)
            {
                ReportUnreferencedBindings(variant);
            }

            if (cookedModule.EntryPoints.empty())
            {
                cookedModule.EntryPoints.reserve(variant.EntryPoints.size());
                for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
                {
                    cookedModule.EntryPoints.push_back(
                        LibraryEntryPoint{ entryPoint.Name, entryPoint.Reflection.Stage });
                }
            }

            const CookResult<void> appendResult =
                AppendVariantToModule(cookedModule, variant, descriptor.Canonical);
            if (!appendResult)
            {
                return std::unexpected(appendResult.error());
            }

            moduleVariants.push_back(variant);
            out_variants.push_back(std::move(variant));
        }

        FreezeModuleTables(cookedModule);

        const CookResult<void> roundTripResult = VerifyLibraryRoundTrip(cookedModule, moduleVariants);
        if (!roundTripResult)
        {
            return std::unexpected(roundTripResult.error());
        }

        std::println(stderr,
                     "[shader_cooker] module {} round trip verified: {} variants resolve to the text "
                     "the compiler produced",
                     moduleName,
                     cookedModule.Variants.size());

        const ModuleInfluence influence = ComputeAxisInfluence(cookedModule);
        const CookResult<void> policyResult = EnforceModulePolicy(cookedModule, influence);
        if (!policyResult)
        {
            return std::unexpected(policyResult.error());
        }

        out_library.Modules.push_back(std::move(cookedModule));
        ++statistics.ModulesCooked;
        return {};
    }

} // namespace

CookResult<CookStatistics> RunCookOnce(const CookerOptions& options, OutputSink& sink)
{
    const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    std::expected<std::filesystem::path, std::error_code> cacheDirectoryResult =
        EnsureModuleCacheDirectory(options.ModuleCacheDirectory);

    if (!cacheDirectoryResult)
    {
        return std::unexpected(CookError::FilesystemError);
    }

    CookStatistics statistics;
    std::vector<CompiledVariant> variants;
    CookedLibrary library;

    for (const std::filesystem::path& modulePath : options.ModulePaths)
    {
        std::println(stderr, "[shader_cooker] cooking {}", modulePath.string());
        const CookResult<void> moduleResult = CookModule(options, modulePath, variants, library, statistics);
        if (!moduleResult)
        {
            return std::unexpected(moduleResult.error());
        }
    }

    if (statistics.ReflectionMismatches != 0u)
    {
        return std::unexpected(CookError::ReflectionMismatch);
    }

    const CookResult<void> emitResult = EmitLibraryArtifacts(library, sink, statistics);
    if (!emitResult)
    {
        return std::unexpected(emitResult.error());
    }

    const std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
    statistics.ElapsedMilliseconds = elapsed.count();

    return statistics;
}

namespace
{

    /** Cooks twice into memory and compares every artifact. Enumeration order is sorted and the
     * interner numbers entries in first-encounter order, so two cooks of one input must agree byte
     * for byte. A difference means an unordered container's iteration order reached the output, which
     * otherwise shows up months later as a rebuild that changes nothing. */
    CookResult<CookStatistics> RunCookTwiceAndCompare(const CookerOptions& options, OutputSink& sink)
    {
        std::println(stderr, "[shader_cooker] determinism check: cooking twice into memory");

        MemoryOutputSink first;
        const CookResult<CookStatistics> firstResult = RunCookOnce(options, first);
        if (!firstResult)
        {
            return firstResult;
        }

        MemoryOutputSink second;
        const CookResult<CookStatistics> secondResult = RunCookOnce(options, second);
        if (!secondResult)
        {
            return secondResult;
        }

        if (first.GetContent() != second.GetContent())
        {
            std::println(stderr, "[shader_cooker] DETERMINISM FAILED: the header differs between cooks");
            return std::unexpected(CookError::CookNotDeterministic);
        }

        if (first.GetArtifacts().size() != second.GetArtifacts().size())
        {
            std::println(stderr,
                         "[shader_cooker] DETERMINISM FAILED: {} artifacts, then {}",
                         first.GetArtifacts().size(),
                         second.GetArtifacts().size());
            return std::unexpected(CookError::CookNotDeterministic);
        }

        for (const auto& [name, content] : first.GetArtifacts())
        {
            const auto other = second.GetArtifacts().find(name);
            if (other == second.GetArtifacts().end() || other->second != content)
            {
                std::println(stderr, "[shader_cooker] DETERMINISM FAILED: {} differs between cooks", name);
                return std::unexpected(CookError::CookNotDeterministic);
            }
        }

        std::println(stderr,
                     "[shader_cooker] determinism verified: {} artifacts identical across two cooks",
                     first.GetArtifacts().size() + 1u);

        const CookResult<void> writeResult = sink.Write(first.GetContent());
        if (!writeResult)
        {
            return std::unexpected(writeResult.error());
        }

        for (const auto& [name, content] : first.GetArtifacts())
        {
            const CookResult<void> artifactResult = sink.WriteArtifact(name, content);
            if (!artifactResult)
            {
                return std::unexpected(artifactResult.error());
            }
        }

        return secondResult;
    }

} // namespace

CookResult<CookStatistics> RunCook(const CookerOptions& options, OutputSink& sink)
{
    if (options.VerifyDeterministic)
    {
        return RunCookTwiceAndCompare(options, sink);
    }

    return RunCookOnce(options, sink);
}

} // namespace lodestone
