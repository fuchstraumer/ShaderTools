#pragma once
#ifndef ST_SHADER_PACK_IMPL_HPP
#define ST_SHADER_PACK_IMPL_HPP
#include "core/Shader.hpp"
#include "generation/ShaderGenerator.hpp"
#include "../../parser/yamlFile.hpp"
#include <filesystem>
#include <future>
#include <memory>

namespace st
{

    class ResourceGroup;
    class ShaderStageProcessor;

    class ShaderPackImpl
    {
    public:
        ShaderPackImpl(const ShaderPackImpl&) = delete;
        ShaderPackImpl& operator=(const ShaderPackImpl&) = delete;

        ShaderPackImpl(const char* shader_pack_file_path);
        ~ShaderPackImpl();

        void createPackScript(const char * fname);

        void processShaderStages();
        void createShaders();
        void createResourceGroups();
        void createSingleGroup(const std::string& name, const std::vector<ShaderStage> shaders);
        void setDescriptorTypeCounts() const;

        std::string packPath;
        std::unordered_map<std::string, std::unique_ptr<Shader>> groups;
        std::unordered_map<ShaderStage, std::future<void>> processorFutures;
        std::unordered_map<ShaderStage, std::unique_ptr<ShaderStageProcessor>> processors;
        std::unique_ptr<yamlFile> filePack{ nullptr };
        std::filesystem::path workingDir;
        std::mutex guardMutex;
        std::unordered_map<std::string, std::unique_ptr<ResourceGroup>> resourceGroups;
        mutable descriptor_type_counts_t typeCounts;
    };

}
#endif //!ST_SHADER_PACK_IMPL_HPP
