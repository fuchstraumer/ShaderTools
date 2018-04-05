#include "Compiler.hpp"
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
#include "FilesystemUtils.hpp"
#define SCL_SECURE_NO_WARNINGS
namespace fs = std::experimental::filesystem;

namespace st {

    extern std::unordered_map<Shader, std::string> shaderFiles;
    extern std::unordered_map<Shader, std::vector<uint32_t>> shaderBinaries;
    extern std::unordered_multimap<Shader, fs::path> shaderPaths;

    static const std::map<std::string, VkShaderStageFlagBits> extension_stage_map {
        { ".vert", VK_SHADER_STAGE_VERTEX_BIT },
        { ".frag", VK_SHADER_STAGE_FRAGMENT_BIT },
        { ".geom", VK_SHADER_STAGE_GEOMETRY_BIT },
        { ".teval", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
        { ".tcntl", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { ".comp", VK_SHADER_STAGE_COMPUTE_BIT }
    };

    class ShaderCompilerImpl {
        ShaderCompilerImpl(const ShaderCompilerImpl&) = delete;
        ShaderCompilerImpl& operator=(const ShaderCompilerImpl&) = delete;
    public:

        ShaderCompilerImpl() = default;
        ~ShaderCompilerImpl() = default;
        ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept = default;
        ShaderCompilerImpl& operator=(ShaderCompilerImpl&& other) noexcept = default;

        Shader compile(const char* path_to_src, const VkShaderStageFlagBits stage);
        Shader compile(const char* name, const char* source_ptr, const size_t src_len, const VkShaderStageFlagBits stage);

        VkShaderStageFlagBits getStage(const char* path) const;
    };

    ShaderCompiler::ShaderCompiler() : impl(std::make_unique<ShaderCompilerImpl>()) {}

    ShaderCompiler::~ShaderCompiler() {}

    ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    Shader ShaderCompiler::Compile(const char* path_to_source_str, const VkShaderStageFlagBits stage) {
        return impl->compile(path_to_source_str, stage);
    }

    Shader ShaderCompiler::Compile(const char* name, const char* src, const size_t len, const VkShaderStageFlagBits stage) {
        return impl->compile(name, src, len, stage);
    }

    VkShaderStageFlags ShaderCompiler::GetShaderStage(const char* path_to_source) const {
        return impl->getStage(path_to_source);
    }

    bool ShaderCompiler::HasShader(const Shader& shader) const {
        return shaderBinaries.count(shader) != 0;
    }

    void ShaderCompiler::GetBinary(const Shader& shader, uint32_t* binary_size, uint32_t* binary_src) const {
        if (!HasShader(shader)) {
            *binary_size = 0;
            std::cerr << "Requested binary does not exist.";
        }
        else {
            const auto& binary = shaderBinaries.at(shader);
            *binary_size = static_cast<uint32_t>(binary.size());
            if (binary_src != nullptr) {
                std::copy(binary.cbegin(), binary.cend(), binary_src);
                return;
            }
        }

    }

    void ShaderCompiler::AddBinary(const char* name, const uint32_t sz, const uint32_t* binary_data, const VkShaderStageFlagBits stage) {
        WriteAndAddShaderBinary(name, std::vector<uint32_t>{ binary_data, binary_data + sz }, stage);
    }

    void ShaderCompiler::SaveBinaryBackToText(const Shader & shader_to_save, const char * fname) const {
        if (shaderBinaries.count(shader_to_save) == 0) {
            return;
        }

        const auto& binary_data = shaderBinaries.at(shader_to_save);
        spirv_cross::CompilerGLSL glsl_compiler(binary_data);

        const std::string source = glsl_compiler.compile();

        std::ofstream output_stream(fname);

        if (!output_stream.is_open()) {
            throw std::runtime_error("Failed to open output stream.");
        }

        output_stream << source;
        output_stream.close();
    }

    Shader ShaderCompilerImpl::compile(const char* name, const char* src_str, const size_t src_len, const VkShaderStageFlagBits stage) {

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_size);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
        options.SetSourceLanguage(shaderc_source_language_glsl);

        shaderc_shader_kind shader_stage;
        switch (stage) {
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

        const std::string source_string{ src_str, src_str + src_len };
        Shader shader_handle = WriteAndAddShaderSource(std::string(name), source_string, stage);
        
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_string, shader_stage, name);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        WriteAndAddShaderBinary(name, std::vector<uint32_t>{ result.begin(), result.end() }, stage);
        return shader_handle;
    }

    Shader ShaderCompilerImpl::compile(const char* path_to_source_str, VkShaderStageFlagBits stage) {
        fs::path path_to_source(path_to_source_str);

        if (stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
            stage = getStage(path_to_source_str);
        }

        Shader shader_handle(path_to_source_str, stage);
        if (shaderBinaries.count(shader_handle) != 0) {      
            return shader_handle;
        }

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source)) {
            std::cerr << "Given shader source path/file does not exist!\n";
            throw std::runtime_error("Failed to open/find given shader file.");
        }

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_size);
        options.SetTargetEnvironment(shaderc_target_env_opengl, 0);
        options.SetSourceLanguage(shaderc_source_language_glsl);

        shaderc_shader_kind shader_stage;
        switch (stage) {
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
        if (!input_file.is_open()) {
            throw std::runtime_error("Failed to open supplied file in GLSLCompiler!");
        }
        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().string();
        Shader source_hash = WriteAndAddShaderSource(file_name, source_code, stage);

        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, shader_stage, file_name.c_str());
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        WriteAndAddShaderBinary(file_name, std::vector<uint32_t>{ result.cbegin(), result.cend() }, stage);
        return source_hash;
    }

    VkShaderStageFlagBits ShaderCompilerImpl::getStage(const char* path_to_source) const {
        // We weren't given a stage, try to infer it from the file.
        const std::string stage_ext = fs::path(path_to_source).extension().string();
        auto iter = extension_stage_map.find(stage_ext);
        if (iter == extension_stage_map.end()) {
            std::cerr << "Failed to infer a shader's stage flag from it's extension.\n";
            throw std::runtime_error("Could not infer shader file's stage based on extension!");
        }
        return (*iter).second;
    }


}