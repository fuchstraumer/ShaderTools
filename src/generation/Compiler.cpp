#include "generation/Compiler.hpp"
#include <mutex>
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include "shaderc/shaderc.hpp"
#include "spirv_glsl.hpp"
#include "../util/FilesystemUtils.hpp"
#include "../util/ShaderFileTracker.hpp"
#define SCL_SECURE_NO_WARNINGS
namespace fs = std::experimental::filesystem;

namespace st {

    extern ShaderFileTracker FileTracker;

    class ShaderCompilerImpl {
        ShaderCompilerImpl(const ShaderCompilerImpl&) = delete;
        ShaderCompilerImpl& operator=(const ShaderCompilerImpl&) = delete;
    public:

        ShaderCompilerImpl() = default;
        ~ShaderCompilerImpl() = default;
        ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept = default;
        ShaderCompilerImpl& operator=(ShaderCompilerImpl&& other) noexcept = default;

        shaderc::CompileOptions getCompilerOptions() const;
        shaderc_shader_kind getShaderKind(const VkShaderStageFlagBits & flags) const;
        void compile(const Shader& handle, const char* path_to_src);
        void compile(const Shader& handle, const std::string& name, const std::string& src);

    };

    ShaderCompiler::ShaderCompiler() : impl(std::make_unique<ShaderCompilerImpl>()) {}

    ShaderCompiler::~ShaderCompiler() {}

    ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    void ShaderCompiler::Compile(const Shader& handle, const char * shader_name, const char * src_str, const size_t src_len) {
        impl->compile(handle, shader_name, std::string{ src_str, src_str + src_len });
    }

    void ShaderCompiler::Compile(const Shader& handle, const char* path_to_source_str) {
        impl->compile(handle, path_to_source_str);
    }

    void ShaderCompiler::GetBinary(const Shader & shader_handle, size_t * binary_sz, uint32_t * binary_dest_ptr) const {
        std::vector<uint32_t> binary_vec;
        if (FileTracker.FindShaderBinary(shader_handle, binary_vec)) {
            *binary_sz = binary_vec.size();
            if (binary_dest_ptr != nullptr) {
                std::copy(binary_vec.begin(), binary_vec.end(), binary_dest_ptr);
            }
        }
        else {
            *binary_sz = 0;
        }
    } 

    shaderc::CompileOptions ShaderCompilerImpl::getCompilerOptions() const {
        shaderc::CompileOptions options;
#ifndef NDEBUG
        options.SetGenerateDebugInfo();
#endif
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
        options.SetSourceLanguage(shaderc_source_language_glsl);
    }

    shaderc_shader_kind ShaderCompilerImpl::getShaderKind(const VkShaderStageFlagBits& flags) const {
        switch (flags) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return shaderc_glsl_vertex_shader;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return shaderc_glsl_fragment_shader;
            // MoltenVK cannot yet use the geometry or tesselation shaders.
#ifndef __APPLE__ 
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return shaderc_glsl_default_geometry_shader;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return shaderc_glsl_tess_control_shader;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return shaderc_glsl_tess_evaluation_shader;
#endif 
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return shaderc_glsl_compute_shader;
        default:
            throw std::domain_error("Invalid shader stage bitfield, or shader stage not supported on current platform!");
        }
    }

    void ShaderCompilerImpl::compile(const Shader& handle, const std::string& name, const std::string& src) {

        shaderc::Compiler compiler;

        const auto options = getCompilerOptions();
        const auto shader_stage = getShaderKind(handle.GetStage());
        
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(src, shader_stage, name.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        FileTracker.Binaries.emplace(handle, std::vector<uint32_t>{result.begin(), result.end()});
    }

    void ShaderCompilerImpl::compile(const Shader& handle, const char* path_to_source_str) {
        fs::path path_to_source(path_to_source_str);

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source)) {
            std::cerr << "Given shader source path/file does not exist!\n";
            throw std::runtime_error("Failed to open/find given shader file.");
        }

        shaderc::Compiler compiler;
        const auto options = getCompilerOptions();
        const auto shader_stage = getShaderKind(handle.GetStage());

        std::ifstream input_file(path_to_source);
        if (!input_file.is_open()) {
            throw std::runtime_error("Failed to open supplied file in GLSLCompiler!");
        }
        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().string();

        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, shader_stage, file_name.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        FileTracker.Binaries.emplace(handle, std::vector<uint32_t>{ result.cbegin(), result.cend() });
    }


}