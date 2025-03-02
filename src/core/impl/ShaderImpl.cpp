#include "ShaderImpl.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "reflection/ShaderReflector.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../reflection/impl/ShaderReflectorImpl.hpp"

namespace st
{

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, yamlFile* yaml_file, Session& error_session) :
        groupName(group_name),
        reflector(std::make_unique<ShaderReflector>(yaml_file, error_session)),
        rsrcFile(yaml_file),
        idx{ 0u }
    {
    }

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl&& other) noexcept :
        groupName{ std::move(other.groupName) },
        idx{ std::move(other.idx) },
        stHandles{ std::move(other.stHandles) },
        optimizationEnabled{ std::move(other.optimizationEnabled) },
        reflector{ std::move(other.reflector) },
        rsrcFile{ other.rsrcFile },
        tags{ std::move(other.tags) },
        resourceGroupBindingIndices{ std::move(other.resourceGroupBindingIndices) },
        resourceScriptPath{ std::move(other.resourceScriptPath) }
    {
    }

    ShaderGroupImpl& ShaderGroupImpl::operator=(ShaderGroupImpl&& other) noexcept
    {
        groupName = std::move(other.groupName);
        idx = std::move(other.idx);
        stHandles = std::move(other.stHandles);
        optimizationEnabled = std::move(other.optimizationEnabled);
        reflector = std::move(other.reflector);
        rsrcFile = other.rsrcFile;
        tags = std::move(other.tags);
        resourceGroupBindingIndices = std::move(other.resourceGroupBindingIndices);
        resourceScriptPath = std::move(other.resourceScriptPath);
        return *this;
    }

    void ShaderGroupImpl::addShaderStage(ShaderStage handle)
    {
        stHandles.emplace(handle);
        reflector->ParseBinary(handle);
    }

}