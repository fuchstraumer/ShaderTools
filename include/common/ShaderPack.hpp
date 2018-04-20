#pragma once 
#ifndef ST_SHADER_PACK_HPP
#define ST_SHADER_PACK_HPP
#include "CommonInclude.hpp"
namespace st {

    class ShaderPackImpl;
    class ShaderGroup;

    static void SetCacheDirectory(const char* new_cache_directory);
    static const char* GetCacheDirectory();

    class ST_API ShaderPack {
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;
    public:

        ShaderPack(const char* shader_pack_lua_script_path);
        ~ShaderPack();

        
        ShaderGroup* GetShaderGroup(const char* name) const;
    private:
        std::unique_ptr<ShaderPackImpl> impl;
    };

}

#endif //ST_SHADER_PACK_HPP