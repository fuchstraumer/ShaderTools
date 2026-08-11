#include "SlangCompiler.hpp"

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <array>
#include <future>
#include <print>
#include <vector>

namespace velox::cooker
{

namespace
{

    constexpr SlangInt k_WgslTargetIndex = 0;
    constexpr const char* k_AllWarningsAsErrors = "all";
    constexpr const char* k_DisabledWarnings = "31010";
    constexpr SlangUInt k_WorkgroupAxisCount = 3u;

    std::string BlobToString(slang::IBlob* blob)
    {
        if (blob == nullptr)
        {
            return {};
        }

        return std::string{ static_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize() };
    }

    void ReportDiagnostics(std::string_view context, slang::IBlob* blob)
    {
        if (blob == nullptr || blob->getBufferSize() == 0u)
        {
            return;
        }

        std::println(stderr, "[shader_cooker] {}: {}", context, BlobToString(blob));
    }

    SlangOptimizationLevel ToSlangOptimizationLevel(uint32_t level) noexcept
    {
        switch (level)
        {
        case 1u:
            return SLANG_OPTIMIZATION_LEVEL_DEFAULT;
        case 2u:
            return SLANG_OPTIMIZATION_LEVEL_HIGH;
        case 3u:
            return SLANG_OPTIMIZATION_LEVEL_MAXIMAL;
        default:
            return SLANG_OPTIMIZATION_LEVEL_NONE;
        }
    }

    ShaderStageKind FromSlangStage(SlangStage stage) noexcept
    {
        switch (stage)
        {
        case SLANG_STAGE_VERTEX:
            return ShaderStageKind::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return ShaderStageKind::Fragment;
        case SLANG_STAGE_COMPUTE:
            return ShaderStageKind::Compute;
        default:
            return ShaderStageKind::Invalid;
        }
    }

    BindingKind FromSlangBindingType(slang::BindingType binding_type) noexcept
    {
        switch (binding_type)
        {
        case slang::BindingType::Sampler:
            return BindingKind::Sampler;
        case slang::BindingType::Texture:
            return BindingKind::SampledTexture;
        case slang::BindingType::MutableTexture:
            return BindingKind::StorageTexture;
        case slang::BindingType::ConstantBuffer:
        case slang::BindingType::ParameterBlock:
        case slang::BindingType::InlineUniformData:
            return BindingKind::UniformBuffer;
        case slang::BindingType::RawBuffer:
        case slang::BindingType::TypedBuffer:
            return BindingKind::ReadOnlyStorageBuffer;
        case slang::BindingType::MutableRawBuffer:
        case slang::BindingType::MutableTypedBuffer:
            return BindingKind::StorageBuffer;
        default:
            return BindingKind::Invalid;
        }
    }

    std::vector<slang::CompilerOptionEntry> MakeCompilerOptions(uint32_t optimization_level)
    {
        std::vector<slang::CompilerOptionEntry> options;
        options.reserve(6u);

        slang::CompilerOptionEntry warningLevel{};
        warningLevel.name = slang::CompilerOptionName::WarningLevel;
        warningLevel.value.kind = slang::CompilerOptionValueKind::Int;
        warningLevel.value.intValue0 = SLANG_WARNING_LEVEL_PEDANTIC;
        options.push_back(warningLevel);
        warningLevel.value.intValue0 = SLANG_WARNING_LEVEL_ALL;
        options.push_back(warningLevel);

        slang::CompilerOptionEntry disableWarnings{};
        disableWarnings.name = slang::CompilerOptionName::DisableWarnings;
        disableWarnings.value.kind = slang::CompilerOptionValueKind::String;
        disableWarnings.value.stringValue0 = k_DisabledWarnings;
        options.push_back(disableWarnings);

        slang::CompilerOptionEntry warningsAsErrors{};
        warningsAsErrors.name = slang::CompilerOptionName::WarningsAsErrors;
        warningsAsErrors.value.kind = slang::CompilerOptionValueKind::String;
        warningsAsErrors.value.stringValue0 = k_AllWarningsAsErrors;
        options.push_back(warningsAsErrors);

        slang::CompilerOptionEntry floatingPointMode{};
        floatingPointMode.name = slang::CompilerOptionName::FloatingPointMode;
        floatingPointMode.value.kind = slang::CompilerOptionValueKind::Int;
        floatingPointMode.value.intValue0 = SLANG_FLOATING_POINT_MODE_FAST;
        options.push_back(floatingPointMode);

        slang::CompilerOptionEntry debugInformation{};
        debugInformation.name = slang::CompilerOptionName::DebugInformation;
        debugInformation.value.kind = slang::CompilerOptionValueKind::Int;
        debugInformation.value.intValue0 = SLANG_DEBUG_INFO_LEVEL_NONE;
        options.push_back(debugInformation);

        slang::CompilerOptionEntry optimization{};
        optimization.name = slang::CompilerOptionName::Optimization;
        optimization.value.kind = slang::CompilerOptionValueKind::Int;
        optimization.value.intValue0 = ToSlangOptimizationLevel(optimization_level);
        options.push_back(optimization);

        return options;
    }

} // namespace

struct SlangCompiler::Impl
{
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;
    slang::IModule* rootModule{ nullptr };
    std::vector<slang::IComponentType*> baseComponents;
    std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPointHandles;
    std::vector<std::string> entryPointNames;
    std::vector<slang::CompilerOptionEntry> compilerOptions;
    std::string moduleName;
    bool multithreadEntryPointCodegen{ true };

    CookResult<void> CreateSession(const SlangCompilerCreateInfo& create_info);
    CookResult<void> LoadRootModule();
    CookResult<void> CollectEntryPoints();
    CookResult<Slang::ComPtr<slang::IComponentType>> LinkVariant(
        const PermutationAssignment& assignment);
    std::vector<std::string> GenerateEntryPointCode(slang::IComponentType* linked_program);
    EntryPointReflection ExtractEntryPointReflection(slang::IComponentType* linked_program,
                                                     slang::ProgramLayout* program_layout,
                                                     SlangInt entry_point_index);
    std::vector<ReflectedBinding> ExtractGlobalBindings(slang::ProgramLayout* program_layout);
    void ApplyEntryPointUsage(slang::IComponentType* linked_program,
                              SlangInt entry_point_index,
                              std::vector<ReflectedBinding>& bindings);
};

CookResult<void> SlangCompiler::Impl::CreateSession(const SlangCompilerCreateInfo& create_info)
{
    if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())) || !globalSession)
    {
        return std::unexpected(CookError::GlobalSessionCreationFailed);
    }

    compilerOptions = MakeCompilerOptions(create_info.OptimizationLevel);

    const std::filesystem::path canonicalModulePath = std::filesystem::canonical(create_info.ModulePath);
    const std::string sourceDirectory = canonicalModulePath.parent_path().string();
    const std::string cacheDirectory = create_info.ModuleCacheDirectory.string();
    const std::array<const char*, 2> searchPaths{ sourceDirectory.c_str(), cacheDirectory.c_str() };

    slang::TargetDesc target{};
    target.format = SLANG_WGSL;
    target.profile = globalSession->findProfile("spirv_1_4");

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &target;
    sessionDesc.targetCount = 1;
    sessionDesc.searchPaths = searchPaths.data();
    sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());
    sessionDesc.compilerOptionEntries = compilerOptions.data();
    sessionDesc.compilerOptionEntryCount = static_cast<SlangInt>(compilerOptions.size());

    if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())) || !session)
    {
        return std::unexpected(CookError::SessionCreationFailed);
    }

    moduleName = canonicalModulePath.stem().string();
    multithreadEntryPointCodegen = create_info.MultithreadEntryPointCodegen;
    return {};
}

CookResult<void> SlangCompiler::Impl::LoadRootModule()
{
    Slang::ComPtr<slang::IBlob> diagnostics;
    rootModule = session->loadModule(moduleName.c_str(), diagnostics.writeRef());
    ReportDiagnostics("loadModule", diagnostics.get());

    if (rootModule == nullptr)
    {
        return std::unexpected(CookError::ModuleLoadFailed);
    }

    baseComponents.clear();
    baseComponents.reserve(4u + static_cast<size_t>(rootModule->getDefinedEntryPointCount()));
    baseComponents.push_back(rootModule);
    return {};
}

CookResult<void> SlangCompiler::Impl::CollectEntryPoints()
{
    const SlangInt entryPointCount = rootModule->getDefinedEntryPointCount();
    entryPointNames.clear();
    entryPointNames.reserve(static_cast<size_t>(entryPointCount));
    entryPointHandles.clear();
    entryPointHandles.reserve(static_cast<size_t>(entryPointCount));

    for (SlangInt i = 0; i < entryPointCount; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        if (SLANG_FAILED(rootModule->getDefinedEntryPoint(i, entryPoint.writeRef())))
        {
            return std::unexpected(CookError::EntryPointEnumerationFailed);
        }

        entryPointNames.emplace_back(entryPoint->getFunctionReflection()->getName());
        baseComponents.push_back(entryPoint.get());
        entryPointHandles.push_back(entryPoint);
    }

    return {};
}

CookResult<Slang::ComPtr<slang::IComponentType>> SlangCompiler::Impl::LinkVariant(
    const PermutationAssignment& assignment)
{
    std::vector<slang::IComponentType*> components = baseComponents;
    components.reserve(baseComponents.size() + assignment.size());

    for (const PermutationBinding& binding : assignment)
    {
        const std::string variantModuleName = MakeVariantModuleName(binding.first->Name, binding.second);
        const std::string variantModulePath = MakeVariantModulePath(binding.first->Name, binding.second);
        const std::string variantSource = MakeExportedConstantSource(binding.first->Name, binding.second);

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* variantModule = session->loadModuleFromSourceString(variantModuleName.c_str(),
                                                                           variantModulePath.c_str(),
                                                                           variantSource.c_str(),
                                                                           diagnostics.writeRef());
        ReportDiagnostics("loadModuleFromSourceString", diagnostics.get());

        if (variantModule == nullptr)
        {
            return std::unexpected(CookError::VariantModuleCreationFailed);
        }

        components.push_back(variantModule);
    }

    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IComponentType> composite;
    session->createCompositeComponentType(components.data(),
                                          static_cast<SlangInt>(components.size()),
                                          composite.writeRef(),
                                          diagnostics.writeRef());
    ReportDiagnostics("createCompositeComponentType", diagnostics.get());

    if (!composite)
    {
        return std::unexpected(CookError::CompositeCreationFailed);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    if (SLANG_FAILED(composite->link(linked.writeRef(), diagnostics.writeRef())))
    {
        ReportDiagnostics("link", diagnostics.get());
        return std::unexpected(CookError::LinkFailed);
    }

    return linked;
}

std::vector<std::string> SlangCompiler::Impl::GenerateEntryPointCode(slang::IComponentType* linked_program)
{
    const size_t entryPointCount = entryPointNames.size();
    std::vector<std::string> generated(entryPointCount);

    if (multithreadEntryPointCodegen)
    {
        std::vector<std::future<std::string>> pending;
        pending.reserve(entryPointCount);

        for (size_t i = 0; i < entryPointCount; ++i)
        {
            pending.push_back(std::async(std::launch::async,
                                         [linked_program, i]()
                                         {
                                             Slang::ComPtr<slang::IBlob> code;
                                             Slang::ComPtr<slang::IBlob> diagnostics;
                                             if (SLANG_FAILED(linked_program->getEntryPointCode(
                                                     static_cast<SlangInt>(i),
                                                     k_WgslTargetIndex,
                                                     code.writeRef(),
                                                     diagnostics.writeRef())))
                                             {
                                                 ReportDiagnostics("getEntryPointCode",
                                                                   diagnostics.get());
                                                 return std::string{};
                                             }
                                             return BlobToString(code.get());
                                         }));
        }

        for (size_t i = 0; i < entryPointCount; ++i)
        {
            generated[i] = pending[i].get();
        }

        return generated;
    }

    for (size_t i = 0; i < entryPointCount; ++i)
    {
        Slang::ComPtr<slang::IBlob> code;
        Slang::ComPtr<slang::IBlob> diagnostics;
        if (SLANG_FAILED(linked_program->getEntryPointCode(static_cast<SlangInt>(i),
                                                           k_WgslTargetIndex,
                                                           code.writeRef(),
                                                           diagnostics.writeRef())))
        {
            ReportDiagnostics("getEntryPointCode", diagnostics.get());
            continue;
        }

        generated[i] = BlobToString(code.get());
    }

    return generated;
}

std::vector<ReflectedBinding> SlangCompiler::Impl::ExtractGlobalBindings(
    slang::ProgramLayout* program_layout)
{
    std::vector<ReflectedBinding> bindings;

    slang::TypeLayoutReflection* globalLayout = program_layout->getGlobalParamsTypeLayout();
    if (globalLayout == nullptr)
    {
        return bindings;
    }

    const SlangInt bindingRangeCount = globalLayout->getBindingRangeCount();
    bindings.reserve(static_cast<size_t>(bindingRangeCount));

    for (SlangInt rangeIndex = 0; rangeIndex < bindingRangeCount; ++rangeIndex)
    {
        const slang::BindingType bindingType = globalLayout->getBindingRangeType(rangeIndex);
        if (bindingType == slang::BindingType::Unknown ||
            bindingType == slang::BindingType::VaryingInput ||
            bindingType == slang::BindingType::VaryingOutput)
        {
            continue;
        }

        const SlangInt descriptorSetIndex = globalLayout->getBindingRangeDescriptorSetIndex(rangeIndex);
        const SlangInt descriptorRangeIndex =
            globalLayout->getBindingRangeFirstDescriptorRangeIndex(rangeIndex);
        if (descriptorSetIndex < 0 || descriptorRangeIndex < 0)
        {
            continue;
        }

        const SlangInt spaceOffset = globalLayout->getDescriptorSetSpaceOffset(descriptorSetIndex);
        const SlangInt indexOffset =
            globalLayout->getDescriptorSetDescriptorRangeIndexOffset(descriptorSetIndex,
                                                                     descriptorRangeIndex);

        if (static_cast<size_t>(indexOffset) == SLANG_UNKNOWN_SIZE ||
            static_cast<size_t>(spaceOffset) == SLANG_UNKNOWN_SIZE)
        {
            std::println(stderr,
                         "[shader_cooker] binding range {} has an unresolved location: link-time "
                         "constants are affecting reflection output",
                         rangeIndex);
            continue;
        }

        slang::VariableReflection* leafVariable = globalLayout->getBindingRangeLeafVariable(rangeIndex);
        const char* leafName = leafVariable != nullptr ? leafVariable->getName() : nullptr;

        ReflectedBinding binding;
        binding.Name = leafName != nullptr ? leafName : std::string{};
        binding.Group = static_cast<uint32_t>(spaceOffset);
        binding.Binding = static_cast<uint32_t>(indexOffset);
        binding.Kind = FromSlangBindingType(bindingType);
        bindings.push_back(std::move(binding));
    }

    SortBindingsByLocation(bindings);
    return bindings;
}

void SlangCompiler::Impl::ApplyEntryPointUsage(slang::IComponentType* linked_program,
                                               SlangInt entry_point_index,
                                               std::vector<ReflectedBinding>& bindings)
{
    Slang::ComPtr<slang::IMetadata> metadata;
    Slang::ComPtr<slang::IBlob> diagnostics;
    if (SLANG_FAILED(linked_program->getEntryPointMetadata(entry_point_index,
                                                           k_WgslTargetIndex,
                                                           metadata.writeRef(),
                                                           diagnostics.writeRef())) ||
        !metadata)
    {
        ReportDiagnostics("getEntryPointMetadata", diagnostics.get());
        return;
    }

    for (ReflectedBinding& binding : bindings)
    {
        bool isUsed = false;
        metadata->isParameterLocationUsed(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT,
                                          binding.Group,
                                          binding.Binding,
                                          isUsed);
        if (isUsed)
        {
            binding.EntryPointUsageMask |= (1u << static_cast<uint32_t>(entry_point_index));
        }
    }
}

EntryPointReflection SlangCompiler::Impl::ExtractEntryPointReflection(
    slang::IComponentType* linked_program,
    slang::ProgramLayout* program_layout,
    SlangInt entry_point_index)
{
    EntryPointReflection reflection;
    reflection.Name = entryPointNames[static_cast<size_t>(entry_point_index)];
    reflection.Bindings = ExtractGlobalBindings(program_layout);

    slang::EntryPointReflection* entryPointLayout =
        program_layout->getEntryPointByIndex(static_cast<SlangUInt>(entry_point_index));
    if (entryPointLayout == nullptr)
    {
        return reflection;
    }

    reflection.Stage = FromSlangStage(entryPointLayout->getStage());

    if (reflection.Stage == ShaderStageKind::Compute)
    {
        std::array<SlangUInt, k_WorkgroupAxisCount> workgroupSizes{ 1u, 1u, 1u };
        entryPointLayout->getComputeThreadGroupSize(k_WorkgroupAxisCount, workgroupSizes.data());
        reflection.Workgroup.X = static_cast<uint32_t>(workgroupSizes[0]);
        reflection.Workgroup.Y = static_cast<uint32_t>(workgroupSizes[1]);
        reflection.Workgroup.Z = static_cast<uint32_t>(workgroupSizes[2]);
    }

    ApplyEntryPointUsage(linked_program, entry_point_index, reflection.Bindings);
    return reflection;
}

SlangCompiler::SlangCompiler() noexcept :
    impl{ nullptr }
{
}

SlangCompiler::~SlangCompiler() = default;
SlangCompiler::SlangCompiler(SlangCompiler&&) noexcept = default;
SlangCompiler& SlangCompiler::operator=(SlangCompiler&&) noexcept = default;

CookResult<void> SlangCompiler::Initialize(const SlangCompilerCreateInfo& create_info)
{
    impl = std::make_unique<Impl>();

    const CookResult<void> sessionResult = impl->CreateSession(create_info);
    if (!sessionResult)
    {
        return sessionResult;
    }

    const CookResult<void> moduleResult = impl->LoadRootModule();
    if (!moduleResult)
    {
        return moduleResult;
    }

    return impl->CollectEntryPoints();
}

CookResult<CompiledVariant> SlangCompiler::CompileVariant(const PermutationAssignment& assignment)
{
    if (impl == nullptr)
    {
        return std::unexpected(CookError::CompilerNotInitialized);
    }

    CookResult<Slang::ComPtr<slang::IComponentType>> linkResult = impl->LinkVariant(assignment);
    if (!linkResult)
    {
        return std::unexpected(linkResult.error());
    }

    slang::IComponentType* linkedProgram = linkResult.value().get();
    slang::ProgramLayout* programLayout = linkedProgram->getLayout();
    if (programLayout == nullptr)
    {
        return std::unexpected(CookError::ReflectionUnavailable);
    }

    const std::vector<std::string> generatedCode = impl->GenerateEntryPointCode(linkedProgram);

    CompiledVariant variant;
    variant.VariantSuffix = MakeAssignmentSuffix(assignment);
    variant.VariantDescription = DescribeAssignment(assignment);
    variant.EntryPoints.reserve(impl->entryPointNames.size());

    for (size_t i = 0; i < impl->entryPointNames.size(); ++i)
    {
        if (generatedCode[i].empty())
        {
            return std::unexpected(CookError::CodeGenerationFailed);
        }

        CompiledEntryPoint entryPoint;
        entryPoint.Name = impl->entryPointNames[i];
        entryPoint.VariantSuffix = variant.VariantSuffix;
        entryPoint.Code = generatedCode[i];
        entryPoint.Reflection =
            impl->ExtractEntryPointReflection(linkedProgram, programLayout, static_cast<SlangInt>(i));
        variant.EntryPoints.push_back(std::move(entryPoint));
    }

    return variant;
}

std::string_view SlangCompiler::GetModuleName() const noexcept
{
    if (impl == nullptr)
    {
        return {};
    }

    return impl->moduleName;
}

std::span<const std::string> SlangCompiler::GetEntryPointNames() const noexcept
{
    if (impl == nullptr)
    {
        return {};
    }

    return impl->entryPointNames;
}

} // namespace velox::cooker
