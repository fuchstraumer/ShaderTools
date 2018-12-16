#pragma once
#ifndef ST_SHADER_STAGE_PROCESSOR_HPP
#define ST_SHADER_STAGE_PROCESSOR_HPP
#include "common/ShaderStage.hpp"
#include <vector>
#include <string>
#include <memory>
#include <experimental/filesystem>

namespace st {

    class ShaderGeneratorImpl;
    class ShaderCompilerImpl;
    class ResourceFile;

    class ShaderStageProcessor {
    public:

        ShaderStageProcessor(ShaderStage stage, ResourceFile* rfile);
        ~ShaderStageProcessor();

        const std::string& Generate(const std::string& body_path, const std::vector<std::string>& extensions, const std::vector<std::string>& includes);
        const std::vector<uint32_t>& Compile();

    private:

        ShaderStage stage{ 0u };
        ResourceFile* rsrcFile{ nullptr };
        std::unique_ptr<ShaderGeneratorImpl> generator{ nullptr };
        std::unique_ptr<ShaderCompilerImpl> compiler{ nullptr };
    };

}

#endif //!SHADER_STAGE_PROCESSOR_HPP
