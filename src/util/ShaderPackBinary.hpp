#pragma once
#ifndef SHADER_PACK_BINARY_FILE_HPP
#define SHADER_PACK_BINARY_FILE_HPP
#include <cstdint>
#include "common/CommonInclude.hpp"

namespace st {

    class Shader;
    class ShaderPack;

    /*
        Single shader binary data.

        Saves everything we need to reconstruct 
        the rest. Most important is compiled binary,
        since that takes the longest to create.

        Null-terminated strings used for char*
        arrays.
    */

    constexpr static uint32_t SHADER_BINARY_MAGIC_VALUE{ 0x70cd20ae };

    struct ST_API ShaderBinary {
        uint32_t ShaderBinaryMagic{ SHADER_BINARY_MAGIC_VALUE };
        uint64_t TotalLength;
        uint32_t NumShaderStages;
        uint64_t* StageIDs;
        uint64_t* LastWriteTimes;
        uint32_t* PathLengths;
        char* Paths;
        uint32_t* SrcStringLengths;
        char* SourceStrings;
        uint32_t* BinaryLengths;
        uint32_t* Binaries;
    };

    // Need to identify how to store resource metadata
    struct ST_API ResourceScriptBinary {

    };

    constexpr static uint32_t SHADER_PACK_BINARY_MAGIC_VALUE{ 0x9db3bb66 };

    struct ST_API ShaderPackBinary {
        uint32_t MagicBits{ SHADER_PACK_BINARY_MAGIC_VALUE };
        uint32_t ShaderToolsVersion;
        uint32_t TotalLength;
        uint32_t PackPathLength;
        char* PackPath{ nullptr };
        uint32_t ResourceScriptPathLength;
        char* ResourceScriptPath{ nullptr };
        uint32_t NumShaders;
        // Where ShaderBinary entries begin
        uint64_t* OffsetsToShaders;
        ShaderBinary* Shaders;
    };

    void ST_API CreateShaderBinary(const Shader* src, ShaderBinary* binary_dst);
    void ST_API DestroyShaderBinary(ShaderBinary* binary);
    void ST_API CreateShaderPackBinary(const ShaderPack* src, ShaderPackBinary* binary_dst);
    void ST_API DestroyShaderPackBinary(ShaderPackBinary* shader_pack);
    ST_API ShaderPackBinary* LoadShaderPackBinary(const char* fname);
    void ST_API SaveBinaryToFile(ShaderPackBinary* binary, const char* fname);

}

#endif //!SHADER_PACK_BINARY_FILE_HPP
