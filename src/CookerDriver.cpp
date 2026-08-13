#include "CookerDriver.hpp"
#include "CookedLibrary.hpp"
#include "PermutationSpace.hpp"
#include "ShaderLibraryEmitter.hpp"
#include "SlangCompiler.hpp"
#include "WgslBindingScanner.hpp"
#include <chrono>
#include <print>

namespace velox::cooker
{

namespace
{

    void EnsureModuleCacheDirectory(const std::filesystem::path& cache_directory)
    {
        if (!std::filesystem::exists(cache_directory))
        {
            std::filesystem::create_directories(cache_directory);
        }
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

        for (const ReflectedBinding& binding : variant.EntryPoints.front().Reflection.Bindings)
        {
            bool referencedByAnyEntryPoint = false;
            for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
            {
                for (const ReflectedBinding& candidate : entryPoint.Reflection.Bindings)
                {
                    if (SameBindingLocation(candidate, binding) && candidate.EntryPointUsageMask != 0u)
                    {
                        referencedByAnyEntryPoint = true;
                    }
                }
            }

            if (!referencedByAnyEntryPoint)
            {
                std::println(stderr,
                             "[shader_cooker] unreferenced binding in [{}]: {} is declared but no "
                             "entrypoint reads it",
                             variant.VariantDescription,
                             DescribeBinding(binding));
            }
        }
    }

    uint32_t ValidateVariantReflection(const CompiledVariant& variant)
    {
        uint32_t mismatchCount = 0u;

        for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
        {
            const std::vector<WgslDeclaredBinding> declared = ScanWgslBindings(entryPoint.Code);
            const std::vector<ReflectedBinding> used =
                SelectBindingsUsedByEntryPoint(entryPoint.Reflection);
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

    /** Writes the header and one source file for each module. The header name comes from the sink, so
     * the generated source includes exactly the file the user asked for. */
    CookResult<void> EmitLibraryArtifacts(const CookedLibrary& library,
                                          OutputSink& sink,
                                          CookStatistics& statistics)
    {
        const std::string headerName{ sink.PrimaryName() };
        const std::filesystem::path headerPath{ headerName };
        const std::string headerStem = headerPath.stem().string();

        const std::string header = EmitShaderLibraryHeader(library);
        const CookResult<void> headerResult = sink.Write(header);
        if (!headerResult)
        {
            return headerResult;
        }

        for (const CookedModule& module : library.Modules)
        {
            const std::string sourceName = MakeModuleSourceFileName(headerStem, module.Name);
            const std::string source = EmitShaderLibraryModuleSource(module, headerName);

            const CookResult<void> sourceResult = sink.WriteArtifact(sourceName, source);
            if (!sourceResult)
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
        const CookResult<void> initializeResult = compiler.Initialize(createInfo);
        if (!initializeResult)
        {
            return initializeResult;
        }

        const std::string_view moduleName = compiler.GetModuleName();
        const std::span<const std::string> entryPointNames = compiler.GetEntryPointNames();
        std::println(stderr,
                     "[shader_cooker] module {} declares {} entrypoints",
                     moduleName,
                     entryPointNames.size());

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

            const CookResult<void> appendResult = AppendVariantToModule(cookedModule, variant);
            if (!appendResult)
            {
                return std::unexpected(appendResult.error());
            }

            moduleVariants.push_back(variant);
            out_variants.push_back(std::move(variant));
        }

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

        out_library.Modules.push_back(std::move(cookedModule));
        ++statistics.ModulesCooked;
        return {};
    }

} // namespace

CookResult<CookStatistics> RunCook(const CookerOptions& options, OutputSink& sink)
{
    const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    EnsureModuleCacheDirectory(options.ModuleCacheDirectory);

    CookStatistics statistics;
    std::vector<CompiledVariant> variants;
    CookedLibrary library;

    for (const std::filesystem::path& modulePath : options.ModulePaths)
    {
        std::println(stderr, "[shader_cooker] cooking {}", modulePath.string());
        const CookResult<void> moduleResult =
            CookModule(options, modulePath, variants, library, statistics);
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

} // namespace velox::cooker
