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

        void Process(std::string body_path, const std::vector<std::string>& extensions, const std::vector<std::string>& includes);
        const std::string& Generate(const std::string& body_path, const std::vector<std::string>& extensions, const std::vector<std::string>& includes);
        const std::vector<uint32_t>& Compile();

        // Each processor has it's own session, so it can collect results on the thread it
        // executes on and the pack launching these will merge them later
        Session ErrorSession;
        
    private:
    
        ShaderStage stage{ 0u, 0u };
        yamlFile* rsrcFile{ nullptr };
        std::string bodyPath;
        std::unique_ptr<ShaderGeneratorImpl> generator{ nullptr };
        std::unique_ptr<ShaderCompilerImpl> compiler{ nullptr };

    };

}

#endif //!SHADER_STAGE_PROCESSOR_HPP
