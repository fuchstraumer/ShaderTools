/**
 * OceanShaderCompiler
 * Loads Slang modules, enumerates defined entrypoints, compiles each to WGSL,
 * and bakes all variants into a C++ header.
 *
 * Usage:
 *   OceanShaderCompiler --output <header.hpp> [--wave-variants]
 *                       [--O[x]]... <module.slang>...
 * --O<n>: optimization level, 0-3, s, or z. If unspecified, defaults to 0 (no optimizations).
 *         this is probably the one to use, since we just feed this into Tint at runtime
 */

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <ranges>
#include <algorithm>
#include <expected>
#include <chrono>
#include <memory>
#include <type_traits>
#include <future>

/*
    TODO:
    - Refactor codegen to dump shaders to source, and add accessor to header
    - Generate accessor that uses a parameters struct for variants: struct created from Permutation Space
    - Dedupe variant sources, since a number of them should end up identical
    - Identify how we want to key/map our input accessor params to the shader variants, and generate that code
    - Expand permutations system so that an axis can actually generate multiple values, e.g. WAVE_SIZE would generate
      and set values for FFT_WAVE_SIZE_LOG2 (paves way for better subdivision of work via threadgroup scaling)
*/

namespace fs = std::filesystem;

namespace PermutationParameters
{
    using Value = std::variant<bool, uint32_t, int32_t>;

    struct Axis
    {
        std::string Name;
        std::vector<Value> Values;
        const Axis* Parent = nullptr;
        Value ReqParentValueToEnable;
    };

    using Space = std::vector<const Axis*>;
    using Assignment = std::vector<std::pair<const Axis*, Value>>;

    constexpr static bool k_PrintAssignmentTraversal = true;

    struct ValueStrBuilder
    {
        ValueStrBuilder(std::string_view _name, const Value& _value) : name(_name), value(_value) {}

        std::string SourceStr() const noexcept
        {
            return std::format("export static const {} {} = {};\n", heldTypeStr(), name, HeldValueStr());
        }

        std::string ModuleNameStr() const noexcept
        {
            return std::format("{}_{}", name, HeldValueStr());
        }

        std::string ModulePathStr() const noexcept
        {
            return std::format("{}_{}.slang", name, HeldValueStr());
        }

        std::string HeldValueStr() const noexcept
        {
            auto visitor =
                [](const auto& v) -> std::string
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        return v ? "true" : "false";
                    }
                    else if constexpr (std::is_same_v<T, uint32_t> ||
                                       std::is_same_v<T, int32_t>)
                    {
                        return std::to_string(v);
                    }
                };
            return std::visit(visitor, value);
        }

    private:

        std::string heldTypeStr() const noexcept
        {
            auto visitor =
                [](const auto& v) -> std::string
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        return "bool";
                    }
                    else if constexpr (std::is_same_v<T, uint32_t>)
                    {
                        return "uint";
                    }
                    else if constexpr (std::is_same_v<T, int32_t>)
                    {
                        return "int";
                    }
                };
            return std::visit(visitor, value);
        }

        const Value& value;
        std::string_view name;

    };

    inline std::vector<Assignment> EnumerateActiveCombinations(const Space& space)
    {
        // start with one empty combination, so we can start expansion
        std::vector<Assignment> partials{ Assignment{} };
        for (const Axis* axis : space)
        {
            std::vector<Assignment> expanded;
            expanded.reserve(partials.size() * axis->Values.size());
            for (const auto& partial : partials)
            {
                if (axis->Parent)
                {
                    auto iter = std::find_if(partial.begin(), partial.end(),
                                             [axis](const std::pair<const Axis*, Value>& p)
                                             {
                                                 return p.first == axis->Parent;
                                             });

                    assert(iter != partial.end() && "axis was declared before it's parent!");

                    if (iter->second != axis->ReqParentValueToEnable)
                    {
                        // parent value doesn't match, so this axis is disabled for this partial
                        expanded.push_back(partial);
                        continue;
                    }
                }

                for (const Value& val : axis->Values)
                {
                    Assignment next = partial;
                    next.emplace_back(axis, val);
                    expanded.push_back(std::move(next));
                }
            }

            partials = std::move(expanded);
        }

        if constexpr (k_PrintAssignmentTraversal)
        {
            std::println("[shader_compiler] EnumerateActiveCombinations: {} combinations", partials.size());
            for (const auto& assignment : partials)
            {
                std::string s;
                for (const auto& [axis, value] : assignment)
                {
                    PermutationParameters::ValueStrBuilder builder(axis->Name, value);
                    s += std::format("{}={}, ", axis->Name, builder.HeldValueStr());
                }
                std::println("[shader_compiler]   {}", s);
            }
        }

        return partials;
    }

}

const static PermutationParameters::Axis k_FftSizeParam{ "FFT_SIZE", {128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u}, nullptr };
const static PermutationParameters::Axis k_UseWaveOpsParam{ "FFT_USE_WAVE_OPS", {false, true}, nullptr };
const static PermutationParameters::Axis k_WaveSizeParam{ "FFT_WAVE_SIZE", {16u, 32u, 64u, 128u}, &k_UseWaveOpsParam, true };
const static PermutationParameters::Space k_IFFT_PermutationSpace{ &k_FftSizeParam, &k_UseWaveOpsParam, &k_WaveSizeParam };

struct CompiledEntryPoint
{
    std::string Name;
    // Axis = collection of potential values
    // VariantParams - discrete collection of values for *this* entrypoint
    std::vector<PermutationParameters::Value> VariantParams;
    // Code should go in string view to dedupe it eventually
    std::string Code;
    CompiledEntryPoint()
    {
        Name.reserve(32);
        VariantParams.reserve(4);
        Code.reserve(2048);
    }
};


std::string EntryPointSuffixStr(const std::vector<PermutationParameters::Value>& values)
{
    std::string s;
    for (const auto& value : values)
    {
        PermutationParameters::ValueStrBuilder builder("unused", value);
        s += std::format("_{}", builder.HeldValueStr());
    }
    return s;
}

// Add compile options from command line invocations
static std::vector<slang::CompilerOptionEntry> s_CompileOptions;
constexpr static const char* k_allWarningsAsErrorsStr = "all";
constexpr static const char* k_disabledWarningsStr = "31010"; // link-time constant array sizing is WIP and may break reflection
constexpr static SlangInt k_WgslTargetIndex = 0;

void AddDefaultCompileOptions()
{
    // catch all warnings as errors, since wgsl is a picky and strict target!
    slang::CompilerOptionEntry optWarningLevel{};
    optWarningLevel.name = slang::CompilerOptionName::WarningLevel;
    optWarningLevel.value.kind = slang::CompilerOptionValueKind::Int;
    // only level enabled by default is "extra". so we add pedantic and all
    optWarningLevel.value.intValue0 = SlangWarningLevel::SLANG_WARNING_LEVEL_PEDANTIC;
    s_CompileOptions.push_back(optWarningLevel);
    optWarningLevel.value.intValue0 = SlangWarningLevel::SLANG_WARNING_LEVEL_ALL;
    s_CompileOptions.push_back(optWarningLevel);
    // disable some warnings: E31010 warns that link time constant array sizing is WIP and may break reflection
    // this is fine for us, we don't use reflection and the alternative would make our life sooo much worse
    slang::CompilerOptionEntry optDisableWarnings{};
    optDisableWarnings.name = slang::CompilerOptionName::DisableWarnings;
    optDisableWarnings.value.kind = slang::CompilerOptionValueKind::String;
    optDisableWarnings.value.stringValue0 = k_disabledWarningsStr;
    s_CompileOptions.push_back(optDisableWarnings);
    // all warnings should be errors. also slang takes the string as a view, so its a constexpr static
    // at program scope. note that a lot of these should be caught from how much we've been testing
    // with test_ifft.py, but still I want to be as sure as I can be
    slang::CompilerOptionEntry optAllWarningsAsErrors{};
    optAllWarningsAsErrors.value.kind = slang::CompilerOptionValueKind::String;
    optAllWarningsAsErrors.value.stringValue0 = k_allWarningsAsErrorsStr;
    optAllWarningsAsErrors.name = slang::CompilerOptionName::WarningsAsErrors;
    s_CompileOptions.push_back(optAllWarningsAsErrors);
    // we want to use fast math, we were pretty intentional about our math and used FMA/mad where we could for precision
    slang::CompilerOptionEntry optFloatingPointMode{};
    optFloatingPointMode.value.kind = slang::CompilerOptionValueKind::Int;
    optFloatingPointMode.name = slang::CompilerOptionName::FloatingPointMode;
    optFloatingPointMode.value.intValue0 = static_cast<int32_t>(SlangFloatingPointMode::SLANG_FLOATING_POINT_MODE_FAST);
    s_CompileOptions.push_back(optFloatingPointMode);
    // disabling debug info, as it has a huge impact on quality of generated code in terms of perf and compactness
    slang::CompilerOptionEntry optDebugInfoLevel{};
    optDebugInfoLevel.name = slang::CompilerOptionName::DebugInformation;
    optDebugInfoLevel.value.kind = slang::CompilerOptionValueKind::Int;
    optDebugInfoLevel.value.intValue0 = SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_NONE;
    s_CompileOptions.push_back(optDebugInfoLevel);
    // enable checking "up to date" for binary modules slang finds in the search path
    slang::CompilerOptionEntry optCheckBinaryModuleUpToDate{};
    optCheckBinaryModuleUpToDate.name = slang::CompilerOptionName::UseUpToDateBinaryModule;
    optCheckBinaryModuleUpToDate.value.kind = slang::CompilerOptionValueKind::Int;
    optCheckBinaryModuleUpToDate.value.intValue0 = static_cast<int32_t>(true);
    s_CompileOptions.push_back(optCheckBinaryModuleUpToDate);
}

static std::string SlangBlobToStr(slang::IBlob* b)
{
    return b ? std::string{ static_cast<const char*>(b->getBufferPointer()), b->getBufferSize() } : "";
}

static Slang::ComPtr<slang::ISession> MakeSlangSession(
    slang::IGlobalSession* global,
    std::span<const std::string> searchPaths)
{
    std::vector<const char*> paths;
    paths.reserve(searchPaths.size());
    for (const std::string& p : searchPaths)
    {
        paths.push_back(p.c_str());
    }

    slang::TargetDesc td{};
    td.format = SlangCompileTarget::SLANG_WGSL;
    // spirv 1.4 should be broadly compatible with most devices
    td.profile = global->findProfile("spirv_1_4");
    const std::vector<slang::TargetDesc> targets = {td};

    slang::SessionDesc sd{};
    sd.targets                  = targets.data();
    sd.targetCount              = static_cast<SlangInt>(targets.size());
    sd.preprocessorMacros       = nullptr;
    sd.preprocessorMacroCount   = 0;
    sd.searchPaths              = paths.data();
    sd.searchPathCount          = static_cast<SlangInt>(paths.size());

    if (!s_CompileOptions.empty())
    {
        sd.compilerOptionEntries = s_CompileOptions.data();
        sd.compilerOptionEntryCount = static_cast<SlangInt>(s_CompileOptions.size());
    }
    else
    {
        sd.compilerOptionEntries = nullptr;
        sd.compilerOptionEntryCount = 0;
    }

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(global->createSession(sd, session.writeRef())))
    {
        std::println(stderr, "[slang] createSession failed");
    }
    return session;
}

std::string ExtractEntryPointBytecodeWGSL(Slang::ComPtr<slang::IComponentType> program_pointer, SlangInt entryPointIndex)
{
    Slang::ComPtr<slang::IBlob> codeBlob;
    Slang::ComPtr<slang::IBlob> diag;
    if (SLANG_FAILED(program_pointer->getEntryPointCode(entryPointIndex, k_WgslTargetIndex, codeBlob.writeRef(), diag.writeRef())))
    {
        std::println(stderr, "[shader_compiler] ExtractEntryPointBytecodeWGSL({}) failed\n", entryPointIndex);
        if (diag && diag->getBufferSize())
        {
            std::println(stderr, "[shader_compiler] Diagnostics: {}\n", SlangBlobToStr(diag.get()));
        }
        return {};
    }

    return SlangBlobToStr(codeBlob.get());
}

// we intentiontally take `components` by value since we mutate it per variant, but at the top level we fill it
// with the core module and the entrypoints
std::expected<std::vector<CompiledEntryPoint>, Slang::ComPtr<slang::IBlob>> CompileModuleVariant(
    Slang::ComPtr<slang::ISession> session,
    std::vector<slang::IComponentType*> components,
    SlangInt entryPointCount,
    const std::unordered_map<SlangInt, std::string>& entryPointNames,
    const PermutationParameters::Assignment& variantParams)
{
    // create variant "modules"
    for (const auto& [axis, value] : variantParams)
    {
        PermutationParameters::ValueStrBuilder builder(axis->Name, value);
        const std::string moduleName = builder.ModuleNameStr();
        const std::string moduleSource = builder.SourceStr();
        const std::string modulePath = builder.ModulePathStr();
        Slang::ComPtr<slang::IBlob> diag;
        // wait, how do we dispose of these modules? is slang tracking them internally?
        slang::IModule* variantModule = session->loadModuleFromSourceString(moduleName.c_str(), modulePath.c_str(), moduleSource.c_str(), diag.writeRef());

        if (!variantModule)
        {
            return std::unexpected(diag);
        }

        components.emplace_back(variantModule);
    }

    Slang::ComPtr<slang::IBlob> diag;
    Slang::ComPtr<slang::IComponentType> program;
    session->createCompositeComponentType(components.data(),
                                          static_cast<SlangInt>(components.size()),
                                          program.writeRef(), diag.writeRef());
    if (!program)
    {
        return std::unexpected(diag);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    if (SLANG_FAILED(program->link(linked.writeRef(), diag.writeRef())))
    {
        return std::unexpected(diag);
    }

    std::vector<CompiledEntryPoint> results(static_cast<size_t>(entryPointCount), CompiledEntryPoint{});
    // construct vector of just the parameter values for this variant, since each variant stores it
    std::vector<PermutationParameters::Value> variantValues;
    for (const auto& [axis, value] : variantParams)
    {
        variantValues.emplace_back(value);
    }

    // leaving this toggle in for now, but all evidence points to this being about 30% faster
    // that's worth it, for now
    constexpr bool k_MultithreadEntryPointCompilation = true;
    if constexpr (k_MultithreadEntryPointCompilation)
    {
        // GetEntryPointCode is one of the very few things we can multithread: use simple std::async and std::future
        // to compile each entrypoint in parallel
        using FutureResult = std::future<std::string>;
        for (SlangInt i = 0; i < entryPointCount; ++i)
        {
            results[static_cast<size_t>(i)].Name = entryPointNames.at(i);
            results[static_cast<size_t>(i)].VariantParams = variantValues;
        }

        // now dispatch the compilation of each entrypoint in parallel
        std::vector<FutureResult> futures;
        for (SlangInt i = 0; i < entryPointCount; ++i)
        {
            futures.emplace_back(std::async(std::launch::async, [linked, i]()
            {
                std::string wgslCode = ExtractEntryPointBytecodeWGSL(linked, i);
                if (wgslCode.empty())
                {
                    return std::string{};

                }
                return std::move(wgslCode);
            }));
        }

        for (SlangInt i = 0; i < entryPointCount; ++i)
        {
            auto result = futures[static_cast<size_t>(i)].get();
            if (result.empty())
            {
                return std::unexpected(diag);
            }
            results[static_cast<size_t>(i)].Code = std::move(result);
        }
    }
    else
    {
        for (SlangInt i = 0; i < entryPointCount; ++i)
        {
            std::string wgslCode = ExtractEntryPointBytecodeWGSL(linked, i);
            if (wgslCode.empty())
            {
                return std::unexpected(diag);
            }

            results[static_cast<size_t>(i)].Name = entryPointNames.at(i);
            results[static_cast<size_t>(i)].VariantParams = variantValues;
            results[static_cast<size_t>(i)].Code = std::move(wgslCode);
        }
    }

    auto variantPrinter = [&variantParams]()
    {
        std::string s; s.reserve(16 * variantParams.size());
        for (const auto& [axis, value] : variantParams)
        {
            PermutationParameters::ValueStrBuilder builder(axis->Name, value);
            s += std::format("{}={}, ", axis->Name, builder.HeldValueStr());
        }
        return s;
    };

    std::println(stderr, "[shader_compiler] compiled variant with values: {}", variantPrinter());

    return results;
}

static std::vector<CompiledEntryPoint> CompileModule(slang::IGlobalSession* global, const fs::path& modPath)
{
    fs::path temporary_dir_for_shaders = fs::temp_directory_path() / "OceanFFT_ShaderCompiler";
    if (!fs::exists(temporary_dir_for_shaders))
    {
        fs::create_directories(temporary_dir_for_shaders);
        std::println(stderr, "[shader_compiler] created temporary directory for cache: {}", temporary_dir_for_shaders.string());
    }
    else
    {
        std::println(stderr, "[shader_compiler] using existing temporary directory for cache: {}", temporary_dir_for_shaders.string());
    }

    fs::path canonicalModulePath = fs::canonical(modPath);
    std::vector<std::string> searchPaths = { canonicalModulePath.parent_path().string(), temporary_dir_for_shaders.string() };
    std::string stem = modPath.stem().string();
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    Slang::ComPtr<slang::ISession> session = MakeSlangSession(global, searchPaths);
    if (!session)
    {
        std::println(stderr, "[shader_compiler] session init failed for {}", stem);
        return {};
    }

    Slang::ComPtr<slang::IBlob> diag;
    slang::IModule* mod = session->loadModule(stem.c_str(), diag.writeRef());

    if (!mod)
    {
        std::println(stderr, "[shader_compiler] loadModule({}): {}", stem, SlangBlobToStr(diag.get()));
        return {};
    }

    if (diag && diag->getBufferSize())
    {
        std::println(stderr, "[shader_compiler] loadModule warnings: {}", SlangBlobToStr(diag.get()));
    }

    // init the components vector with the core module, which is the first component in the composite
    std::vector<slang::IComponentType*> components;
    components.reserve(4 + mod->getDefinedEntryPointCount());
    components.emplace_back(mod);

    // dump the module to file - just this "core" module we're compiling, not the dummy ones we create for each variant    
    SlangInt numModules = session->getLoadedModuleCount();
    for (SlangInt i = 0; i < numModules; ++i)
    {
        slang::IModule* loadedModule = session->getLoadedModule(i);
        const std::string moduleName = loadedModule->getName();
        const std::string modulePath = (temporary_dir_for_shaders / (moduleName + ".slang-module")).string();
        if (SLANG_FAILED(loadedModule->writeToFile(modulePath.c_str())))
        {
            std::println(stderr, "[shader_compiler] failed to write built module {} to {}", moduleName, modulePath);
        }
        else
        {
            const std::string moduleFName = fs::path(modulePath).filename().string();
            std::println(stderr, "[shader_compiler] wrote built module {} to cache as {}", moduleName, moduleFName);
        }
    }


    const SlangInt epCount = mod->getDefinedEntryPointCount();
    std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(epCount);
    // annoying thing: entrypoints are accessed by index after linking, but we can only get the names before linking, so we
    // need to have this map to map the indices back to the name
    std::unordered_map<SlangInt, std::string> entryPointNames;
    for (SlangInt i = 0; i < epCount; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> ep;
        if (SLANG_FAILED(mod->getDefinedEntryPoint(i, ep.writeRef())))
        {
            std::println(stderr, "[shader_compiler] getDefinedEntryPoint({}) failed for {}", i, stem);
            continue;
        }

        assert(!entryPointNames.contains(i));
        entryPointNames[i] = ep->getFunctionReflection()->getName();
        components.emplace_back(ep.get());
    }

    // print names of found entrypoints in module, just for sanity check / debugging
    std::println(stderr, "[shader_compiler] found {} entrypoints in module {}:", epCount, stem);
    for (SlangInt i = 0; i < epCount; ++i)
    {
        std::println(stderr, "[shader_compiler]   {}: {}", i, entryPointNames[i]);
    }

    std::vector<CompiledEntryPoint> results;

    // in the future, we'll have a better way to do this per module, but I really need to get moving in this project
    // so im hardcoding this. everything else I've done is generic and reusable, at least!
    // the real question will be, do we sort the space axes by dependencies, or just hardcode that too? lol
    auto curr_variant_space = k_IFFT_PermutationSpace;
    auto assignments = PermutationParameters::EnumerateActiveCombinations(curr_variant_space);
    for (const auto& assignment : assignments)
    {

        auto compileResult = CompileModuleVariant(session, components, epCount, entryPointNames, assignment);
        if (!compileResult)
        {
            std::println(stderr, "[shader_compiler] CompileModuleVariant failed for module {}: {}", stem, SlangBlobToStr(compileResult.error().get()));
            continue;
        }

        auto& compiledEntryPoints = compileResult.value();
        results.insert(results.end(),
                       std::make_move_iterator(compiledEntryPoints.begin()),
                       std::make_move_iterator(compiledEntryPoints.end()));
    }

    std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedTime = endTime - startTime;
    // cast elapsed time to milliseconds for easier reading
    std::chrono::duration<double, std::milli> elapsedTimeMs = elapsedTime;
    std::println(stderr, "[shader_compiler] compiled {} entrypoints for module {} in {}ms", results.size(), stem, elapsedTimeMs.count());

    return results;
}

static std::string GetShaderCodeArrayName(std::string_view name, std::string_view suffix)
{
    std::string s{"k_"};
    for (char c : name)
    {
        s += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    }
    for (char c : suffix)
    {
        s += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    }
    return s;
}

std::string WriteWgslShaderSourceToCppArray(const std::string& wgslSource, const std::string& arrayName)
{
    std::string cppArray;
    cppArray += "inline constexpr std::string_view " + arrayName + " = R\"WGSL_END(\n";
    cppArray += wgslSource;
    cppArray += ")WGSL_END\";\n";
    return cppArray;
}

static void WriteHeader(
    const std::vector<CompiledEntryPoint>& shaders,
    const fs::path& outPath)
{
    fs::path parentDir = outPath.parent_path();
    if (!fs::exists(parentDir))
    {
        fs::create_directories(parentDir);
    }
    else if (!fs::is_directory(parentDir))
    {
        std::println(stderr, "[shader_compiler] output path parent is not a directory: {}\n", parentDir.string());
        return;
    }

    std::ofstream f{outPath};
    if (!f.is_open())
    {
        std::println(stderr, "[shader_compiler] failed to open output file: {}\n", outPath.string());
        return;
    }

    f << "#pragma once\n"
         "// Auto-generated by shaders/shader_compiler.cpp -- do not edit manually.\n"
         "#include <string_view>\n\n"
         "namespace OceanFFT::Shaders\n{\n\n";

    for (const CompiledEntryPoint& shader : shaders)
    {
        const std::string shaderSuffix = EntryPointSuffixStr(shader.VariantParams);
        f << WriteWgslShaderSourceToCppArray(shader.Code, GetShaderCodeArrayName(shader.Name, shaderSuffix));
    }

    f << "} // namespace OceanFFT::Shaders\n";
    f.close();

    std::println(stderr, "[shader_compiler] wrote {} shader variants to {}", shaders.size(), outPath.string());
}

int main(int argc, char** argv)
{
    fs::path outputPath;
    std::vector<fs::path> modulePaths;
    AddDefaultCompileOptions();

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if ((arg == "--output" || arg == "-o") && i + 1 < argc)
        {
            outputPath = argv[++i];
        }
        else if (arg.starts_with("--O"))
        {
            // find level of optimization, e.g. --O0, --O1, --O2, --O3, --Os, --Oz
            char level = arg.at(3);
            slang::CompilerOptionEntry optLevel{};
            optLevel.name = slang::CompilerOptionName::Optimization;
            switch (level)
            {
            case '0':
                optLevel.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE;
                break;
            case '1':
                optLevel.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_DEFAULT;
                break;
            case '2':
                optLevel.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH;
                break;
            case '3':
                optLevel.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL;
                break;
            }
            s_CompileOptions.push_back(optLevel);
        }
        else if (!arg.starts_with('-'))
        {
            modulePaths.emplace_back(argv[i]);
        }
    }

    // if no optimization option in s_CompilerOptions, add default one that disables all optimizations
    auto hasOpt = std::find_if(
        s_CompileOptions.begin(),
        s_CompileOptions.end(),
        [](const slang::CompilerOptionEntry& opt)
        {
            return opt.name == slang::CompilerOptionName::Optimization;
        });

    if (hasOpt == s_CompileOptions.end())
    {
        slang::CompilerOptionEntry optLevel{};
        optLevel.name = slang::CompilerOptionName::Optimization;
        optLevel.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE;
        s_CompileOptions.push_back(optLevel);
    }

    if (outputPath.empty() || modulePaths.empty())
    {
        std::cerr << "Usage: OceanShaderCompiler --output <header.hpp>"
                     " [-O<level>]..."
                     " <module.slang>...\n";
        return 1;
    }

    Slang::ComPtr<slang::IGlobalSession> global;
    slang::createGlobalSession(global.writeRef());

    std::vector<CompiledEntryPoint> all_entry_points;
    for (auto& module_path : modulePaths)
    {
        std::println(stderr, "[shader_compiler] Compiling module: {}", module_path.string());
        auto entry_points = CompileModule(global.get(), module_path);
        all_entry_points.insert(all_entry_points.end(),
            std::make_move_iterator(entry_points.begin()),
            std::make_move_iterator(entry_points.end()));
    }

    if (all_entry_points.empty())
    {
        std::cerr << "no entrypoints compiled\n";
        return 1;
    }

    // now search entrypoints and dedupe: we can point multiple entrypoint variants to
    // the same source, but shouldn't have duplicate source in the header

    WriteHeader(all_entry_points, outputPath);

    return 0;
}