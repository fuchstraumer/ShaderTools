#include "CompilerImpl.hpp"
#include "spirv_glsl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace st
{
    
    static const std::unordered_map<ShaderCompilerOptions::OptimizationLevel, shaderc_optimization_level> opt_level_map =
    {
        { ShaderCompilerOptions::OptimizationLevel::Disabled, shaderc_optimization_level_zero },
        { ShaderCompilerOptions::OptimizationLevel::Performance, shaderc_optimization_level_performance },
        { ShaderCompilerOptions::OptimizationLevel::Size, shaderc_optimization_level_size }
    };

    static const std::unordered_map<ShaderCompilerOptions::TargetVersionEnum, uint32_t> target_version_map =
    {
        { ShaderCompilerOptions::TargetVersionEnum::Vulkan1_0, shaderc_env_version_vulkan_1_0 },
        { ShaderCompilerOptions::TargetVersionEnum::Vulkan1_1, shaderc_env_version_vulkan_1_1 },
        { ShaderCompilerOptions::TargetVersionEnum::Vulkan1_2, shaderc_env_version_vulkan_1_2 },
        { ShaderCompilerOptions::TargetVersionEnum::Vulkan1_3, shaderc_env_version_vulkan_1_3 },
        { ShaderCompilerOptions::TargetVersionEnum::Vulkan1_4, shaderc_env_version_vulkan_1_4 },
        { ShaderCompilerOptions::TargetVersionEnum::VulkanLatest, shaderc_env_version_vulkan_1_4 }
    };

    namespace fs = std::filesystem;

    ShaderCompilerImpl::ShaderCompilerImpl(const ShaderCompilerOptions& options, Session& error_session) : compilerOptions(options), errorSession(error_session)
    {}

    shaderc::CompileOptions ShaderCompilerImpl::getCompilerOptions() const
    {
        shaderc::CompileOptions options;
        // We can't use this until we figure out how to get the debug info even if we don't actually use debug-info-build shaders for PSO creation
        // if (compilerOptions->GenerateDebugInfo)
        {
            options.SetGenerateDebugInfo();
        }

        options.SetOptimizationLevel(opt_level_map.at(compilerOptions.Optimization));
        options.SetTargetEnvironment(shaderc_target_env_vulkan, target_version_map.at(compilerOptions.TargetVersion));
        options.SetSourceLanguage(shaderc_source_language_glsl);
        return options;
    }

    ShaderToolsErrorCode ShaderCompilerImpl::getShaderKind(const uint32_t& flags, shaderc_shader_kind& result) const
    {
        switch (static_cast<VkShaderStageFlagBits>(flags))
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            result = shaderc_glsl_vertex_shader;    
            return ShaderToolsErrorCode::Success;
        // MoltenVK cannot yet use the geometry or tesselation shaders.
        // TODO: is this still true?
#ifndef __APPLE__
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            result = shaderc_glsl_geometry_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            result = shaderc_glsl_tess_control_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            result = shaderc_glsl_tess_evaluation_shader;
            return ShaderToolsErrorCode::Success;
#endif
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            result = shaderc_glsl_fragment_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            result = shaderc_glsl_compute_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            result = shaderc_glsl_raygen_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            result = shaderc_glsl_anyhit_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            result = shaderc_glsl_closesthit_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            result = shaderc_glsl_miss_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            result = shaderc_glsl_intersection_shader;
            return ShaderToolsErrorCode::Success;
        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            result = shaderc_glsl_callable_shader;
            return ShaderToolsErrorCode::Success;
        //case VK_SHADER_STAGE_TASK_BIT_KHR:
        //    return shaderc_glsl_task_shader;
        //case VK_SHADER_STAGE_MESH_BIT_KHR:
        //    return shaderc_glsl_mesh_shader;
        default:
            return ShaderToolsErrorCode::CompilerShaderKindNotSupported;
        }
    }

    enum class dump_reason
    {
        failed_compile = 0,
        failed_recompile = 1,
    };

    void dump_bad_source_to_file(const std::string& name, const std::string& src, const std::string& err_text, dump_reason reason)
    {

        std::string suffix = (reason == dump_reason::failed_compile) ? std::string{ "_failed_compile.glsl" } : std::string{ "_failed_recompile.glsl" };
        const std::string output_name = name + suffix;

        std::ofstream output_stream(output_name);
        output_stream << src;
        output_stream.flush();
        output_stream.close();

        if (reason == dump_reason::failed_compile)
        {
            const std::string err_msg_output = name + std::string{ "_compiliation_errors.txt" };
            output_stream.open(err_msg_output);
            output_stream << err_text;
            output_stream.flush();
            output_stream.close();
        }
    }

    ShaderToolsErrorCode ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, const std::string& name, const std::string& src)
    {
        shaderc_shader_kind shaderStage;
        ShaderToolsErrorCode error = getShaderKind(handle.stageBits, shaderStage);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
            return error;
        }

        error = compile(handle, shaderStage, name, src);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
        }

        return error;
    }

    ShaderToolsErrorCode ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, const char* path_to_source_str)
    {
        fs::path path_to_source(path_to_source_str);

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source))
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::FilesystemPathDoesNotExist, path_to_source_str);
            return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
        }

        std::ifstream input_file(path_to_source);
        if (!input_file.is_open())
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened, path_to_source_str);
            return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
        }

        shaderc_shader_kind shaderStage;
        ShaderToolsErrorCode error = getShaderKind(handle.stageBits, shaderStage);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
            return error;
        }

        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().replace_extension().string();
        error = compile(handle, shaderStage, file_name, source_code);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
        }

        return error;
    }


    ShaderToolsErrorCode ShaderCompilerImpl::compile(const ShaderStage& handle, const shaderc_shader_kind& kind, const std::string& name, const std::string& src_str)
    {
        ShaderToolsErrorCode error = ShaderToolsErrorCode::Success;

        shaderc::Compiler compiler;
        shaderc::CompileOptions options = getCompilerOptions();

        // We allow for optimization to be disabled for shaders in case they break or need debugging
        // Should probably make this part of a try-catch with compiling shaders, where we try a second pass and notify the user if their shader fails optimized compile
        // Of note, this is due to a bunch of shaders that only failed compile when optimized, but compiled fine otherwise.
        bool optimizationDisabled = false;
        {
            FileTrackerReadRequest readReqeust{ FileTrackerReadRequest::Type::FindOptimizationStatus, handle };
            ReadRequestResult readResult = MakeFileTrackerReadRequest(readReqeust);
            if (readResult.has_value())
            {
                optimizationDisabled = std::get<bool>(*readResult);
            }
            else
            {
                errorSession.AddError(this, ShaderToolsErrorSource::Compiler, readResult.error(), nullptr);
            }
        }

        if (optimizationDisabled)
        {
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
        }

        if (k_EnableSpvAssembly)
        {
            shaderc::AssemblyCompilationResult assembly_result = compiler.CompileGlslToSpvAssembly(src_str, kind, name.c_str(), options);
            if (assembly_result.GetCompilationStatus() == shaderc_compilation_status_success)
            {
                std::string assemblyResult{ assembly_result.begin(), assembly_result.end() };
                FileTrackerWriteRequest writeRequest{ FileTrackerWriteRequest::Type::AddShaderAssembly, handle, assemblyResult };
                ShaderToolsErrorCode addAssemblyError = MakeFileTrackerWriteRequest(writeRequest);

                if (addAssemblyError != ShaderToolsErrorCode::Success)
                {
                    errorSession.AddError(this, ShaderToolsErrorSource::Compiler, addAssemblyError, "Warning: failed to emplace assembly string into storage");
                }
            }
        }

        shaderc::SpvCompilationResult compiliation_result = compiler.CompileGlslToSpv(src_str, kind, name.c_str(), options);

        if (compiliation_result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            error = ShaderToolsErrorCode::CompilerShaderCompilationFailed;
            const std::string err_msg = compiliation_result.GetErrorMessage();
            errorSession.AddError(this, ShaderToolsErrorSource::Compiler, error, err_msg.c_str());

#ifndef NDEBUG
            std::cerr << "Dumping shader source to file...";
            dump_bad_source_to_file(name, src_str, err_msg, dump_reason::failed_compile);
#endif

        }
        else
        {
            FileTrackerWriteRequest writeRequest{ FileTrackerWriteRequest::Type::AddShaderBinary, handle, std::vector<uint32_t>{ compiliation_result.begin(), compiliation_result.end() } };
            ShaderToolsErrorCode writeResult = MakeFileTrackerWriteRequest(writeRequest);
            if (writeResult != ShaderToolsErrorCode::Success)
            {
                errorSession.AddError(this, ShaderToolsErrorSource::Compiler, writeResult, name.c_str());
            }
        }

        return error;
    }

    ShaderToolsErrorCode ShaderCompilerImpl::recompileBinaryToGLSL(const ShaderStage& handle, size_t* str_size, char* dest_str)
    {
        FileTrackerReadRequest readRequest{ FileTrackerReadRequest::Type::FindRecompiledShaderSource, handle };
        ReadRequestResult requestResult = MakeFileTrackerReadRequest(readRequest);
        if (requestResult.has_value())
        {
            const std::string& recompiledSrcStrRef = std::get<std::string>(*requestResult);
            *str_size = recompiledSrcStrRef.size();
            if (dest_str != nullptr)
            {
                // use a move this time since I want to tell the compiler to, hopefully, move this result too
                std::string finalSrcString = std::move(std::get<std::string>(*requestResult));
                std::copy(finalSrcString.begin(), finalSrcString.end(), dest_str);
            }
        }
        else
        {
            return requestResult.error();
        }
    }

    ShaderToolsErrorCode ShaderCompilerImpl::getBinaryAssemblyString(const ShaderStage& handle, size_t* str_size, char* dest_str)
    {
        FileTrackerReadRequest readRequest{ FileTrackerReadRequest::Type::FindAssemblyString, handle };
        ReadRequestResult requestResult = MakeFileTrackerReadRequest(readRequest);
        if (requestResult.has_value())
        {
            const std::string& assemblyStrRef = std::get<std::string>(*requestResult);
            *str_size = assemblyStrRef.size();
            if (dest_str != nullptr)
            {
                std::string assemblyStr = std::move(std::get<std::string>(*requestResult));
                std::copy(assemblyStr.begin(), assemblyStr.end(), dest_str);
            }
        }
        else
        {
            return requestResult.error();
        }
    }

}
