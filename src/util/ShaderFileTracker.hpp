#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <variant>
#include <expected>
#include <filesystem>

namespace st
{
    // TODO: LRU cache for read requests, since we have to do dual-requests to first fill out the size of the result followed by actually copying the result.
    // This way we can avoid locking or blocking core storage on the second request, ideally

    void ST_API InitializeFileTracker(const char* cache_directory);
    void ST_API ClearProgramState();
    void ST_API DumpProgramStateToCache();

    using RequestPayload = std::variant<
        std::string,
        std::vector<uint32_t>,
        bool,
        std::filesystem::path,
        std::filesystem::file_time_type>;

    using ReadRequestResult = std::expected<RequestPayload, ShaderToolsErrorCode>;

    struct FileTrackerReadRequest
    {
        // uint64_t so I can eventually use this with 128bit CMPEXCHG MWSR magic
        enum class Type : uint64_t
        {
            Invalid = 0,
			FindShaderBody,
			FindShaderBinary,
			FindRecompiledShaderSource,
			FindAssemblyString,
			FindLastModificationTime,
			FindFullSourceString,
            FindOptimizationStatus
        };
        Type RequestType{ Type::Invalid };
        ShaderStage ShaderHandle;
    };

    struct FileTrackerWriteRequest
    {
        enum class Type : uint8_t
        {
			Invalid = 0,
			AddShaderBody,
            AddShaderAssembly,
			AddShaderBinary,
			UpdateModificationTime,
			AddShaderBodyPath,
        };
        Type RequestType{ Type::Invalid };
        ShaderStage ShaderHandle;
        RequestPayload Payload;
    };

    ReadRequestResult MakeFileTrackerReadRequest(const FileTrackerReadRequest& request);
    ShaderToolsErrorCode MakeFileTrackerWriteRequest(const FileTrackerWriteRequest& request);

}

#endif // !ST_SHADER_FILE_TRACKER_HPP
