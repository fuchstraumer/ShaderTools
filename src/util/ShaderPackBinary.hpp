#pragma once
#ifndef SHADER_PACK_BINARY_FILE_HPP
#define SHADER_PACK_BINARY_FILE_HPP
#include "ShaderStage.hpp"
#include <cstdint>

namespace st {

    /*
        Single shader binary data.

        Saves everything we need to reconstruct 
        the rest. Most important is compiled binary,
        since that takes the longest to create.

        Null-terminated strings used for char*
        arrays.
    */

    struct ShaderBinary {
        uint32_t ShaderBinaryMagic;
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

    struct ShaderPackBinary {
        uint64_t MagicBits;
        uint64_t ShaderToolsVersion;
        uint32_t PackPathLength;
        char* PackPath;
        uint32_t ResourceScriptPathLength;
        char* ResourceScriptPath;
        uint32_t* NumShaders;
        // Where ShaderBinary entries begin
        uint64_t* OffsetsToShaders;
        ShaderBinary* Shaders;
    };

    ShaderPackBinary* LoadShaderPackBinary(const char* fname);
    void DestroyShaderPackBinary(ShaderPackBinary* shader_pack);
    void SaveBinaryToFile(const char* fname);

}

#endif //!SHADER_PACK_BINARY_FILE_HPP
