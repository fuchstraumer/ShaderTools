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

        const ShaderGroup* GetShaderGroup(const char* name) const;
        dll_retrieved_strings_t GetShaderGroupNames() const;
        dll_retrieved_strings_t GetResourceGroupNames() const;
        const descriptor_type_counts_t& GetTotalDescriptorTypeCounts() const;

        void GetResourceGroupPointers(const char* group_name, size_t* num_resources, const ShaderResource** pointers) const;
        void CopyShaderResources(const char* group_name, size_t* num_resources, ShaderResource* dest_array) const;
        void GetGroupSpecializationConstants(const char* group_name, size_t* num_spcs, SpecializationConstant* constants) const;
        const ShaderResource* GetResource(const char* rsrc_name) const;

        static engine_environment_callbacks_t& RetrievalCallbacks();

    private:
        std::unique_ptr<ShaderPackImpl> impl;
    };

}

#endif //ST_SHADER_PACK_HPP