#include "CompilerImpl.hpp"
#include "spirv_glsl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../common/impl/SessionImpl.hpp"
#include "../../common/UtilityStructsInternal.hpp"

#include <iostream>
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

    ShaderCompilerImpl::ShaderCompilerImpl(const ShaderCompilerOptions& options, SessionImpl* error_session) noexcept : compilerOptions(options), errorSession(error_session)
    {}


	ShaderCompilerImpl::~ShaderCompilerImpl() noexcept
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
        failed_optimized_compile = 2,
    };

    void dump_bad_source_to_file(const std::string& name, const std::string& src, const std::string& err_text, dump_reason reason)
    {

        std::string suffix;
        switch (reason)
        {
        case dump_reason::failed_compile:
            suffix = "_failed_compile.glsl";
            break;
        case dump_reason::failed_recompile:
            suffix = "_failed_recompile.glsl";
            break;
        case dump_reason::failed_optimized_compile:
            suffix = "_failed_optimized_compile.glsl";
            break;
        };

        const std::string output_name = name + suffix;

        std::ofstream output_stream(output_name);
        output_stream << src;
        output_stream.flush();
        output_stream.close();

        if (reason == dump_reason::failed_compile || reason == dump_reason::failed_optimized_compile)
        {
            const std::string err_msg_output = name + std::string{ "_compiliation_errors.txt" };
            output_stream.open(err_msg_output);
            output_stream << err_text;
            output_stream.flush();
            output_stream.close();
        }
    }

    ShaderToolsErrorCode ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, std::string name, std::string src)
    {
        shaderc_shader_kind shaderStage;
        ShaderToolsErrorCode error = getShaderKind(handle.stageBits, shaderStage);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
            return error;
        }

        error = compile(handle, shaderStage, std::move(name), std::move(src));
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
        }

        return error;
    }

    ShaderToolsErrorCode ShaderCompilerImpl::prepareToCompile(const ShaderStage& handle, const char* path_to_source_str)
    {
        fs::path path_to_source(path_to_source_str);

        // First check to verify the path given exists.
        if (!fs::exists(path_to_source))
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::FilesystemPathDoesNotExist, path_to_source_str);
            return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
        }

        std::ifstream input_file(path_to_source);
        if (!input_file.is_open())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened, path_to_source_str);
            return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
        }

        shaderc_shader_kind shaderStage;
        ShaderToolsErrorCode error = getShaderKind(handle.stageBits, shaderStage);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
            return error;
        }

        const std::string source_code((std::istreambuf_iterator<char>(input_file)), (std::istreambuf_iterator<char>()));
        const std::string file_name = path_to_source.filename().replace_extension().string();
        error = compile(handle, shaderStage, file_name, source_code);
        if (error != ShaderToolsErrorCode::Success)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, error, nullptr);
        }

        return error;
    }


    ShaderToolsErrorCode ShaderCompilerImpl::compile(const ShaderStage& handle, const shaderc_shader_kind& kind, std::string name, std::string src_str)
    {

        shaderc::Compiler compiler;
        shaderc::CompileOptions options = getCompilerOptions();

        // We allow for optimization to be disabled for shaders in case they break or need debugging
        // Should probably make this part of a try-catch with compiling shaders, where we try a second pass and notify the user if their shader fails optimized compile
        // Of note, this is due to a bunch of shaders that only failed compile when optimized, but compiled fine otherwise.
        bool optimization_disabled = false;
        {
            ReadRequest read_request{ ReadRequest::Type::FindOptimizationStatus, handle };
            ReadRequestResult read_result = MakeFileTrackerReadRequest(read_request);
            if (read_result.has_value())
            {
                optimization_disabled = std::get<bool>(*read_result);
            }
            else
            {
                //errorSession->AddError(this, ShaderToolsErrorSource::Compiler, read_result.error(), "WARNING: Couldn't find optimization status for given shader");
            }
        }

        // Perform dual compilation, where we compile first with zero optimization + all the debug info we need to run the reflection step
        // Then we compile again with selected optimization from the config, and that's what we'll give the user as needed.
        ShaderBinaryData binaryData;

        // Do optimized compilation first since it's most likely to fail, but also isn't blocking if it does fail
        {
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
            shaderc::SpvCompilationResult optimizedResult = compiler.CompileGlslToSpv(src_str, kind, name.c_str(), options);
            if (optimizedResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                const std::string err_msg = "Optimized binary compiliation failed, debug unoptimized binary is all that will be available! \n" + optimizedResult.GetErrorMessage();
                errorSession->AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::CompilerShaderCompilationFailed, err_msg.c_str());
#ifndef NDEBUG
                std::cerr << "Dumping source for " << name << "to file....\n";
                dump_bad_source_to_file(name, src_str, err_msg, dump_reason::failed_optimized_compile);
#endif
            }
            else
            {
                binaryData.optimizedSpirv = std::vector<uint32_t>{ optimizedResult.begin(), optimizedResult.end() };
            }
        }

        {
            // Now compile with zero optimization and debug info
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
            options.SetGenerateDebugInfo();
            shaderc::SpvCompilationResult debugResult = compiler.CompileGlslToSpv(src_str, kind, name.c_str(), options);
            if (debugResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                const std::string err_msg = debugResult.GetErrorMessage();
                errorSession->AddError(this, ShaderToolsErrorSource::Compiler, ShaderToolsErrorCode::CompilerShaderCompilationFailed, err_msg.c_str());          
#ifndef NDEBUG
            std::cerr << "Dumping source for " << name << "to file....\n";
            dump_bad_source_to_file(name, src_str, err_msg, dump_reason::failed_compile);
#endif
                return ShaderToolsErrorCode::CompilerShaderCompilationFailed;
            }
            else
            {
                binaryData.spirvForReflection = std::vector<uint32_t>{ debugResult.begin(), debugResult.end() };
            }   
        }

        WriteRequest write_request{ WriteRequest::Type::AddShaderBinary, handle, binaryData };
        ShaderToolsErrorCode write_result = MakeFileTrackerWriteRequest(std::move(write_request));  
        if (write_result != ShaderToolsErrorCode::Success)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Compiler, write_result, name.c_str());
        }

        return write_result;
    }

    ShaderToolsErrorCode ShaderCompilerImpl::recompileBinaryToGLSL(const ShaderStage& handle, size_t* str_size, char* dest_str)
    {
        ReadRequest read_request{ ReadRequest::Type::FindRecompiledShaderSource, handle };
        ReadRequestResult request_result = MakeFileTrackerReadRequest(read_request);
        if (request_result.has_value())
        {

            const std::string& recompiledSrcStrRef = std::get<std::string>(*request_result);
            *str_size = recompiledSrcStrRef.size();
            if (dest_str != nullptr)
            {
                // use a move this time since I want to tell the compiler to, hopefully, move this result too
                std::string finalSrcString = std::move(std::get<std::string>(*request_result));
                std::copy(finalSrcString.begin(), finalSrcString.end(), dest_str);
            }

            return ShaderToolsErrorCode::Success;
        }
        else
        {
            return request_result.error();
        }
    }

    ShaderToolsErrorCode ShaderCompilerImpl::getBinaryAssemblyString(const ShaderStage& handle, size_t* str_size, char* dest_str)
    {
        ReadRequest read_request{ ReadRequest::Type::FindAssemblyString, handle };
        ReadRequestResult request_result = MakeFileTrackerReadRequest(read_request);
        if (request_result.has_value())
        {

            const std::string& assemblyStrRef = std::get<std::string>(*request_result);
            *str_size = assemblyStrRef.size();
            if (dest_str != nullptr)
            {
                std::string assemblyStr = std::move(std::get<std::string>(*request_result));
                std::copy(assemblyStr.begin(), assemblyStr.end(), dest_str);
            }

            return ShaderToolsErrorCode::Success;
        }
        else
        {
            return request_result.error();
        }
    }

}
