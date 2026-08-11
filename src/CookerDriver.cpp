#include "CookerDriver.hpp"
#include "PermutationSpace.hpp"
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

    CookResult<void> CookModule(const CookerOptions& options,
                                const std::filesystem::path& module_path,
                                std::vector<CompiledVariant>& out_variants,
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
        const CookResult<std::vector<PermutationAssignment>> assignments =
            EnumerateActiveCombinations(*space);
        if (!assignments)
        {
            return std::unexpected(assignments.error());
        }

        std::println(stderr,
                     "[shader_cooker] module {} expands to {} variants",
                     moduleName,
                     assignments.value().size());

        for (const PermutationAssignment& assignment : assignments.value())
        {
            CookResult<CompiledVariant> variantResult = compiler.CompileVariant(assignment);
            if (!variantResult)
            {
                std::println(stderr,
                             "[shader_cooker] variant [{}] failed: {}",
                             DescribeAssignment(assignment),
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

            out_variants.push_back(std::move(variant));
        }

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

    for (const std::filesystem::path& modulePath : options.ModulePaths)
    {
        std::println(stderr, "[shader_cooker] cooking {}", modulePath.string());
        const CookResult<void> moduleResult = CookModule(options, modulePath, variants, statistics);
        if (!moduleResult)
        {
            return std::unexpected(moduleResult.error());
        }
    }

    if (statistics.ReflectionMismatches != 0u)
    {
        return std::unexpected(CookError::ReflectionMismatch);
    }

    const std::string header = GenerateShaderHeader(variants);
    const CookResult<void> writeResult = sink.Write(header);
    if (!writeResult)
    {
        return std::unexpected(writeResult.error());
    }

    const std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
    statistics.ElapsedMilliseconds = elapsed.count();

    return statistics;
}

} // namespace velox::cooker
