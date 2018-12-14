#pragma once
#ifndef ST_SHADER_PACK_IMPL_HPP
#define ST_SHADER_PACK_IMPL_HPP
#include "core/Shader.hpp"
#include "core/ShaderResource.hpp"
#include "generation/ShaderGenerator.hpp"
#include "shader_pack_file.hpp"
#include <unordered_map>
#include <experimental/filesystem>
#include <future>
#include <memory>

namespace st {

    class ResourceFile;
    class ResourceGroup;

    class ShaderPackImpl {
    public:
        ShaderPackImpl(const ShaderPackImpl&) = delete;
        ShaderPackImpl& operator=(const ShaderPackImpl&) = delete;

        ShaderPackImpl(const char* shader_pack_file_path);
        void createShaders();
        void createResourceGroups();
        void createSingleGroup(const std::string& name, const std::map<VkShaderStageFlagBits, std::string>& shader_map);
        void setDescriptorTypeCounts() const;

        std::string packScriptPath;
        std::string resourceScriptPath;
        std::unordered_map<std::string, std::unique_ptr<Shader>> groups;
        std::unique_ptr<shader_pack_file_t> filePack;
        std::experimental::filesystem::path workingDir;
        std::mutex guardMutex;
        std::unordered_map<std::string, std::unique_ptr<ResourceGroup>> resourceGroups;
        mutable descriptor_type_counts_t typeCounts;
    private:
        ResourceFile* rsrcFile;
    };

}
#endif //!ST_SHADER_PACK_IMPL_HPP
