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
		MapAndMutex<ShaderStage, std::vector<uint32_t>> Binaries;
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

	constexpr ReadRequest::RequestKey::RequestKey(ShaderStage handle) noexcept : ShaderHandle{ handle }
	{}

	constexpr ReadRequest::RequestKey::RequestKey(std::string_view key_string) noexcept : KeyString{ key_string }
	{}

	constexpr ReadRequest::ReadRequest(Type type, ShaderStage handle) noexcept : RequestType{ type }, Key{ handle }
	{}

	constexpr ReadRequest::ReadRequest(Type type, std::string_view key_string) noexcept : RequestType{ type }, Key{ key_string }
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

	ReadRequestResult MakeFileTrackerReadRequest(ReadRequest request)
	{
        switch (request.RequestType)
        {
            case ReadRequest::Type::Invalid:
                return std::unexpected{ ShaderToolsErrorCode::FileTrackerInvalidRequest };
            case ReadRequest::Type::FindShaderBody:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::ShaderBodies);
            case ReadRequest::Type::FindShaderBinary:
                return FindRequestedPayload(request.Key.ShaderHandle, detail::Binaries);
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
            default:
                return std::unexpected{ ShaderToolsErrorCode::FileTrackerInvalidRequest };
        }
	}

	std::vector<ReadRequestResult> MakeFileTrackerBatchReadRequest(const size_t numRequests, const ReadRequest* requests)
	{
        std::vector<ReadRequestResult> results(numRequests, std::unexpected{ ShaderToolsErrorCode::InvalidErrorCode });

        // We'll get fancier later, for now I want to get to testing
        for (size_t i = 0; i < numRequests; ++i)
        {
            results[i] = MakeFileTrackerReadRequest(requests[i]);
        }

        return results;
	}

	ShaderToolsErrorCode MakeFileTrackerWriteRequest(const WriteRequest& request)
	{
        return ShaderToolsErrorCode::Success;
	}

	ShaderToolsErrorCode MakeFileTrackerWriteRequest(WriteRequest request)
	{
		return ShaderToolsErrorCode::Success;
	}

	ShaderToolsErrorCode MakeFileTrackerBatchWriteRequest(const size_t numRequests, const WriteRequest* requests)
	{
		return ShaderToolsErrorCode::Success;
	}

	ShaderToolsErrorCode MakeFileTrackerEraseRequest(EraseRequest request)
	{
        return ShaderToolsErrorCode::Success;
	}

	ShaderToolsErrorCode MakeFileTrackerBatchEraseRequest(const size_t numRequests, const EraseRequest* requests)
	{
        return ShaderToolsErrorCode::Success;
	}

}
