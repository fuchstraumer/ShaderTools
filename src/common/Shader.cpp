#include "common/Shader.hpp"
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
namespace st {

    namespace fs = std::experimental::filesystem;
    inline uint64_t GetShaderHash(const std::string& name, const VkShaderStageFlagBits stage) {
        namespace fs = std::experimental::filesystem;
        const uint64_t base_hash = static_cast<uint64_t>(std::hash<std::string>()(name));
        const uint32_t stage_bits(stage);
        return (base_hash << 32) | (stage_bits);
    }


    Shader::Shader(const char * shader_name, const VkShaderStageFlagBits stages) : ID(GetShaderHash(shader_name, stages)) {}

    void Shader::addSource(const char * source_path) {
        std::ifstream input_file(source_path);

        if (!input_file.is_open()) {
            std::cerr << "Failed to open input file for reading shader source file.\n";
            throw std::runtime_error("Failed to open input file.");
        }

    }

    Shader::~Shader() {}

    VkShaderStageFlagBits Shader::getStage() const noexcept {
        return VkShaderStageFlagBits((uint32_t)ID);
    }

    bool Shader::operator==(const Shader & other) const noexcept {
        return impl->ID == other.GetID();
    }

}

