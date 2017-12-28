#include "Compiler.hpp"
#include <shaderc/shaderc.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>

namespace fs = std::experimental::filesystem;

namespace st {

    const std::vector<uint32_t>& ShaderCompiler::Compile(const std::string& path_to_source_str, const VkShaderStageFlags& stage) {
        fs::path path_to_source(path_to_source_str);
        if(compiledShaders.count(path_to_source) != 0) {
            return compiledShaders.at(path_to_source);
        }

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source)) {
            std::cerr << "Given shader source path/file does not exist!\n";
            throw std::runtime_error("Failed to open/find given shader file.");
        }
        
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, 1);
        options.SetSourceLanguage(shaderc_source_language_glsl);

        shaderc_shader_kind shader_stage;
        switch(stage) {
            case VK_SHADER_STAGE_VERTEX_BIT:
                shader_stage = shaderc_glsl_vertex_shader;
                break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                shader_stage = shaderc_glsl_fragment_shader;
                break;
// MoltenVK cannot yet use the geometry or tesselation shaders.
#ifndef __APPLE__ 
            case VK_SHADER_STAGE_GEOMETRY_BIT:
                shader_stage = shaderc_glsl_default_geometry_shader;
                break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                shader_stage = shaderc_glsl_tess_control_shader;
                break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                shader_stage = shaderc_glsl_tess_evaluation_shader;
                break;
#endif 
            case VK_SHADER_STAGE_COMPUTE_BIT:
                shader_stage = shaderc_glsl_compute_shader;
                break;
            default:
                throw std::domain_error("Invalid shader stage bitfield, or shader stage not supported on current platform!");
        }

        std::ifstream input_file(path_to_source);
        if(!input_file.is_open()) {
            throw std::runtime_error("Failed to open supplied file in GLSLCompiler!");
        }
        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().string();
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, shader_stage, file_name.c_str());
        if(result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard(insertion_mutex);
        auto inserted = compiledShaders.insert(std::make_pair(fs::absolute(path_to_source), std::vector<uint32_t>{ result.begin(), result.end() }));
        if(!inserted.second) {
            throw std::runtime_error("Failed to insert shader into compiled shader map!");
        }
        return (inserted.first)->second;   
    }

    bool ShaderCompiler::HasShader(const std::string & binary_path) const {
        const fs::path absolute_path = fs::absolute(fs::path(binary_path));
        return compiledShaders.count(absolute_path) != 0;
    }

    const std::vector<uint32_t>& ShaderCompiler::GetBinary(const std::string & binary_path) const {
        if (!HasShader(binary_path)) {
            throw std::runtime_error("Requested binary not found!");
        }
        const fs::path absolute_path = fs::absolute(fs::path(binary_path));
        return compiledShaders.at(absolute_path);
    }

    void ShaderCompiler::AddBinary(const std::string & path, std::vector<uint32_t> binary_data) {
        const fs::path absolute_path = fs::absolute(fs::path(path));
        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard(insertion_mutex);
        auto inserted = compiledShaders.insert(std::make_pair(absolute_path, std::move(binary_data)));
        if (!inserted.second) {
            throw std::runtime_error("Tried to insert already-stored shader binary!");
        }
    }

}