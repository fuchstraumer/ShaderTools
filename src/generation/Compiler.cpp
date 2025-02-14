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
		FileTrackerReadRequest readRequest{ FileTrackerReadRequest::Type::FindShaderBinary, shader_handle };
		ReadRequestResult readResult = MakeFileTrackerReadRequest(readRequest);
		if (readResult.has_value())
		{
			std::vector<uint32_t> binary_vec = std::get<std::vector<uint32_t>>(*readResult);
			*binary_sz = binary_vec.size();
			if (binary_dest_ptr != nullptr)
			{
				std::copy(binary_vec.begin(), binary_vec.end(), binary_dest_ptr);
			}
		}
		else
		{
			impl->errorSession.AddError(this, ShaderToolsErrorSource::Compiler, readResult.error(), "Compiler API request to retrieve binary failed.");
			*binary_sz = 0;
		}
	}

	ShaderToolsErrorCode ShaderCompiler::GetAssembly(const ShaderStage& shader_handle, size_t* assembly_size, char* dest_assembly_str) const
	{
		return impl->getBinaryAssemblyString(shader_handle, assembly_size, dest_assembly_str);
	}

	ShaderToolsErrorCode ShaderCompiler::RecompileBinaryToGLSL(const ShaderStage& shader_handle, size_t* recompiled_size, char* dest_glsl_str) const
	{
		return impl->recompileBinaryToGLSL(shader_handle, recompiled_size, dest_glsl_str);
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
		FileTrackerReadRequest readRequest{ FileTrackerReadRequest::Type::FindShaderBinary, shader_handle };
		ReadRequestResult readResult = MakeFileTrackerReadRequest(readRequest);
		if (readResult.has_value())
		{
			std::vector<uint32_t> binaryVec = std::get<std::vector<uint32_t>>(*readResult);
			*binary_sz = binaryVec.size();
			if (binary_dest != nullptr)
			{
				std::copy(binaryVec.begin(), binaryVec.end(), binary_dest);
			}

			return ShaderToolsErrorCode::Success;
		}
		else
		{
			return readResult.error();
		}
	}

}
