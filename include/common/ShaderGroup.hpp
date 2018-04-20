#pragma once
#ifndef VPSK_SHADER_GROUP_HPP
#define VPSK_SHADER_GROUP_HPP
#include "CommonInclude.hpp"
#include "Shader.hpp"

namespace st {

    class ShaderGroupImpl;


    struct engine_environment_callbacks_t {
        std::add_pointer<int()>::type GetScreenSizeX{ nullptr };
        std::add_pointer<int()>::type GetScreenSizeY{ nullptr };
        std::add_pointer<double()>::type GetZNear{ nullptr };
        std::add_pointer<double()>::type GetZFar{ nullptr };
        std::add_pointer<double()>::type GetFOVY{ nullptr };
    };


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
        void GetSetLayoutBindings(const uint32_t set_idx, uint32_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        
        struct dll_retrieved_strings_t {
            dll_retrieved_strings_t(const dll_retrieved_strings_t&) = delete;
            dll_retrieved_strings_t& operator=(const dll_retrieved_strings_t&) = delete;
            // Names are retrieved using strdup(), so we need to free the duplicated names once done with them.
            // Use this structure to "buffer" the names, and copy them over.
            // Once this structure exits scope the memory should be cleaned up.
            dll_retrieved_strings_t();
            ~dll_retrieved_strings_t();
            dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept;
            dll_retrieved_strings_t& operator=(dll_retrieved_strings_t&& other) noexcept;
            char** Strings{ nullptr };
            uint32_t NumNames{ 0 };
        };

        static engine_environment_callbacks_t RetrievalCallbacks;
        dll_retrieved_strings_t GetSetResourceNames(const uint32_t set_idx) const;
        dll_retrieved_strings_t GetUsedResourceBlocks(const Shader& handle) const;

        size_t GetMemoryReqForResource(const char* rsrc_name);
        size_t GetNumSetsRequired() const;
        
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP