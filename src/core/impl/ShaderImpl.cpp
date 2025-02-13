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
        errorSession(error_session) {}

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl && other) noexcept : stHandles(std::move(other.stHandles)), reflector(std::move(other.reflector)), rsrcFile(std::move(other.rsrcFile)),
        groupName(std::move(other.groupName)), resourceScriptPath(std::move(other.resourceScriptPath)), errorSession(other.errorSession) {}

    ShaderGroupImpl& ShaderGroupImpl::operator=(ShaderGroupImpl&& other) noexcept
    {
        stHandles = std::move(other.stHandles);
        reflector = std::move(other.reflector);
        rsrcFile = std::move(other.rsrcFile);
        groupName = std::move(other.groupName);
        resourceScriptPath = std::move(other.resourceScriptPath);
        errorSession = std::move(other.errorSession);
        return *this;
    }

    void ShaderGroupImpl::addShaderStage(const ShaderStage& handle)
    {
        stHandles.emplace(handle);
        reflector->ParseBinary(handle);
    }

}