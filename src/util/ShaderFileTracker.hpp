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

    union RequestKey
    {
		explicit RequestKey(ShaderStage handle) noexcept;
		explicit RequestKey(std::string_view key_string) noexcept;
        RequestKey(const RequestKey&) noexcept = default;
        RequestKey(RequestKey&&) noexcept = default;
        RequestKey& operator=(const RequestKey&) noexcept = default;
        RequestKey& operator=(RequestKey&&) noexcept = default;
		ShaderStage ShaderHandle;
		std::string_view KeyString;
    };

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
        RequestKey Key;

        ReadRequest(Type type, ShaderStage handle) noexcept;
        ReadRequest(Type type, std::string_view key_string) noexcept;

        ReadRequest(const ReadRequest&) noexcept = default;
        ReadRequest(ReadRequest&&) noexcept = default;
        ReadRequest& operator=(const ReadRequest&) noexcept = default;
        ReadRequest& operator=(ReadRequest&&) noexcept = default;
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
        RequestKey Key;
        RequestPayload Payload;

        WriteRequest(Type type, ShaderStage handle, RequestPayload payload) noexcept;
        WriteRequest(Type type, std::string_view key_string, RequestPayload payload) noexcept;

        WriteRequest(const WriteRequest&) noexcept = default;
        WriteRequest& operator=(const WriteRequest&) noexcept = default;

        WriteRequest(WriteRequest&& other) noexcept = default;
        WriteRequest& operator=(WriteRequest&& other) noexcept = default;
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
    std::vector<ReadRequestResult> MakeFileTrackerBatchReadRequest(std::vector<ReadRequest> readRequests);
    ShaderToolsErrorCode MakeFileTrackerWriteRequest(WriteRequest request);
    ShaderToolsErrorCode MakeFileTrackerBatchWriteRequest(std::vector<WriteRequest> writeRequests);
    ShaderToolsErrorCode MakeFileTrackerEraseRequest(EraseRequest request);
    ShaderToolsErrorCode MakeFileTrackerBatchEraseRequest(std::vector<EraseRequest> eraseRequests);

    constexpr bool WasWriteRequestSuccessful(ShaderToolsErrorCode code) noexcept;

}

#endif // !ST_SHADER_FILE_TRACKER_HPP
