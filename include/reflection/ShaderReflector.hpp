#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/ShaderToolsErrors.hpp"

namespace st
{

    class Shader;
    class ShaderGroupImpl;
    class ShaderReflectorImpl;
    class ResourceUsage;
    struct VertexAttributeInfo;
    struct PushConstantInfo;
    struct yamlFile;
    struct Session;

    class ShaderReflector
    {
        ShaderReflector(const ShaderReflector&) = delete;
        ShaderReflector& operator=(const ShaderReflector&) = delete;
    public:

        ShaderReflector(yamlFile* yaml_file, Session& error_session);
        ~ShaderReflector();
        ShaderReflector(ShaderReflector&& other) noexcept;
        ShaderReflector& operator=(ShaderReflector&& other) noexcept;

        ShaderToolsErrorCode ParseBinary(const ShaderStage& shader, std::string shader_name);
        uint32_t GetNumSets() const noexcept;
        void GetShaderResources(const size_t set_idx, size_t* num_resources, ResourceUsage* resources);
        void GetInputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes);
        void GetOutputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes);
        PushConstantInfo GetStagePushConstantInfo(const VkShaderStageFlags stage) const;

        friend class Shader;
        friend class ShaderGroupImpl;
    protected:
        // required as the shader and shader group have to get down to the impl, in order to finish *using* the reflection data
        ShaderReflectorImpl* GetImpl();
    private:
        std::unique_ptr<ShaderReflectorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP
