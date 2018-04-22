#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#include "common/CommonInclude.hpp"
#include "DescriptorStructs.hpp"
#include "common/ShaderResource.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include "spirv-cross/spirv_glsl.hpp"
#include <array>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <set>
#include "../util/ShaderFileTracker.hpp"

namespace st {

    struct DescriptorSetInfo {
        uint32_t Binding;
        std::map<uint32_t, ShaderResource> Members;
    };

    class BindingGeneratorImpl {
        BindingGeneratorImpl(const BindingGeneratorImpl&) = delete;
        BindingGeneratorImpl& operator=(const BindingGeneratorImpl&) = delete;
    public:

        BindingGeneratorImpl() = default;
        ~BindingGeneratorImpl() = default;
        BindingGeneratorImpl(BindingGeneratorImpl&& other) noexcept;
        BindingGeneratorImpl& operator=(BindingGeneratorImpl&& other) noexcept;

        void parseBinary(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage);
        void collateSets();
        void parseResourceType(const VkShaderStageFlags & stage, const VkDescriptorType & type_being_parsed);
        void parseBinary(const Shader& shader_handle);
        void parseImpl(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage);

        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::map<uint32_t, DescriptorSetInfo> sortedSets;
        std::multimap<uint32_t, ShaderResource> tempResources;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> inputAttributes;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> outputAttributes;
        std::map<uint32_t, SpecializationConstant> specializationConstants;
        std::unique_ptr<spirv_cross::Compiler> recompiler{ nullptr };

        decltype(sortedSets)::iterator findSetWithIdx(const uint32_t idx);
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP