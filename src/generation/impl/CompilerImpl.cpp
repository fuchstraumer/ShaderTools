#include "CompilerImpl.hpp"
#include <experimental/filesystem>
#include <fstream>
#include "spirv_glsl.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include "../../util/ShaderFileTracker.hpp"

namespace st {

    namespace fs = std::experimental::filesystem;

    shaderc::CompileOptions ShaderCompilerImpl::getCompilerOptions() const {
        shaderc::CompileOptions options;
        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
        options.SetSourceLanguage(shaderc_source_language_glsl);
        return options;
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
            return shaderc_glsl_geometry_shader;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return shaderc_glsl_tess_control_shader;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return shaderc_glsl_tess_evaluation_shader;
#endif 
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return shaderc_glsl_compute_shader;
        default:
            LOG(ERROR) << "Invalid shader stage bitfield, or shader stage selected not supported on current platform!";
            throw std::domain_error("Invalid shader stage bitfield, or shader stage not supported on current platform!");
        }
    }

    enum class dump_reason {
        failed_compile = 0,
        failed_recompile = 1,
    };

    void dump_bad_source_to_file(const std::string& name, const std::string& src, const std::string& err_text, dump_reason reason) {
        std::string suffix = (reason == dump_reason::failed_compile) ? std::string{ "_failed_compile.glsl" } : std::string{ "_failed_recompile.glsl" };
        const std::string output_name = name + suffix;
        std::ofstream output_stream(output_name);
        output_stream << src;
        output_stream.flush(); output_stream.close();
        if (reason == dump_reason::failed_compile) {
            const std::string err_msg_output = name + std::string{ "_compiliation_errors.txt" };
            output_stream.open(err_msg_output);
            output_stream << err_text;
            output_stream.flush(); output_stream.close();
        }
    }

    void ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, const std::string& name, const std::string& src) {
        const auto shader_stage = getShaderKind(handle.GetStage());
        compile(handle, shader_stage, name, src);
    }

    void ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, const char* path_to_source_str) {
        fs::path path_to_source(path_to_source_str);

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source)) {
            LOG(ERROR) << "Given shader source path/file does not exist!\n";
            throw std::runtime_error("Failed to open/find given shader file.");
        }

        const auto shader_stage = getShaderKind(handle.GetStage());

        std::ifstream input_file(path_to_source);
        if (!input_file.is_open()) {
            LOG(ERROR) << "Failed to open " << path_to_source_str << " for compiliation.";
            throw std::runtime_error("Failed to open supplied file in GLSLCompiler!");
        }
        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().replace_extension().string();
        compile(handle, shader_stage, file_name, source_code);

    }


    void ShaderCompilerImpl::compile(const ShaderStage & handle, const shaderc_shader_kind & kind, const std::string & name, const std::string & src_str) {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        shaderc::Compiler compiler;
        auto options = getCompilerOptions();

        // Note: optimizer seems to behave weird and generally break for geometry shaders. Currently safest to just disable it.
        if (kind == shaderc_glsl_geometry_shader) {
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
        }

        shaderc::AssemblyCompilationResult assembly_result = compiler.CompileGlslToSpvAssembly(src_str, kind, name.c_str(), options);

        if (assembly_result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = assembly_result.GetErrorMessage();
            LOG(ERROR) << "Shader compiliation to assembly failed: " << err_msg.c_str() << "\n";
#ifndef NDEBUG
            LOG(ERROR) << "Dumping shader source to file...";
            dump_bad_source_to_file(name, src_str, err_msg, dump_reason::failed_compile);
#endif
            const std::string except_msg = std::string("Failed to compile shader to assembly: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        if (FileTracker.AssemblyStrings.count(handle) != 0) {
            // Erase pre-existing
            FileTracker.AssemblyStrings.erase(handle);
        }

        auto iter = FileTracker.AssemblyStrings.emplace(handle, std::string{ assembly_result.cbegin(), assembly_result.cend() });
        if (!iter.second) {
            LOG(ERROR) << "Failed to emplace generated assembly string into program's storage!";
            throw std::runtime_error("Emplacement of shader assembly string failed!");
        }

        // Now compile to binary.
        shaderc::SpvCompilationResult binary_result = compiler.AssembleToSpv(iter.first->second, options);
        if (binary_result.GetCompilationStatus() != shaderc_compilation_status_success) {
            const std::string err_msg = binary_result.GetErrorMessage();
            LOG(ERROR) << "Shader assembly into final SPIR-V result failed.\n";
            LOG(ERROR) << "Error message(s): \n" << err_msg << "\n";
            const std::string except_msg = std::string("Failed to assemble SPIR-V assembly into final binary result: ") + err_msg + std::string("\n");
            throw std::runtime_error(except_msg.c_str());
        }

        if (FileTracker.Binaries.count(handle) != 0) {
            // erase as we're gonna replace with an updated binary
            FileTracker.Binaries.erase(handle);
        }

        auto iter = FileTracker.Binaries.emplace(handle, std::vector<uint32_t>{binary_result.begin(), binary_result.end()});
        if (!iter.second) {
            LOG(ERROR) << "Failed to emplace compiled SPIR-V binary into program's storage!";
            throw std::runtime_error("Emplacement of shader SPIR-V binary failed.");
        }

    }

    void ShaderCompilerImpl::recompileBinaryToGLSL(const ShaderStage & handle, size_t * str_size, char * dest_str) {

        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        std::string recompiled_src_str;
        if (!FileTracker.FindRecompiledShaderSource(handle, recompiled_src_str)) {
            std::vector<uint32_t> found_binary;

            if (FileTracker.FindShaderBinary(handle, found_binary)) {
                using namespace spirv_cross;
                CompilerGLSL recompiler(found_binary);
                spirv_cross::CompilerGLSL::Options options;
                options.vulkan_semantics = true;
                recompiler.set_common_options(options);
                std::string recompiled_source;
                try {
                    recompiled_source = recompiler.compile();
                }
                catch (const spirv_cross::CompilerError& e) {
                    LOG(WARNING) << "Failed to fully parse/recompile SPIR-V binary back to GLSL text. Outputting partial source thus far.";
                    LOG(WARNING) << "spirv_cross::CompilerError.what(): " << e.what() << "\n";
                    recompiled_source = recompiler.get_partial_source();
                    dump_bad_source_to_file(FileTracker.GetShaderName(handle), recompiled_source, "", dump_reason::failed_recompile);
                }

                auto iter = FileTracker.RecompiledSourcesFromBinaries.emplace(handle, recompiled_source);
                if (!iter.second) {
                    LOG(WARNING) << "Failed to emplace recompiled shader source into program's filetracker system!";
                }

                *str_size = recompiled_source.size();
                if (dest_str != nullptr) {
                    std::copy(recompiled_source.cbegin(), recompiled_source.cend(), dest_str);
                }

            }
            else {
                LOG(WARNING) << "Failed to find or recompile shader's binary to GLSL code.";
                *str_size = 0;
                return;
            }
        }
        else {
            *str_size = recompiled_src_str.size();
            if (dest_str != nullptr) {
                std::copy(recompiled_src_str.cbegin(), recompiled_src_str.cend(), dest_str);
            }
        }
    }

    void ShaderCompilerImpl::getBinaryAssemblyString(const ShaderStage& handle, size_t* str_size, char* dest_str) {

        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        std::string recompiled_src_str;
        if (!FileTracker.FindAssemblyString(handle, recompiled_src_str)) {
            LOG(WARNING) << "Could not find requested shader's assembly source!";
            *str_size = 0;
            return;
        }
        else {
            *str_size = recompiled_src_str.size();
            if (dest_str != nullptr) {
                std::copy(recompiled_src_str.cbegin(), recompiled_src_str.cend(), dest_str);
            }
        }
    }


}
