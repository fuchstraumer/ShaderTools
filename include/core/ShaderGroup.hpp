#pragma once
#ifndef VPSK_SHADER_GROUP_HPP
#define VPSK_SHADER_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
#include "common/UtilityStructs.hpp"
namespace st {

    class ShaderGroupImpl;
    class ShaderPackImpl;
    class BindingGeneratorImpl;
    class ResourceUsage;

    /*  Designed to be used to group shaders into the groups that they are used in
        when bound to a pipeline, to simplify a few key things.
    */
    class ST_API ShaderGroup {
        ShaderGroup(const ShaderGroup&) = delete;
        ShaderGroup& operator=(const ShaderGroup&) = delete;
    public:

        ShaderGroup(const char* group_name, const char* resource_file_path, const size_t num_includes = 0, const char* const* paths = nullptr);
        ~ShaderGroup();
        ShaderGroup(ShaderGroup&& other) noexcept;
        ShaderGroup& operator=(ShaderGroup&& other) noexcept;

        Shader AddShader(const char* shader_name, const char* body_src_file_path, const VkShaderStageFlagBits& flags);
        
        void GetShaderBinary(const Shader& handle, size_t* binary_size, uint32_t* dest_binary_ptr) const;
        void GetVertexAttributes(size_t* num_attributes, VkVertexInputAttributeDescription* attributes) const;
        void GetSetLayoutBindings(const size_t& set_idx, size_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        void GetSpecializationConstants(size_t* num_constants, SpecializationConstant* constants) const;
        void GetResourceUsages(const size_t& set_idx, size_t* num_resources, ResourceUsage* resources) const;

        dll_retrieved_strings_t GetSetResourceNames(const uint32_t set_idx) const;
        dll_retrieved_strings_t GetUsedResourceBlocks() const;
        size_t GetNumSetsRequired() const;
        size_t GetIndex() const noexcept;
        void SetIndex(size_t _idx);

    protected:
        friend class ShaderPackImpl;
        BindingGeneratorImpl * GetBindingGeneratorImpl();
        const BindingGeneratorImpl * GetBindingGeneratorImpl() const;
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP
