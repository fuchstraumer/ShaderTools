#include "CookerDriver.hpp"
#include "CookedLibrary.hpp"
#include "CookerErrors.hpp"
#include "CookerOptions.hpp"
#include "DedupeReport.hpp"
#include "OutputSink.hpp"
#include "PermutationSpace.hpp"
#include "ResolveStage.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryEmitter.hpp"
#include "ShaderManifestEmitter.hpp"
#include "SlangCompiler.hpp"
#include "StageDump.hpp"
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

    void ReportEntryPointReflection(const CompiledVariant& variant, size_t entry_point_index)
    {
        const CompiledEntryPoint& entryPoint = variant.EntryPoints[entry_point_index];
        std::println(stderr,
                     "[shader_cooker]   {}{} [{}] workgroup {}x{}x{}",
                     entryPoint.Name,
                     entryPoint.VariantSuffix,
                     ToString(entryPoint.Reflection.Stage),
                     entryPoint.Reflection.Workgroup.X,
                     entryPoint.Reflection.Workgroup.Y,
                     entryPoint.Reflection.Workgroup.Z);

        for (const ReflectedBinding& binding : BuildEntryPointLayout(variant, entry_point_index))
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

        const std::string raster = DescribeRasterState(entryPoint.Reflection.Raster);
        if (!raster.empty())
        {
            std::print(stderr, "{}", raster);
        }
    }

    /** Slang emits only the bindings an entry point actually references, so the WGSL for one entry
     * point is compared against the subset of program-scope bindings that entry point uses. */
    std::vector<ReflectedBinding> SelectBindingsUsedByEntryPoint(const CompiledVariant& variant,
                                                                 size_t entry_point_index)
    {
        const std::vector<uint32_t>& usedIndices =
            variant.EntryPoints[entry_point_index].Reflection.UsedBindingIndices;

        std::vector<ReflectedBinding> used;
        used.reserve(usedIndices.size());

        for (const uint32_t bindingIndex : usedIndices)
        {
            used.push_back(variant.GlobalBindings[bindingIndex]);
        }

        return used;
    }

    bool AnyEntryPointReadsBinding(const CompiledVariant& variant, uint32_t binding_index) noexcept
    {
        return std::ranges::any_of(variant.EntryPoints,
                                   [binding_index](const CompiledEntryPoint& entry_point)
                                   {
                                       return std::ranges::contains(entry_point.Reflection.UsedBindingIndices,
                                                                    binding_index);
                                   });
    }

    void ReportUnreferencedBindings(const CompiledVariant& variant)
    {
        for (size_t bindingIndex = 0u; bindingIndex < variant.GlobalBindings.size(); ++bindingIndex)
        {
            if (AnyEntryPointReadsBinding(variant, static_cast<uint32_t>(bindingIndex)))
            {
                continue;
            }

            std::println(
                stderr,
                "[shader_cooker] unreferenced binding in [{}]: {} is declared but no entrypoint reads it",
                variant.VariantDescription,
                DescribeBinding(variant.GlobalBindings[bindingIndex]));
        }
    }

    uint32_t ValidateVariantReflection(const CompiledVariant& variant)
    {
        uint32_t mismatchCount = 0u;

        for (size_t i = 0u; i < variant.EntryPoints.size(); ++i)
        {
            const CompiledEntryPoint& entryPoint = variant.EntryPoints[i];
            const std::vector<WgslDeclaredBinding> declared = ScanWgslBindings(entryPoint.Code);
            const std::vector<ReflectedBinding> used = SelectBindingsUsedByEntryPoint(variant, i);
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

    /** Builds the dump only when the flag asked for it, because a dump of a large module costs real
     * work. The dump then goes out through the sink, so the determinism check compares it against
     * the second cook exactly as it compares every other artifact. */
    template<typename BuildDumpFn>
    CookResult<void> WriteStageDumpIfRequested(const CookerOptions& options,
                                               OutputSink& sink,
                                               std::string_view module_name,
                                               StageDumpKind kind,
                                               BuildDumpFn build_dump)
    {
        if (!IsStageDumpRequested(options, kind))
        {
            return {};
        }

        return sink.WriteArtifact(MakeStageDumpFileName(module_name, kind), build_dump());
    }

    /** Builds the compiler for one module, and checks everything that must hold before the first
     * variant compiles. */
    CookResult<void> PrepareModuleCompiler(const CookerOptions& options,
                                           const std::filesystem::path& module_path,
                                           SlangCompiler& compiler,
                                           const PermutationSpace*& out_space)
    {
        SlangCompilerCreateInfo createInfo;
        createInfo.ModulePath = module_path;
        createInfo.ModuleCacheDirectory = options.ModuleCacheDirectory;
        createInfo.OptimizationLevel = options.OptimizationLevel;
        createInfo.MultithreadEntryPointCodegen = options.MultithreadEntryPointCodegen;

        if (auto initializeResult = compiler.Initialize(createInfo); !initializeResult)
        {
            return initializeResult;
        }

        const std::string_view moduleName = compiler.GetModuleName();
        std::println(stderr,
                     "[shader_cooker] module {} declares {} entrypoints",
                     moduleName,
                     compiler.GetEntryPointNames().size());

        out_space = FindPermutationSpaceForModule(moduleName);

        if (CookResult<void> axisResult =
                VerifyAxisNamesAreDeclared(*out_space, compiler.GetModuleSourceTexts(), moduleName);
            !axisResult)
        {
            return axisResult;
        }

        ReportUndrivenExternConstants(*out_space, compiler.GetModuleSourceTexts(), moduleName);

        return compiler.ResolveExternConstantDefaults(*out_space);
    }

    /** Everything the cook measures for one compiled variant, before it reaches the tables. */
    void RecordVariantStatistics(const CompiledVariant& variant, CookStatistics& statistics)
    {
        ++statistics.VariantsCompiled;
        statistics.EntryPointsCompiled += static_cast<uint32_t>(variant.EntryPoints.size());

        for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
        {
            statistics.TotalWgslBytes += entryPoint.Code.size();
        }
    }

    void ReportVariantIfRequested(const CookerOptions& options, const CompiledVariant& variant)
    {
        if (!options.ReportReflection)
        {
            return;
        }

        std::println(stderr, "[shader_cooker] variant [{}]", variant.VariantDescription);
        for (size_t i = 0u; i < variant.EntryPoints.size(); ++i)
        {
            ReportEntryPointReflection(variant, i);
        }
    }

    /** Names each entry point once, from the first variant. Every variant holds the same set. */
    void CaptureEntryPointsOnce(CookedModule& cooked_module, const CompiledVariant& variant)
    {
        if (!cooked_module.EntryPoints.empty())
        {
            return;
        }

        cooked_module.EntryPoints.reserve(variant.EntryPoints.size());
        for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
        {
            cooked_module.EntryPoints.push_back(
                LibraryEntryPoint{ entryPoint.Name, entryPoint.Reflection.Stage });
        }
    }

    /** Stage 3 then stage 4, one variant at a time. The raw output is kept only when a dump asked for
     * it, because it holds the target text a second time and a 35-variant module is not small. */
    CookResult<void> CompileModuleVariants(const CookerOptions& options,
                                           SlangCompiler& compiler,
                                           const VariantSet& variant_set,
                                           CookedModule& cooked_module,
                                           RawModule& raw_module,
                                           std::vector<CompiledVariant>& out_module_variants,
                                           std::vector<CompiledVariant>& out_variants,
                                           CookStatistics& statistics)
    {
        const bool keepRawVariants = IsStageDumpRequested(options, StageDumpKind::Raw);

        for (const VariantDescriptor& descriptor : variant_set.Variants)
        {
            CookResult<RawVariant> rawResult = compiler.CompileVariantRaw(descriptor);
            if (!rawResult)
            {
                std::println(stderr,
                             "[shader_cooker] variant [{}] failed: {}",
                             DescribeAssignment(descriptor.Canonical),
                             ToString(rawResult.error()));
                return std::unexpected(rawResult.error());
            }

            const ResolveContext context =
                MakeResolveContext(descriptor.Canonical, compiler.GetExternConstantDefaults());
            CookResult<CompiledVariant> variantResult = ResolveVariant(rawResult.value(), context);
            if (!variantResult)
            {
                std::println(stderr,
                             "[shader_cooker] variant [{}] failed: {}",
                             DescribeAssignment(descriptor.Canonical),
                             ToString(variantResult.error()));
                return std::unexpected(variantResult.error());
            }

            if (keepRawVariants)
            {
                raw_module.Variants.push_back(std::move(rawResult.value()));
            }

            CompiledVariant& variant = variantResult.value();
            RecordVariantStatistics(variant, statistics);
            ReportVariantIfRequested(options, variant);

            if (options.ValidateReflectionAgainstWgsl)
            {
                statistics.ReflectionMismatches += ValidateVariantReflection(variant);
            }

            if (options.ReportReflection)
            {
                ReportUnreferencedBindings(variant);
            }

            CaptureEntryPointsOnce(cooked_module, variant);

            if (CookResult<void> appendResult =
                    AppendVariantToModule(cooked_module, variant, descriptor.Canonical);
                !appendResult)
            {
                return appendResult;
            }

            out_module_variants.push_back(variant);
            out_variants.push_back(std::move(variant));
        }

        return {};
    }

    /** Freezes the tables, then runs every check that reads the finished model. */
    CookResult<void> FinalizeModule(CookedModule& cooked_module,
                                    std::span<const CompiledVariant> module_variants)
    {
        FreezeModuleTables(cooked_module);

        if (CookResult<void> roundTripResult = VerifyLibraryRoundTrip(cooked_module, module_variants);
            !roundTripResult)
        {
            return roundTripResult;
        }

        std::println(stderr,
                     "[shader_cooker] module {} round trip verified: {} variants resolve to the text "
                     "the compiler produced",
                     cooked_module.Name,
                     cooked_module.Variants.size());

        const ModuleInfluence influence = ComputeAxisInfluence(cooked_module);
        return EnforceModulePolicy(cooked_module, influence);
    }

    CookResult<void> CookModule(const CookerOptions& options,
                                const std::filesystem::path& module_path,
                                OutputSink& sink,
                                std::vector<CompiledVariant>& out_variants,
                                CookedLibrary& out_library,
                                CookStatistics& statistics)
    {
        SlangCompiler compiler;
        const PermutationSpace* space = nullptr;

        if (CookResult<void> prepared = PrepareModuleCompiler(options, module_path, compiler, space);
            !prepared)
        {
            return prepared;
        }

        const CookResult<VariantSet> variantSet = EnumerateVariants(*space);
        if (!variantSet)
        {
            return std::unexpected(variantSet.error());
        }

        const std::string_view moduleName = compiler.GetModuleName();
        std::println(stderr,
                     "[shader_cooker] module {} expands to {} variants over an index space of {}",
                     moduleName,
                     variantSet.value().Variants.size(),
                     variantSet.value().SpaceSize);

        if (CookResult<void> spaceDump = WriteStageDumpIfRequested(options,
                                                                   sink,
                                                                   moduleName,
                                                                   StageDumpKind::Space,
                                                                   [&]
                                                                   {
                                                                       return DumpPermutationSpace(moduleName,
                                                                                                   *space);
                                                                   });
            !spaceDump)
        {
            return spaceDump;
        }

        if (CookResult<void> variantDump =
                WriteStageDumpIfRequested(options,
                                          sink,
                                          moduleName,
                                          StageDumpKind::Variants,
                                          [&]
                                          {
                                              return DumpVariantSet(moduleName, variantSet.value());
                                          });
            !variantDump)
        {
            return variantDump;
        }

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

        RawModule rawModule;
        rawModule.Name = moduleName;
        rawModule.EntryPointNames.assign(compiler.GetEntryPointNames().begin(),
                                         compiler.GetEntryPointNames().end());
        rawModule.ExternDefaults.assign(compiler.GetExternConstantDefaults().begin(),
                                        compiler.GetExternConstantDefaults().end());

        if (CookResult<void> compiled = CompileModuleVariants(options,
                                                              compiler,
                                                              variantSet.value(),
                                                              cookedModule,
                                                              rawModule,
                                                              moduleVariants,
                                                              out_variants,
                                                              statistics);
            !compiled)
        {
            return compiled;
        }

        if (CookResult<void> rawDump = WriteStageDumpIfRequested(options,
                                                                 sink,
                                                                 moduleName,
                                                                 StageDumpKind::Raw,
                                                                 [&]
                                                                 {
                                                                     return DumpRawModule(rawModule);
                                                                 });
            !rawDump)
        {
            return rawDump;
        }

        if (CookResult<void> resolvedDump =
                WriteStageDumpIfRequested(options,
                                          sink,
                                          moduleName,
                                          StageDumpKind::Resolved,
                                          [&]
                                          {
                                              return DumpResolvedModule(moduleName, moduleVariants);
                                          });
            !resolvedDump)
        {
            return resolvedDump;
        }

        if (CookResult<void> finalized = FinalizeModule(cookedModule, moduleVariants); !finalized)
        {
            return finalized;
        }

        if (CookResult<void> cookedDump = WriteStageDumpIfRequested(options,
                                                                    sink,
                                                                    moduleName,
                                                                    StageDumpKind::Cooked,
                                                                    [&]
                                                                    {
                                                                        return DumpCookedModule(cookedModule);
                                                                    });
            !cookedDump)
        {
            return cookedDump;
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
        const CookResult<void> moduleResult =
            CookModule(options, modulePath, sink, variants, library, statistics);
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

        // Both memory sinks take the real sink's primary name. The emitter builds every companion
        // artifact name from it, so a different name here would make the check compare a different
        // set of file names than the cook it stands in for.
        MemoryOutputSink first{ sink.PrimaryName() };
        const CookResult<CookStatistics> firstResult = RunCookOnce(options, first);
        if (!firstResult)
        {
            return firstResult;
        }

        MemoryOutputSink second{ sink.PrimaryName() };
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
