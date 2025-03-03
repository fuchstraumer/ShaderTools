#pragma once
#ifndef ST_SHADER_STAGE_PROCESSOR_HPP
#define ST_SHADER_STAGE_PROCESSOR_HPP
#include "common/ShaderStage.hpp"
#include "common/stSession.hpp"
#include <vector>
#include <string>
#include <memory>

namespace st
{

    class ShaderGeneratorImpl;
    class ShaderCompilerImpl;
    struct yamlFile;

    class ShaderStageProcessor
    {
    public:

        ShaderStageProcessor(ShaderStage stage, yamlFile* yfile);
        ~ShaderStageProcessor();

        ShaderStageProcessor(const ShaderStageProcessor&) = delete;
        ShaderStageProcessor& operator=(const ShaderStageProcessor&) = delete;

        void Process(
            std::string shader_name,
            std::string body_path,
            std::vector<std::string> extensions,
            std::vector<std::string> includes);

        [[nodiscard]] std::string Generate(
            const std::string& body_path,
            const std::vector<std::string>& extensions,
            const std::vector<std::string>& includes);

        [[nodiscard]] std::vector<uint32_t> Compile(
            std::string shader_name,
            std::string full_source_string);

        // Each processor has it's own session, so it can collect results on the thread it
        // executes on and the pack launching these will merge them later
        Session ErrorSession;

    private:

        ShaderStage stage{ 0u, 0u };
        yamlFile* rsrcFile{ nullptr };
        std::string bodyPath;
        std::unique_ptr<ShaderGeneratorImpl> generator;
        std::unique_ptr<ShaderCompilerImpl> compiler;

    };

}

#endif //!SHADER_STAGE_PROCESSOR_HPP
