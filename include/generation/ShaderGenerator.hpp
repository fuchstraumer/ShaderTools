#pragma once
#ifndef SG_SHADER_HPP
#define SG_SHADER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
namespace st {

    class ShaderGeneratorImpl;
    class ResourceFile;

    class ST_API ShaderGenerator { 
        ShaderGenerator(const ShaderGenerator&) = delete;
        ShaderGenerator& operator=(const ShaderGenerator&) = delete;
    public:
        ShaderGenerator(const VkShaderStageFlagBits& stage = VK_SHADER_STAGE_VERTEX_BIT);
        ~ShaderGenerator();
        ShaderGenerator(ShaderGenerator&& other) noexcept;
        ShaderGenerator& operator=(ShaderGenerator&& other) noexcept;

        void SetResourceFile(ResourceFile* rsrc_file);
        void Generate(const ShaderStage& handle, const char* path_to_src, const size_t num_extensions, const char* const* extensions, 
            const size_t num_includes, const char* const* paths);
        void AddIncludePath(const char* path_to_include);
        void GetFullSource(size_t* len, char* dest) const;
        ShaderStage SaveCurrentToFile(const char* fname) const;

        VkShaderStageFlagBits GetStage() const;
        static void SetBasePath(const char* new_base_path);
        static const char* GetBasePath();
    private:
        std::unique_ptr<ShaderGeneratorImpl> impl;
    };

}

#endif //!SG_SHADER_HPP
