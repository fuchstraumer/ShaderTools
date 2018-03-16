#include "Compiler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <shaderc/shaderc.hpp>
#define SCL_SECURE_NO_WARNINGS
namespace fs = std::experimental::filesystem;

namespace st {

    class ShaderCompilerImpl {
        ShaderCompilerImpl(const ShaderCompilerImpl&) = delete;
        ShaderCompilerImpl& operator=(const ShaderCompilerImpl&) = delete;
    public:

        ShaderCompilerImpl() = default;
        ~ShaderCompilerImpl() = default;
        ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept;
        ShaderCompilerImpl& operator=(ShaderCompilerImpl&& other) noexcept;

        static std::string preferredShaderDirectory;
        static bool saveCompiledBinaries;
        std::map<std::experimental::filesystem::path, std::vector<uint32_t>> compiledShaders;

        bool compile(const char* path_to_src, const VkShaderStageFlags stage);
        bool compile(const char* source_ptr, const size_t src_len, const VkShaderStageFlags stage);
        VkShaderStageFlags getStage(const char* path) const;
        void saveShaderToFile(const std::experimental::filesystem::path& source_path);
        void saveBinary(const std::experimental::filesystem::path& source_path, const std::experimental::filesystem::path& path_to_save_to);
        bool shaderSourceNewerThanBinary(const std::experimental::filesystem::path& source, const std::experimental::filesystem::path& binary);
    };

    std::string ShaderCompilerImpl::preferredShaderDirectory = std::string("./");
    bool ShaderCompilerImpl::saveCompiledBinaries = false;
    
    static const std::map<std::string, VkShaderStageFlags> extension_stage_map {
        { ".vert", VK_SHADER_STAGE_VERTEX_BIT },
        { ".frag", VK_SHADER_STAGE_FRAGMENT_BIT },
        { ".geom", VK_SHADER_STAGE_GEOMETRY_BIT },
        { ".teval", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
        { ".tcntl", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { ".comp", VK_SHADER_STAGE_COMPUTE_BIT }
    };

    ShaderCompiler::ShaderCompiler() : impl(std::make_unique<ShaderCompilerImpl>()) {}

    ShaderCompiler::~ShaderCompiler() {}

    ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderCompilerImpl::ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept : compiledShaders(std::move(other.compiledShaders)) {}

    ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    ShaderCompilerImpl& ShaderCompilerImpl::operator=(ShaderCompilerImpl&& other) noexcept {
        compiledShaders = std::move(other.compiledShaders);
        return *this;
    }

    bool ShaderCompiler::Compile(const char* path_to_source_str, const VkShaderStageFlags stage) {
        return impl->compile(path_to_source_str, stage);
    }

    bool ShaderCompiler::Compile(const char* src, const size_t len, const VkShaderStageFlags stage) {
        return impl->compile(src, len, stage);
    }

    VkShaderStageFlags ShaderCompiler::GetShaderStage(const char* path_to_source) const {
        return impl->getStage(path_to_source);
    }

    bool ShaderCompiler::HasShader(const char* binary_path) const {
        const fs::path absolute_path = fs::absolute(fs::path(binary_path));
        return impl->compiledShaders.count(absolute_path) != 0;
    }

    void ShaderCompiler::GetBinary(const char* binary_path, uint32_t* binary_size, uint32_t* binary_src) const {
        if (!HasShader(binary_path)) {
            *binary_size = std::numeric_limits<uint32_t>::max();
            std::cerr << "Requested binary does not exist.";
        }
        else {
            const fs::path absolute_path = fs::absolute(fs::path(binary_path));
            const auto& binary = impl->compiledShaders.at(absolute_path);
            *binary_size = static_cast<uint32_t>(binary.size());
            if (binary_src != nullptr) {
                std::copy(binary.cbegin(), binary.cend(), binary_src);
                return;
            }
        }

    }

    void ShaderCompiler::AddBinary(const char* path, const uint32_t sz, const uint32_t* binary_data) {
        const fs::path absolute_path = fs::absolute(fs::path(path));
        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard(insertion_mutex);
        auto inserted = impl->compiledShaders.emplace(absolute_path, std::move(std::vector<uint32_t>{ binary_data, binary_data + sz }));
        if (!inserted.second) {
            throw std::runtime_error("Tried to insert already-stored shader binary!");
        }
    }

    const char * ShaderCompiler::GetPreferredDirectory() {
        return ShaderCompilerImpl::preferredShaderDirectory.c_str();
    }

    void ShaderCompiler::SetPreferredDirectory(const char * directory) {
        ShaderCompilerImpl::preferredShaderDirectory = std::string(directory);
    }

    bool ShaderCompilerImpl::compile(const char* src_str, const size_t len src_len, const VkShaderStageFlags stage) {

                shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, 1);
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
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_string, shader_stage, nullptr);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard(insertion_mutex);
        auto inserted = compiledShaders.insert(std::make_pair(fs::absolute(path_to_source), std::vector<uint32_t>{ result.begin(), result.end() }));
        if (!inserted.second) {
            throw std::runtime_error("Failed to insert shader into compiled shader map!");
        }
        return true;
    }

    bool ShaderCompilerImpl::compile(const char* path_to_source_str, VkShaderStageFlags stage) {
        fs::path path_to_source(path_to_source_str);
        if (compiledShaders.count(path_to_source) != 0) {
            std::cerr << "Shader at given path has already been compiled...\n";
            return false;
        }

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source)) {
            std::cerr << "Given shader source path/file does not exist!\n";
            throw std::runtime_error("Failed to open/find given shader file.");
        }

        if (stage == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
            stage = getStage(path_to_source_str);
        }

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, 1);
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

        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, shader_stage, file_name.c_str());
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = result.GetErrorMessage();
            std::cerr << "Shader compiliation failed: " << err_msg.c_str() << "\n";
            const std::string except_msg = std::string("Failed to compile shader: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard(insertion_mutex);
        auto inserted = compiledShaders.insert(std::make_pair(fs::absolute(path_to_source), std::vector<uint32_t>{ result.begin(), result.end() }));
        if (!inserted.second) {
            throw std::runtime_error("Failed to insert shader into compiled shader map!");
        }
        return true;
    }

    VkShaderStageFlags ShaderCompilerImpl::getStage(const char* path_to_source) const {
        // We weren't given a stage, try to infer it from the file.
        const std::string stage_ext = fs::path(path_to_source).extension().string();
        auto iter = extension_stage_map.find(stage_ext);
        if (iter == extension_stage_map.end()) {
            std::cerr << "Failed to infer a shader's stage flag from it's extension.\n";
            throw std::runtime_error("Could not infer shader file's stage based on extension!");
        }
        return (*iter).second;
    }

    void ShaderCompilerImpl::saveShaderToFile(const fs::path& source_path) {

        if (fs::exists(fs::path(preferredShaderDirectory))) {
            bool success = fs::create_directories(fs::path(preferredShaderDirectory));
            if (!success) {
                throw std::runtime_error("Failed to create directory to save shaders into.");
            }
        }
        // check for existing binary and check for version parity
        fs::path binary_path = source_path.filename();
        binary_path.replace_extension(source_path.extension().string() + std::string(".spv"));
        
        if (fs::exists(binary_path)) {
            // Binary exists already, check if we need to re-save what we have 
            // loaded in the executable right now
            if (shaderSourceNewerThanBinary(source_path, binary_path)) {
                saveBinary(source_path, binary_path);
                return;
            } 
            else {
                // shader source and binary vaguely the same version-wise
                return;
            }
        }
        else {
            saveBinary(source_path, binary_path);
            return;
        }
    }

    void ShaderCompilerImpl::saveBinary(const fs::path& source, const fs::path& dest) {

        const auto& binary_src = compiledShaders.at(fs::absolute(source));

        std::ofstream outfile(dest, std::ofstream::binary);
        outfile.write(reinterpret_cast<const char*>(binary_src.data()), sizeof(uint32_t) * binary_src.size());
        outfile.close();

    }

    bool ShaderCompilerImpl::shaderSourceNewerThanBinary(const std::experimental::filesystem::path& source, const std::experimental::filesystem::path& binary) {
        const fs::file_time_type source_mod_time(fs::last_write_time(source));
        if (source_mod_time == fs::file_time_type::min()) {
            throw std::runtime_error("File write time for a source shader file is invalid - suggests invalid path passed to checker method!");
        }
        const fs::file_time_type binary_mod_time(fs::last_write_time(binary));
        if (binary_mod_time == fs::file_time_type::min()) {
            return true;
        }
        else {
            // don't check equals, as they could be equal (very nearly, at least)
            return binary_mod_time < source_mod_time;
        }
    }

}