#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <variant>
#include <expected>
#include <filesystem>
#include <vector>
#include <unordered_map>

namespace st
{
    // TODO: LRU cache for read requests, since we have to do dual-requests to first fill out the size of the result followed by actually copying the result.
    // This way we can avoid locking or blocking core storage on the second request, ideally

    using RequestPayload = std::variant<
        std::string,
        std::vector<uint32_t>,
        bool,
        std::filesystem::path,
        std::filesystem::file_time_type,
        std::vector<std::string>,
        std::unordered_map<ShaderStage, uint32_t>>;

    using ReadRequestResult = std::expected<RequestPayload, ShaderToolsErrorCode>;

    struct ReadRequest
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
            FindOptimizationStatus,
            FindShaderName,
            HasFullSourceString,
            FindResourceGroupSetIndexMap
        };
        Type RequestType{ Type::Invalid };
        ShaderStage ShaderHandle;
    };

    struct WriteRequest
    {
        enum class Type : uint8_t
        {
			Invalid = 0,
			AddShaderBody,
            AddShaderAssembly,
			AddShaderBinary,
			UpdateModificationTime,
			AddShaderBodyPath,
            AddFullSourceString,
            AddUsedResourceBlocks,
            SetRecompiledSourceString,
            SetStageOptimizationDisabled
        };
        Type RequestType{ Type::Invalid };
        ShaderStage ShaderHandle;
        RequestPayload Payload;
    };

    struct EraseRequest
    {
        enum class Type : uint32_t
        {
            Invalid = 0,
            Optional, // Does not return error on failure
            Required
        };

        enum class Target : uint32_t
        {
            Invalid = 0,
            AllDataForHandle,
            ShaderBody,
            FullSourceString,
            RecompiledSource,
            BinarySource
        };
        
        Type RequestType{ Type::Invalid };
        Target RequestTarget{ Target::Invalid };
        ShaderStage ShaderHandle;
    };

    ReadRequestResult MakeFileTrackerReadRequest(ReadRequest request);
    std::vector<ReadRequestResult> MakeFileTrackerBatchReadRequest(const size_t numRequests, const ReadRequest* requests);
    ShaderToolsErrorCode MakeFileTrackerWriteRequest(WriteRequest request);
    ShaderToolsErrorCode MakeFileTrackerBatchWriteRequest(const size_t numRequests, const WriteRequest* requests);
    ShaderToolsErrorCode MakeFileTrackerEraseRequest(EraseRequest request);
    ShaderToolsErrorCode MakeFileTrackerBatchEraseRequest(const size_t numRequests, const EraseRequest* requests);

}

#endif // !ST_SHADER_FILE_TRACKER_HPP
