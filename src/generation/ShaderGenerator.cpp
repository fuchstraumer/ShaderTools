#include "generation/ShaderGenerator.hpp"
#include "impl/ShaderGeneratorImpl.hpp"
#include "common/UtilityStructs.hpp"
#include <cassert>
#include "../util/FilesystemUtils.hpp"
#include "easyloggingpp/src/easylogging++.h"
namespace fs = std::filesystem;

namespace st {

    ShaderGenerator::ShaderGenerator(ShaderStage stage) : impl(std::make_unique<ShaderGeneratorImpl>(std::move(stage))) {}

    ShaderGenerator::~ShaderGenerator() {}

    ShaderGenerator::ShaderGenerator(ShaderGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderGenerator& ShaderGenerator::operator=(ShaderGenerator&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    void ShaderGenerator::SetResourceFile(yamlFile* rsrc_file) {
        impl->resourceFile = rsrc_file;
    }

    void ShaderGenerator::Generate(const ShaderStage& handle, const char* path, const size_t num_extensions, const char* const* extensions, const size_t num_includes, const char* const* paths) {
        for (size_t i = 0; i < num_includes; ++i) {
            assert(paths);
            impl->addIncludePath(paths[i]);
        }
        impl->generate(handle, path, num_extensions, extensions);
    }

    void ShaderGenerator::AddIncludePath(const char * path_to_include) {
        impl->addIncludePath(path_to_include);
    }

    void ShaderGenerator::GetFullSource(size_t * len, char * dest) const {
        
        const std::string source = impl->getFullSource();
        *len = source.size();

        if (dest != nullptr) {
            std::copy(source.cbegin(), source.cend(), dest);
        }
        
    }

    ShaderStage ShaderGenerator::SaveCurrentToFile(const char* fname) const {
        
        const std::string source_str = impl->getFullSource();
        std::ofstream output_file(fname);
        if (!output_file.is_open()) {
            LOG(ERROR) << "Could not open file to output completed shader to.";
            throw std::runtime_error("Could not open file to write completed plaintext shader to!");
        }

        fs::path file_path(fs::canonical(fname));
        const std::string path_str = file_path.string();
        
       
        return WriteAndAddShaderSource(fname, source_str, impl->Stage.GetStage());
    }

    VkShaderStageFlagBits ShaderGenerator::GetStage() const {
        return impl->Stage.GetStage();
    }

    void ShaderGenerator::SetBasePath(const char * new_base_path) {
        ShaderGeneratorImpl::BasePath = std::string(new_base_path);
    }

    const char* ShaderGenerator::GetBasePath() {
        return ShaderGeneratorImpl::BasePath.c_str();
    }

}
