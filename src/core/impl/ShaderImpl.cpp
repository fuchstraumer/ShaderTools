#include "ShaderImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "reflection/ShaderReflector.hpp"
#include "../../reflection/impl/ShaderReflectorImpl.hpp"

namespace st {

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, yamlFile* yaml_file) : groupName(group_name), reflector(std::make_unique<ShaderReflector>(yaml_file)), rsrcFile(yaml_file) {}

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl && other) noexcept : stHandles(std::move(other.stHandles)), reflector(std::move(other.reflector)), rsrcFile(std::move(other.rsrcFile)), 
        groupName(std::move(other.groupName)), resourceScriptPath(std::move(other.resourceScriptPath)) {}

    ShaderGroupImpl & ShaderGroupImpl::operator=(ShaderGroupImpl && other) noexcept
    {
        stHandles = std::move(other.stHandles);
        reflector = std::move(other.reflector);
        rsrcFile = std::move(other.rsrcFile);
        groupName = std::move(other.groupName);
        resourceScriptPath = std::move(other.resourceScriptPath);
        return *this;
    }

    void ShaderGroupImpl::addShaderStage(const ShaderStage& handle)
    {
        stHandles.emplace(handle);
        reflector->ParseBinary(handle);
    }

}