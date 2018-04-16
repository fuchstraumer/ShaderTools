#pragma once
#ifndef SG_SHADER_HPP
#define SG_SHADER_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
namespace st {

    class ShaderGeneratorImpl;
    class ResourceFile;


    class ShaderGenerator { 
        ShaderGenerator(const ShaderGenerator&) = delete;
        ShaderGenerator& operator=(const ShaderGenerator&) = delete;
    public:
        ShaderGenerator(const VkShaderStageFlagBits& stage = VK_SHADER_STAGE_VERTEX_BIT);
        ~ShaderGenerator();
        ShaderGenerator(ShaderGenerator&& other) noexcept;
        ShaderGenerator& operator=(ShaderGenerator&& other) noexcept;

        void AddResources(ResourceFile* rsrc_file);
        void AddBody(const char* path_to_src, const size_t num_includes = 0, const char* const* paths = nullptr);
        void AddIncludePath(const char* path_to_include);
        void GetFullSource(size_t* len, char* dest) const;
        Shader SaveCurrentToFile(const char* fname) const;

        VkShaderStageFlagBits GetStage() const;
        static void SetBasePath(const char* new_base_path);
        static const char* GetBasePath();
    private:
        std::unique_ptr<ShaderGeneratorImpl> impl;
    };

}

#endif //!SG_SHADER_HPP