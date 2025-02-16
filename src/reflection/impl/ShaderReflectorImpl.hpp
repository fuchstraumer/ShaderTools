#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "reflection/ReflectionStructs.hpp"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <string>
#include <vulkan/vulkan_core.h>

namespace spirv_cross
{
    class Compiler;
    class CompilerGLSL;
    struct Resource;
}

namespace st
{
    struct ShaderStage;
    class ResourceUsage;
    struct yamlFile;
    struct Session;

    struct DescriptorSetInfo
    {
        uint32_t Binding;
        std::map<uint32_t, ResourceUsage> Members;
    };

    class ShaderReflectorImpl
    {
        ShaderReflectorImpl(const ShaderReflectorImpl&) = delete;
        ShaderReflectorImpl& operator=(const ShaderReflectorImpl&) = delete;
    public:

        ShaderReflectorImpl(yamlFile* yaml_file, Session& error_session) noexcept;
        ~ShaderReflectorImpl();
        ShaderReflectorImpl(ShaderReflectorImpl&& other) noexcept;
        ShaderReflectorImpl& operator=(ShaderReflectorImpl&& other) noexcept;

        void collateSets();
        ShaderToolsErrorCode parseSpecializationConstants();
        ShaderToolsErrorCode parseResourceType(const std::string& shader_name, const ShaderStage& shader_handle, const VkDescriptorType& type_being_parsed);

        std::vector<ShaderResourceSubObject> ExtractBufferMembers(const spirv_cross::Compiler& cmplr, const spirv_cross::Resource& rsrc);
        std::string GetActualResourceName(const std::string& rsrc_name);

        ShaderToolsErrorCode parseBinary(const ShaderStage& shader_handle, std::string shader_name);
        ShaderToolsErrorCode parseImpl(const ShaderStage& shader_handle, const std::string& shader_name, std::vector<uint32_t> binary_data);

        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::map<uint32_t, SpecializationConstant> specializationConstants;
        std::map<uint32_t, DescriptorSetInfo> sortedSets;
        std::multimap<uint32_t, ResourceUsage> tempResources;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::unordered_map<VkShaderStageFlags, std::vector<VertexAttributeInfo>> inputAttributes;
        std::unordered_map<VkShaderStageFlags, std::vector<VertexAttributeInfo>> outputAttributes;
        std::unordered_map<std::string, uint32_t> resourceGroupSetIndices;
        std::unordered_set<std::string> usedResourceGroupNames;
        std::unique_ptr<spirv_cross::CompilerGLSL> recompiler;
        
        yamlFile* rsrcFile;
        Session& errorSession;

        size_t getNumSets() const noexcept;

    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
