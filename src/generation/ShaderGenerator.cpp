#include "generation/ShaderGenerator.hpp"
#include "impl/ShaderGeneratorImpl.hpp"
#include "common/UtilityStructs.hpp"
#include "common/ShaderToolsErrors.hpp"
#include "../util/FilesystemUtils.hpp"
#include <fstream>

namespace st
{
    namespace fs = std::filesystem;

    ShaderGenerator::ShaderGenerator(ShaderStage stage, Session& error_session) : impl(std::make_unique<ShaderGeneratorImpl>(std::move(stage), error_session)) {}

    ShaderGenerator::~ShaderGenerator() {}

    ShaderGenerator::ShaderGenerator(ShaderGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderGenerator& ShaderGenerator::operator=(ShaderGenerator&& other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }

    void ShaderGenerator::SetResourceFile(yamlFile* rsrc_file)
    {
        impl->resourceFile = rsrc_file;
    }

    ShaderToolsErrorCode ShaderGenerator::Generate(const ShaderStage& handle, const char* path_to_src, const size_t num_extensions, const char* const* extensions, const size_t num_includes, const char* const* paths)
    {
        if (num_includes != 0 && paths == nullptr)
        {
            throw std::runtime_error("Shader generator received nonzero number of include paths, but array of include paths was null");
        }

        for (size_t i = 0; i < num_includes; ++i)
        {
            impl->addIncludePath(paths[i]);
        }

        return impl->generate(handle, path_to_src, num_extensions, extensions);
    }

    void ShaderGenerator::AddIncludePath(const char* path_to_include)
    {
        impl->addIncludePath(path_to_include);
    }

    void ShaderGenerator::GetFullSource(size_t* len, char* dest) const
    {

        const std::string source = impl->getFullSource();
        *len = source.size();

        if (dest != nullptr)
        {
            std::copy(source.cbegin(), source.cend(), dest);
        }

    }

    ShaderStage ShaderGenerator::SaveCurrentToFile(const char* fname) const
    {

        const std::string source_str = impl->getFullSource();
        std::ofstream output_file(fname);

        if (!output_file.is_open())
        {
            throw std::runtime_error("Shader generator could not open file to write completed plaintext shader to");
        }

        return WriteAndAddShaderSource(fname, source_str, impl->Stage.stageBits);
    }

    VkShaderStageFlagBits ShaderGenerator::GetStage() const
    {
        return static_cast<VkShaderStageFlagBits>(impl->Stage.stageBits);
    }

    void ShaderGenerator::SetBasePath(const char* new_base_path)
    {
        ShaderGeneratorImpl::BasePath = std::string(new_base_path);
    }

    const char* ShaderGenerator::GetBasePath()
    {
        return ShaderGeneratorImpl::BasePath.c_str();
    }

}
