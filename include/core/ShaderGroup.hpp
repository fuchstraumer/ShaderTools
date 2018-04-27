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
        void GetVertexAttributes(uint32_t num_bindings, VkVertexInputAttributeDescription* bindings) const;

        static engine_environment_callbacks_t RetrievalCallbacks;
        dll_retrieved_strings_t GetSetResourceNames(const uint32_t set_idx) const;
        dll_retrieved_strings_t GetUsedResourceBlocks(const Shader& handle) const;

        size_t GetMemoryReqForResource(const char* rsrc_name);
        size_t GetNumSetsRequired() const;

    protected:
        friend class ShaderPackImpl;
        BindingGeneratorImpl * GetBindingGeneratorImpl();
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP