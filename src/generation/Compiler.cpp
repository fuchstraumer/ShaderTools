#include "generation/Compiler.hpp"
#include "impl/CompilerImpl.hpp"
#include "../util/FilesystemUtils.hpp"
#include "../util/ShaderFileTracker.hpp"
#include <fstream>
#include <iostream>

namespace st
{

    static const ShaderCompilerOptions defaultOptions = ShaderCompilerOptions();

    ShaderCompiler::ShaderCompiler(Session& error_session) : impl(std::make_unique<ShaderCompilerImpl>(defaultOptions, error_session)) {}

    ShaderCompiler::~ShaderCompiler() {}

    ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&& other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }

    ShaderToolsErrorCode ShaderCompiler::Compile(const ShaderStage& handle, const char* shader_name, const char* src_str, const size_t src_len)
    {
        return impl->prepareToCompile(handle, shader_name, std::string{ src_str, src_str + src_len });
    }

    ShaderToolsErrorCode ShaderCompiler::Compile(const ShaderStage& handle, const char* path_to_source_str)
    {
        return impl->prepareToCompile(handle, path_to_source_str);
    }

    void ShaderCompiler::GetBinary(const ShaderStage& shader_handle, size_t* binary_sz, uint32_t* binary_dest_ptr) const
    {

        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        std::vector<uint32_t> binary_vec;
        if (FileTracker.FindShaderBinary(shader_handle, binary_vec) == ShaderToolsErrorCode::Success)
        {
            *binary_sz = binary_vec.size();
            if (binary_dest_ptr != nullptr)
            {
                std::copy(binary_vec.begin(), binary_vec.end(), binary_dest_ptr);
            }
        }
        else
        {
            std::cerr << "Could not find requested Shader handle's binary source!\n";
            *binary_sz = 0;
        }
    }

    void ShaderCompiler::GetAssembly(const ShaderStage& shader_handle, size_t* assembly_size, char* dest_assembly_str) const
    {
        impl->getBinaryAssemblyString(shader_handle, assembly_size, dest_assembly_str);
    }

    void ShaderCompiler::RecompileBinaryToGLSL(const ShaderStage& shader_handle, size_t* recompiled_size, char* dest_glsl_str) const
    {
        impl->recompileBinaryToGLSL(shader_handle, recompiled_size, dest_glsl_str);
    }

    void ShaderCompiler::SaveBinaryToFile(const ShaderStage & handle, const char * fname)
    {
        auto& tracker = ShaderFileTracker::GetFileTracker();

        std::vector<uint32_t> binary = tracker.Binaries.at(handle);
        std::ofstream output_file(fname, std::ios::binary | std::ios::out);

        if (!output_file.is_open())
        {
            throw std::runtime_error("Failed to open file for writing binary to!");
        }

        for (auto& word : binary)
        {
            output_file.write((const char*)&word, 4);
        }
    }

    ShaderToolsErrorCode ST_API CompileStandaloneShader(ShaderStage& resultHandle, const char* shader_name, const VkShaderStageFlags shader_stage, const char* src_str, const size_t src_len)
    {
        resultHandle = ShaderStage(shader_name, shader_stage);
        const std::string copiedSourceString(src_str, src_str + src_len);
        ShaderCompilerOptions options;
        Session errorSession;
        ShaderCompilerImpl compiler(options, errorSession);
        ShaderToolsErrorCode error = compiler.prepareToCompile(resultHandle, shader_name, copiedSourceString);
        return error;
    }

	ShaderToolsErrorCode ST_API RetrieveCompiledStandaloneShader(const ShaderStage shader_handle, size_t* binary_sz, uint32_t* binary_dest)
    {
        auto& fileTracker = ShaderFileTracker::GetFileTracker();
        auto binary_iter = fileTracker.Binaries.find(shader_handle);
        if (binary_iter != std::end(fileTracker.Binaries))
        {
            auto& binaryVec = binary_iter->second;
            *binary_sz = binaryVec.size();
            if (binary_dest != nullptr)
            {
                std::copy(binaryVec.begin(), binaryVec.end(), binary_dest);
            }

            return ShaderToolsErrorCode::Success;
        }

        return ShaderToolsErrorCode::FilesystemNoFileDataForGivenHandleFound;
    }

}
