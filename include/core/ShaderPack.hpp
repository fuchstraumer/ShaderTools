#pragma once 
#ifndef ST_SHADER_PACK_HPP
#define ST_SHADER_PACK_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
namespace st {

    class ShaderPackImpl;
    class ShaderGroup;
    class ShaderResource;

    static void SetCacheDirectory(const char* new_cache_directory);
    static const char* GetCacheDirectory();

    class ST_API ShaderPack {
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;
    public:

        ShaderPack(const char* shader_pack_lua_script_path);
        ~ShaderPack();

        ShaderGroup* GetShaderGroup(const char* name) const;
        dll_retrieved_strings_t GetShaderGroupNames() const;
        dll_retrieved_strings_t GetResourceGroupNames() const;
        void GetResourceGroupPointers(const char* name, size_t* num_resources, const ShaderResource** pointers);
        void CopyShaderResources(const char* name, size_t* num_resources, ShaderResource* dest_array);
        const descriptor_type_counts_t& GetTotalDescriptorTypeCounts() const;
        descriptor_type_counts_t GetSingleSetTypeCounts() const;
        const ShaderResource* GetResource(const char* rsrc_name);

        static engine_environment_callbacks_t& RetrievalCallbacks();

    private:
        std::unique_ptr<ShaderPackImpl> impl;
    };

}

#endif //ST_SHADER_PACK_HPP