#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "DescriptorStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <set>
#include "../util/ShaderFileTracker.hpp"

namespace st {

    class ResourceUsage;

    struct DescriptorSetInfo {
        uint32_t Binding;
        std::map<uint32_t, ResourceUsage> Members;
    };

    class BindingGeneratorImpl {
        BindingGeneratorImpl(const BindingGeneratorImpl&) = delete;
        BindingGeneratorImpl& operator=(const BindingGeneratorImpl&) = delete;
    public:

        BindingGeneratorImpl() = default;
        ~BindingGeneratorImpl() = default;
        BindingGeneratorImpl(BindingGeneratorImpl&& other) noexcept;
        BindingGeneratorImpl& operator=(BindingGeneratorImpl&& other) noexcept;

        void collateSets();
        void parseSpecializationConstants();
        void parseResourceType(const Shader& shader_handle, const VkDescriptorType & type_being_parsed);
        void parseBinary(const Shader& shader_handle);
        void parseImpl(const Shader& shader_handle, const std::vector<uint32_t>& binary_data);

        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::map<uint32_t, SpecializationConstant> specializationConstants;
        std::map<uint32_t, DescriptorSetInfo> sortedSets;
        std::multimap<uint32_t, ResourceUsage> tempResources;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> inputAttributes;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> outputAttributes;
        std::unique_ptr<spirv_cross::Compiler> recompiler{ nullptr };
        size_t getNumSets() const noexcept;

        decltype(sortedSets)::iterator findSetWithIdx(const uint32_t idx);
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_IMPL_HPP