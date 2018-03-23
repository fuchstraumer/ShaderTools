#pragma once
#ifndef SG_SHADER_HPP
#define SG_SHADER_HPP
#include "CommonInclude.hpp"

namespace st {
    class ShaderGeneratorImpl;

    class ST_API ShaderGenerator { 
        ShaderGenerator(const ShaderGenerator&) = delete;
        ShaderGenerator& operator=(const ShaderGenerator&) = delete;
    public:
        ShaderGenerator(const VkShaderStageFlagBits& stage = VK_SHADER_STAGE_VERTEX_BIT);
        ~ShaderGenerator();
        ShaderGenerator(ShaderGenerator&& other) noexcept;
        ShaderGenerator& operator=(ShaderGenerator&& other) noexcept;

        void AddResources(const char* path_to_resource_file);
        void AddBody(const char* path_to_src, const size_t num_includes = 0, const char* const* paths = nullptr);
        void AddIncludePath(const char* path_to_include);
        void GetFullSource(size_t* len, char* dest) const;
        uint32_t SaveCurrentToFile(const char* fname) const;

        VkShaderStageFlagBits GetStage() const;
        static const char* const BasePath;
        static const char* const LibPath;
    private:
        std::unique_ptr<ShaderGeneratorImpl> impl;
    };

}

#endif //!SG_SHADER_HPP