#include "ShaderFileTracker.hpp"
#include "ResourceFormats.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>


namespace fs = std::filesystem;

namespace st
{

    namespace detail
    {

        struct RwLockGuard
        {
            enum class Mode : uint8_t
            {
                Read,
                Write
            };

            RwLockGuard(Mode _mode, std::shared_mutex& _mutex) noexcept :
                mode{ _mode }, mutex{ _mutex }
            {
                if (mode == Mode::Read)
                {
                    mutex.lock_shared();
                }
                else if (mode == Mode::Write)
                {
                    mutex.lock();
                }
            }

            ~RwLockGuard() noexcept
            {
                if (mode == Mode::Read)
                {
                    mutex.unlock_shared();
                }
                else if (mode == Mode::Write)
                {
                    mutex.unlock();
                }
            }

        private:
            Mode mode{ Mode::Read };
            std::shared_mutex& mutex;
        };


        template<typename Key, typename Value>
        struct MapAndMutex
        {
            std::unordered_map<Key, Value> map;
            std::shared_mutex mutex;

            void clear()
            {
                RwLockGuard guard(RwLockGuard::Mode::Write, mutex);
                map.clear();
            }
        };

		MapAndMutex<ShaderStage, std::string> ShaderBodies;
		MapAndMutex<ShaderStage, ShaderBinaryData> Binaries;
		MapAndMutex<ShaderStage, std::string> RecompiledSourcesFromBinaries;
		MapAndMutex<ShaderStage, std::string> AssemblyStrings;
		MapAndMutex<ShaderStage, std::filesystem::file_time_type> StageLastModificationTimes;
		MapAndMutex<ShaderStage, std::string> FullSourceStrings;
		MapAndMutex<ShaderStage, bool> StageOptimizationDisabled;
		MapAndMutex<ShaderStage, std::unordered_multimap<ShaderStage, std::string>> ShaderUsedResourceBlocks;
		// first key is resource group, second map is stage and index of group in that stage
		MapAndMutex<std::string, std::unordered_map<ShaderStage, uint32_t>> ResourceGroupSetIndexMaps;
		MapAndMutex<ShaderStage, std::filesystem::path> BodyPaths;
		MapAndMutex<ShaderStage, std::filesystem::path> BinaryPaths;

    } // namespace detail

	RequestKey::RequestKey(ShaderStage handle) noexcept : ShaderHandle{ handle }
	{}

	RequestKey::RequestKey(std::string_view key_string) noexcept : KeyString{ key_string }
	{}

    WriteRequest::WriteRequest(Type type, ShaderStage handle, RequestPayload payload) noexcept : RequestType{ type }, Key{ handle }, Payload{ payload }
	{}

    WriteRequest::WriteRequest(Type type, std::string_view key_string, RequestPayload payload) noexcept : RequestType{ type }, Key{ key_string }, Payload{ payload }
	{}

	ReadRequest::ReadRequest(Type type, ShaderStage handle) noexcept : RequestType{ type }, Key{ handle }
	{}

	ReadRequest::ReadRequest(Type type, std::string_view key_string) noexcept : RequestType{ type }, Key{ key_string }
	{}

	template<typename KeyType, typename ResultType>
    ReadRequestResult FindRequestedPayload(const KeyType key, detail::MapAndMutex<KeyType, ResultType>& map_and_mutex)
    {
        static_assert(std::is_default_constructible_v<ResultType>);
        using namespace detail;
        RwLockGuard lock_guard(RwLockGuard::Mode::Read, map_and_mutex.mutex);
        auto& map = map_and_mutex.map;
        auto iter = map.find(key);
        if (iter != map.end())
        {
            return iter->second;
        }
        else
        {
            return std::unexpected(ShaderToolsErrorCode::FileTrackerReadRequestFailed);
        }
    }

    ReadRequestResult FindShaderBinary(const ShaderStage handle, bool isOptimizedBinaryRequest)
    {
        using namespace detail;
        RwLockGuard lock_guard(RwLockGuard::Mode::Read, Binaries.mutex);
        auto& map = Binaries.map;
        auto iter = map.find(handle);
        if (iter != map.end())
        {
            if (isOptimizedBinaryRequest && iter->second.optimizedSpirv)
            {
				return iter->second.optimizedSpirv.value();
			}
			else
			{
                // default return reflection spir-v
				return iter->second.spirvForReflection;
			}
        }
        else
        {
			return std::unexpected(ShaderToolsErrorCode::FileTrackerReadRequestFailed);
        }
    }

    ReadRequestResult FindShaderName(const ShaderStage handle)
    {
        ReadRequestResult result = FindRequestedPayload(handle, detail::BodyPaths);
        // gotta do a silly little transform, oops
        if (result.has_value())
        {
            fs::path bodyPath = std::get<fs::path>(*result);
            // note, filename() first strips that path info before we convert the rest to string
            std::string filename = bodyPath.filename().string();
            size_t idx = filename.find_first_of('.');
            if (idx != std::string::npos)
            {
                filename = filename.substr(0, idx);
            }

            return filename;
        }
        else
        {
            return result;
        }
    }

    ReadRequestResult HasFullSourceString(const ShaderStage handle)
    {
        // Annoying special case because of type conversions
		detail::MapAndMutex<ShaderStage, std::string>& curr = detail::FullSourceStrings;
		detail::RwLockGuard lock_guard(detail::RwLockGuard::Mode::Read, curr.mutex);
		auto& map = curr.map;
        return map.count(handle) != 0;
    }
	
    template<typename KeyType, typename PayloadType>
	ShaderToolsErrorCode DoWriteRequest(KeyType key, PayloadType payload, detail::MapAndMutex<KeyType, PayloadType>& map_and_mutex)
	{
		using namespace detail;
		RwLockGuard lock_guard(RwLockGuard::Mode::Write, map_and_mutex.mutex);
		auto& map = map_and_mutex.map;
		auto result = map.try_emplace(key, std::move(payload));
		if (!result.second)
		{
            auto iter = result.first;
            // payload already exists
            if (iter->first == key)
            {
                return ShaderToolsErrorCode::FileTrackerPayloadAlreadyStored;
            }
            else
            {
                return ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage;
            }
		}
		else
		{
			return ShaderToolsErrorCode::Success;
		}
	}

    ShaderToolsErrorCode DoWriteRequest(ShaderStage key, std::vector<std::string> payload, detail::MapAndMutex<ShaderStage, std::unordered_multimap<ShaderStage, std::string>>& map_and_mutex)
    {
        using namespace detail;
        RwLockGuard lock_guard(RwLockGuard::Mode::Write, map_and_mutex.mutex);
        auto& map_of_maps = map_and_mutex.map;
        auto iter_to_contained_map = map_of_maps.find(key);
        if (iter_to_contained_map == map_of_maps.end())
        {
			auto& curr_map = map_of_maps[key];
            for (auto&& resource_block : payload)
            {
                auto result = curr_map.emplace(key, std::move(resource_block));
                if (result == curr_map.end())
                {
                    return ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage;
                }
            }
			return ShaderToolsErrorCode::Success;
        }
        else
        {
            auto& curr_map = iter_to_contained_map->second;
            for (auto&& resource_block : payload)
            {
                auto result = curr_map.emplace(key, std::move(resource_block));
                if (result == curr_map.end())
                {
                    return ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage;
                }
            }
            return ShaderToolsErrorCode::Success;
        }
    }

    template<>
    ShaderToolsErrorCode DoWriteRequest<ShaderStage, std::filesystem::path>(ShaderStage key, std::filesystem::path payload, detail::MapAndMutex<ShaderStage, std::filesystem::path>& map_and_mutex)
    {
		using namespace detail;
		RwLockGuard lock_guard(RwLockGuard::Mode::Write, map_and_mutex.mutex);
		auto& path_map = map_and_mutex.map;
		auto result = path_map.try_emplace(key, std::move(payload));
        if (!result.second)
		{
			auto iter = result.first;
			// payload already exists
			if (iter->first == key)
			{
				return ShaderToolsErrorCode::FileTrackerPayloadAlreadyStored;
			}
			else
			{
				return ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage;
			}
		}
        else
        {
            namespace fs = std::filesystem;

            std::filesystem::path path_to_body_file = fs::canonical((result.first)->second);
            if (!fs::exists(path_to_body_file))
            {
                return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
            }

            std::ifstream body_file_stream(path_to_body_file);
            if (!body_file_stream.is_open())
            {
                return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
            }

            std::string body_string{ std::istreambuf_iterator<char>(body_file_stream), std::istreambuf_iterator<char>() };
            RwLockGuard body_string_lock_guard(RwLockGuard::Mode::Write, detail::ShaderBodies.mutex);
            auto& body_map = detail::ShaderBodies.map;
            auto body_emplace_result = body_map.try_emplace(key, std::move(body_string));
			if (!body_emplace_result.second)
			{
				auto iter = body_emplace_result.first;
				// payload already exists
				if (iter->first == key)
				{
					return ShaderToolsErrorCode::FileTrackerPayloadAlreadyStored;
				}
				else
				{
					return ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage;
				}
			}
            else
            {
                return ShaderToolsErrorCode::Success;
            }
		}
    }

    template<typename KeyType, typename PayloadType>
    ShaderToolsErrorCode ClearMap(detail::MapAndMutex<KeyType, PayloadType>& map_and_mutex)
    {
        using namespace detail;
        RwLockGuard lock_guard(RwLockGuard::Mode::Write, map_and_mutex.mutex);
        map_and_mutex.map.clear();
        return ShaderToolsErrorCode::Success;
    }

	ReadRequestResult MakeFileTrackerReadRequest(ReadRequest request)
	{
        switch (request.RequestType)
        {
            case ReadRequest::Type::Invalid:
                return std::unexpected{ ShaderToolsErrorCode::FileTrackerInvalidRequest };
            case ReadRequest::Type::FindShaderBody:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::ShaderBodies);
            case ReadRequest::Type::FindShaderBinary:
                return FindShaderBinary(request.Key.ShaderHandle, true);
            case ReadRequest::Type::FindRecompiledShaderSource:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::RecompiledSourcesFromBinaries);
            case ReadRequest::Type::FindAssemblyString:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::AssemblyStrings);
            case ReadRequest::Type::FindLastModificationTime:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::StageLastModificationTimes);
            case ReadRequest::Type::FindFullSourceString:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::FullSourceStrings);
            case ReadRequest::Type::FindOptimizationStatus:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::StageOptimizationDisabled);
            case ReadRequest::Type::FindShaderName:
                return FindShaderName(request.Key.ShaderHandle);
            case ReadRequest::Type::HasFullSourceString:
                return HasFullSourceString(request.Key.ShaderHandle);
            case ReadRequest::Type::FindResourceGroupSetIndexMap:
                return FindRequestedPayload(std::string{ request.Key.KeyString }, detail::ResourceGroupSetIndexMaps);
			case ReadRequest::Type::FindShaderBinaryForReflection:
				return FindShaderBinary(request.Key.ShaderHandle, false);
            default:
                return std::unexpected{ ShaderToolsErrorCode::FileTrackerInvalidRequest };
        }
	}

	std::vector<ReadRequestResult> MakeFileTrackerBatchReadRequest(std::vector<ReadRequest> readRequests)
	{
        std::vector<ReadRequestResult> results(readRequests.size(), std::unexpected{ ShaderToolsErrorCode::InvalidErrorCode });

        // We'll get fancier later, for now I want to get to testing
        for (size_t i = 0; i < readRequests.size(); ++i)
        {
            results[i] = MakeFileTrackerReadRequest(std::move(readRequests[i]));
        }

        return results;
	}

	ShaderToolsErrorCode MakeFileTrackerWriteRequest(WriteRequest request)
	{
        switch (request.RequestType)
        {
        case WriteRequest::Type::Invalid:
            return ShaderToolsErrorCode::FileTrackerInvalidRequest;
        case WriteRequest::Type::AddShaderBody:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::string>(request.Payload)), detail::ShaderBodies);
        case WriteRequest::Type::AddShaderAssembly:
			return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::string>(request.Payload)), detail::AssemblyStrings);
        case WriteRequest::Type::AddShaderBinary:
			return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<ShaderBinaryData>(request.Payload)), detail::Binaries);
        case WriteRequest::Type::UpdateModificationTime:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::filesystem::file_time_type>(request.Payload)), detail::StageLastModificationTimes);
        case WriteRequest::Type::AddShaderBodyPath:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::filesystem::path>(request.Payload)), detail::BodyPaths);
        case WriteRequest::Type::AddFullSourceString:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::string>(request.Payload)), detail::FullSourceStrings);
		case WriteRequest::Type::AddUsedResourceBlocks:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::vector<std::string>>(request.Payload)), detail::ShaderUsedResourceBlocks);
        case WriteRequest::Type::SetRecompiledSourceString:
            return DoWriteRequest(request.Key.ShaderHandle, std::move(std::get<std::string>(request.Payload)), detail::RecompiledSourcesFromBinaries);
        case WriteRequest::Type::SetStageOptimizationDisabled:
            return DoWriteRequest(request.Key.ShaderHandle, std::get<bool>(request.Payload), detail::StageOptimizationDisabled);
        default:
            return ShaderToolsErrorCode::FileTrackerInvalidRequest;
        };

	}

	ShaderToolsErrorCode MakeFileTrackerBatchWriteRequest(std::vector<WriteRequest> writeRequests)
	{
        std::vector<ShaderToolsErrorCode> error_codes(writeRequests.size(), ShaderToolsErrorCode::Success);

        for (size_t i = 0; i < writeRequests.size(); ++i)
        {
            error_codes[i] = MakeFileTrackerWriteRequest(std::move(writeRequests[i]));
        }

        bool allSuccessful = std::any_of(error_codes.cbegin(), error_codes.cend(), &WasWriteRequestSuccessful);

		return allSuccessful ? ShaderToolsErrorCode::Success : ShaderToolsErrorCode::FileTrackerBatchWriteRequestFailed;
	}

	ShaderToolsErrorCode MakeFileTrackerEraseRequest(EraseRequest request)
	{
        return ShaderToolsErrorCode::Success;
	}

	ShaderToolsErrorCode MakeFileTrackerBatchEraseRequest(std::vector<EraseRequest> writeRequests)
	{
        return ShaderToolsErrorCode::Success;
	}

	constexpr bool WasWriteRequestSuccessful(ShaderToolsErrorCode code) noexcept
	{
        return (code == ShaderToolsErrorCode::Success) || (code == ShaderToolsErrorCode::FileTrackerPayloadAlreadyStored);
	}

	ShaderToolsErrorCode ClearAllInternalStorage()
	{
        ShaderToolsErrorCode result = ClearMap(detail::ShaderBodies);
		result = ClearMap(detail::Binaries);
        result = ClearMap(detail::RecompiledSourcesFromBinaries);
        result = ClearMap(detail::AssemblyStrings);
        result = ClearMap(detail::StageLastModificationTimes);
        result = ClearMap(detail::FullSourceStrings);
        result = ClearMap(detail::StageOptimizationDisabled);
        result = ClearMap(detail::ShaderUsedResourceBlocks);
        result = ClearMap(detail::ResourceGroupSetIndexMaps);
        result = ClearMap(detail::BodyPaths);
        result = ClearMap(detail::BinaryPaths);
        result = ClearMap(detail::AssemblyStrings);
        return result;
	}

}
