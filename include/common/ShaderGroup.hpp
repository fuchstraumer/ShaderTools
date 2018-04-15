#pragma once
#ifndef VPSK_SHADER_GROUP_HPP
#define VPSK_SHADER_GROUP_HPP
#include "CommonInclude.hpp"
#include "Shader.hpp"


namespace st {

    class ShaderGroupImpl;

    /*  Designed to be used to group shaders into the groups that they are used in
        when bound to a pipeline, to simplify a few key things.
    */
    class ShaderGroup {
        ShaderGroup(const ShaderGroup&) = delete;
        ShaderGroup& operator=(const ShaderGroup&) = delete;
    public:

        ShaderGroup();
        ~ShaderGroup();
        ShaderGroup(ShaderGroup&& other) noexcept;
        ShaderGroup& operator=(ShaderGroup&& other) noexcept;


        void AddShader(const char* fname, VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
        void AddShader(const char* shader_name, const char* src_str, const uint32_t src_str_len, const VkShaderStageFlagBits stage);

        void GetVertexAttributes(uint32_t num_bindings, VkVertexInputAttributeDescription* bindings) const;
        void GetSetLayoutBindings(const uint32_t set_idx, uint32_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        
        struct shader_resource_names_t {
            shader_resource_names_t(const shader_resource_names_t&) = delete;
            shader_resource_names_t& operator=(const shader_resource_names_t&) = delete;
            // Names are retrieved using strdup(), so we need to free the duplicated names once done with them.
            // Use this structure to "buffer" the names, and copy them over.
            // Once this structure exits scope the memory should be cleaned up.
            shader_resource_names_t();
            ~shader_resource_names_t();
            shader_resource_names_t(shader_resource_names_t&& other) noexcept;
            shader_resource_names_t& operator=(shader_resource_names_t&& other) noexcept;
            char** Names{ nullptr };
            uint32_t NumNames{ 0 };
        };

        shader_resource_names_t GetSetResourceNames(const uint32_t set_idx) const;

        size_t GetMemoryReqForResource(const char* rsrc_name);
        size_t GetNumSetsRequired() const;
        
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP