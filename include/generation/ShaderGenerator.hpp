#pragma once
#ifndef SG_SHADER_HPP
#define SG_SHADER_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
namespace st {

    class ShaderGeneratorImpl;

    struct engine_environment_callbacks_t {
        std::add_pointer<int()>::type GetScreenSizeX{ nullptr };
        std::add_pointer<int()>::type GetScreenSizeY{ nullptr };
        std::add_pointer<double()>::type GetZNear{ nullptr };
        std::add_pointer<double()>::type GetZFar{ nullptr };
        std::add_pointer<double()>::type GetFOVY{ nullptr };
    };
    static engine_environment_callbacks_t RetrievalCallbacks;

    class ShaderGenerator { 
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
        Shader SaveCurrentToFile(const char* fname) const;

        VkShaderStageFlagBits GetStage() const;
        static void SetBasePath(const char* new_base_path);
        static const char* GetBasePath();
    private:
        std::unique_ptr<ShaderGeneratorImpl> impl;
    };

}

#endif //!SG_SHADER_HPP