#pragma once
#ifndef SHADER_PACK_BINARY_FILE_HPP
#define SHADER_PACK_BINARY_FILE_HPP
#include <cstdint>
#include "common/CommonInclude.hpp"

namespace st
{

    class Shader;
    class ShaderPack;
    class ShaderPackImpl;
    struct ShaderStage;

    /*
        Single shader binary data.

        Saves everything we need to reconstruct 
        the rest. Most important is compiled binary,
        since that takes the longest to create.

        Null-terminated strings used for char*
        arrays.
    */

    struct ShaderBinary;
    struct ShaderPackBinary;

    ST_API ShaderBinary* CreateShaderBinary(const Shader* src);
    void ST_API DestroyShaderBinary(ShaderBinary* binary);
    ST_API ShaderPackBinary* CreateShaderPackBinary(const ShaderPack* src);
    void ST_API DestroyShaderPackBinary(ShaderPackBinary* shader_pack);
    ST_API ShaderPackBinary* LoadShaderPackBinary(const char* fname);
    void ST_API SaveBinaryToFile(ShaderPackBinary* binary, const char* fname);
    void ST_API LoadPackFromBinary(ShaderPackImpl* pack, ShaderPackBinary* bin);

}

#endif //!SHADER_PACK_BINARY_FILE_HPP
