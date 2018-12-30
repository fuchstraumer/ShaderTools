#pragma once
#ifndef VPSK_SHADER_GROUP_HPP
#define VPSK_SHADER_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/UtilityStructs.hpp"
#include "reflection/ReflectionStructs.hpp"
namespace st {

    class ShaderGroupImpl;
    class ShaderPackImpl;
    class ShaderReflectorImpl;
    class ResourceUsage;

    /*  Designed to be used to group shaders into the groups that they are used in
        when bound to a pipeline, to simplify a few key things.
    */
    class ST_API Shader {
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
    public:

        Shader(const char* group_name, const size_t num_stages, const ShaderStage* stages, const char* resource_file_path);
        ~Shader();
        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;

        ShaderStage AddShaderStage(const char* shader_name,const VkShaderStageFlagBits& flags);
        
        void GetInputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const;
        void GetOutputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const;
        PushConstantInfo GetPushConstantInfo(const VkShaderStageFlags stage) const;
        void GetShaderStages(size_t* num_stages, ShaderStage* stages) const;
        void GetShaderBinary(const ShaderStage& handle, size_t* binary_size, uint32_t* dest_binary_ptr) const;
        void GetSetLayoutBindings(const size_t& set_idx, size_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        void GetSpecializationConstants(size_t* num_constants, SpecializationConstant* constants) const;
        void GetResourceUsages(const size_t& set_idx, size_t* num_resources, ResourceUsage* resources) const;
        VkShaderStageFlags Stages() const noexcept;
        bool OptimizationEnabled(const ShaderStage& handle) const noexcept;
        uint32_t ResourceGroupSetIdx(const char* name) const;

        dll_retrieved_strings_t GetTags() const;
        dll_retrieved_strings_t GetSetResourceNames(const uint32_t set_idx) const;
        dll_retrieved_strings_t GetUsedResourceBlocks() const;
        size_t GetNumSetsRequired() const;
        size_t GetIndex() const noexcept;

        void SetIndex(size_t _idx);
        void SetTags(const size_t num_tags, const char** tag_strings);

    protected:
        friend class ShaderPackImpl;
        ShaderReflectorImpl* GetShaderReflectorImpl();
        const ShaderReflectorImpl* GetShaderReflectorImpl() const;
        Shader(const char * group_name, const size_t num_extensions, const char * const * extensions, const size_t num_includes, const char * const * paths);
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP
