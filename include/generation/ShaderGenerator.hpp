#pragma once
#ifndef SHADERTOOLS_SHADER_GENERATOR_HPP
#define SHADERTOOLS_SHADER_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"

namespace st
{

    class ShaderGeneratorImpl;
    struct yamlFile;
    struct Session;

    /**
     * @brief Takes the incomplete abstracted source code provided by the YAML file/user, and generates a valid shader source string from it.
     * Substitutes in includes, extensions, preprocessor directives, and the actual resource declarations given the USE_RESOURCE macros it detects in the source code. Also inserts or handles
     * vertex interfaces if they're used by the shader.
     * 
     * @note This is effectively an internal class that end users should not need to use directly, but I have added some documentation to provide insight on it's functionality.
     * @ingroup Generation
     */
    class ST_API ShaderGenerator
    {
        ShaderGenerator(const ShaderGenerator&) = delete;
        ShaderGenerator& operator=(const ShaderGenerator&) = delete;
    public:

        ShaderGenerator(ShaderStage stage, Session& error_session);
        ~ShaderGenerator();
        ShaderGenerator(ShaderGenerator&& other) noexcept;
        ShaderGenerator& operator=(ShaderGenerator&& other) noexcept;

        /**
         * @brief Sets the YAML file to pull extra data from when generating the given ShaderStage into a valid source string
         */
        void SetResourceFile(yamlFile* rsrc_file);
        ShaderToolsErrorCode Generate(
            const ShaderStage& handle,
            const char* path_to_src,
            const size_t num_extensions, const char* const* extensions,
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

#endif //!SHADERTOOLS_SHADER_GENERATOR_HPP
