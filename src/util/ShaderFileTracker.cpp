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

        struct ShaderFileTracker
        {
            ShaderFileTracker(const std::string& initial_directory = std::string{ "" });
            ~ShaderFileTracker();

            void ClearAllContainers();
            void DumpContentsToCacheDir();

            ShaderToolsErrorCode FindShaderBody(const ShaderStage& handle, std::string& dest_str);
            ShaderToolsErrorCode AddShaderBodyPath(const ShaderStage& handle, const std::string& shader_body_path);
            ShaderToolsErrorCode FindShaderBinary(const ShaderStage& handle, std::vector<uint32_t>& dest_binary_vector);
            bool FindRecompiledShaderSource(const ShaderStage& handle, std::string& destination_str);
            bool FindAssemblyString(const ShaderStage& handle, std::string& destination_str);
            std::string GetShaderName(const ShaderStage& handle);

            std::recursive_mutex mapMutex;
            std::filesystem::path cacheDir{ std::filesystem::temp_directory_path() };
            
        };

        ShaderFileTracker::ShaderFileTracker(const std::string& initial_directory)
        {
            if (!initial_directory.empty())
            {
                cacheDir = fs::canonical(fs::path(initial_directory));
            }
            else
            {
                cacheDir = fs::temp_directory_path() / fs::path("ShaderToolsCache");

            }
            if (!fs::exists(cacheDir))
            {
                if (!fs::create_directories(cacheDir))
                {
                    std::cerr << "Couldn't create cache directory, outputs won't be cached for future runs (non-critical).\n";
                }
            }
        }

        ShaderFileTracker::~ShaderFileTracker()
        {
            DumpContentsToCacheDir();
        }

        void ShaderFileTracker::DumpContentsToCacheDir()
        {

            auto write_output = [&](const ShaderStage& handle, const std::string& contents, const std::string& extension)
            {
                const std::string output_name = GetShaderName(handle) + GetShaderStageString(handle.stageBits) + extension;
                const fs::path output_path = cacheDir / output_name;
                std::ofstream output_stream(output_path);
                if (output_stream.is_open())
                {
                    output_stream << contents;
                }
                output_stream.flush();
                output_stream.close();
            };

            for (const auto& handle : ShaderBodies)
            {
                write_output(handle.first, handle.second, std::string("Body.glsl"));
            }

            for (const auto& handle : FullSourceStrings)
            {
                write_output(handle.first, handle.second, std::string("Generated.glsl"));
            }

            for (const auto& handle : AssemblyStrings)
            {
                write_output(handle.first, handle.second, std::string(".spvasm"));
            }

            for (const auto& handle : RecompiledSourcesFromBinaries)
            {
                write_output(handle.first, handle.second, std::string("FromBinary.glsl"));
            }
        }

        ShaderToolsErrorCode ShaderFileTracker::FindShaderBody(const ShaderStage& handle, std::string& dest_str)
        {
            if (ShaderBodies.count(handle) != 0)
            {
                dest_str = ShaderBodies.at(handle);
                return ShaderToolsErrorCode::Success;
            }
            else if (BodyPaths.count(handle) != 0)
            {

                std::lock_guard map_guard(mapMutex);
                // Load source string into memory
                std::ifstream input_file(BodyPaths.at(handle));
                if (!input_file.is_open())
                {
                    return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
                }

                auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>() });
                if (!iter.second)
                {
                    return ShaderToolsErrorCode::FilesystemCouldNotEmplaceIntoInternalStorage;
                }

                dest_str = iter.first->second;
                return ShaderToolsErrorCode::Success;
            }
            else
            {
                return ShaderToolsErrorCode::FilesystemNoFileDataForGivenHandleFound;
            }
        }

        ShaderToolsErrorCode ShaderFileTracker::AddShaderBodyPath(const ShaderStage& handle, const std::string& shader_body_path)
        {
            if (BodyPaths.count(handle) != 0)
            {
                // Already had this path registered. Shouldn't really reach this point.
                return ShaderToolsErrorCode::Success;
            }
            else
            {

                std::lock_guard map_guard(mapMutex);
                fs::path source_body_path(shader_body_path);
                if (!fs::exists(source_body_path))
                {
                return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
                }

                BodyPaths.emplace(handle, fs::canonical(source_body_path));
                StageLastModificationTimes.emplace(handle, fs::last_write_time(BodyPaths.at(handle)));

                std::ifstream input_stream(source_body_path);
                if (!input_stream.is_open())
                {
                    return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
                }

                auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>() });
                if (iter.second)
                {
                    return ShaderToolsErrorCode::Success;
                }
                else
                {
                    return ShaderToolsErrorCode::FilesystemCouldNotEmplaceIntoInternalStorage;
                }
            }
        }

        ShaderToolsErrorCode ShaderFileTracker::FindShaderBinary(const ShaderStage& handle, std::vector<uint32_t>& dest_binary_vector)
        {
            if (Binaries.count(handle) != 0)
            {
                dest_binary_vector = Binaries.at(handle);
                return ShaderToolsErrorCode::Success;
            }
            else if (BinaryPaths.count(handle) != 0)
            {

                std::lock_guard map_guard(mapMutex);
                std::ifstream input_file(BinaryPaths.at(handle), std::ios::binary | std::ios::in | std::ios::ate);
                if (!input_file.is_open())
                {
                    return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
                }

                size_t code_size = static_cast<size_t>(input_file.tellg());
                std::vector<char> buffer(code_size);
                input_file.seekg(0, std::ios::beg);
                input_file.read(buffer.data(), code_size);
                input_file.close();

                std::vector<uint32_t> imported_binary(code_size / sizeof(uint32_t) + 1);
                memcpy(imported_binary.data(), buffer.data(), buffer.size());
                auto iter = Binaries.emplace(handle, imported_binary);

                if (!iter.second)
                {
                    return ShaderToolsErrorCode::FilesystemCouldNotEmplaceIntoInternalStorage;
                }

                dest_binary_vector = iter.first->second;
                return ShaderToolsErrorCode::Success;
            }
            else
            {
                return ShaderToolsErrorCode::FilesystemNoFileDataForGivenHandleFound;
            }
        }

        bool ShaderFileTracker::FindRecompiledShaderSource(const ShaderStage& handle, std::string& destination_str)
        {
            if (RecompiledSourcesFromBinaries.count(handle) != 0)
            {
                destination_str = RecompiledSourcesFromBinaries.at(handle);
                return true;
            }
            return false;
        }

        bool ShaderFileTracker::FindAssemblyString(const ShaderStage& handle, std::string& destination_str)
        {
            if (AssemblyStrings.count(handle) != 0)
            {
                destination_str = AssemblyStrings.at(handle);
                return true;
            }
            return false;
        }

        std::string ShaderFileTracker::GetShaderName(const ShaderStage& handle)
        {
            auto iter = BodyPaths.find(handle);
            if (iter == std::cend(BodyPaths))
            {
                return std::string{};
            }

            std::string filename = iter->second.filename().string();
            // now strip .(stage) if found
            size_t idx = filename.find_first_of('.');
            if (idx != std::string::npos)
            {
                filename = filename.substr(0, idx);
            }

            return filename;
        }

    } // namespace detail


    template<typename KeyType, typename ResultType>
    ReadRequestResult FindRequestedPayload(const KeyType key, const detail::MapAndMutex<KeyType, ResultType>& map_and_mutex)
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
                return FindRequestedPayload(request.ShaderHandle, detail::ShaderBodies);
            case ReadRequest::Type::FindShaderBinary:
                return FindRequestedPayload(request.ShaderHandle, detail::Binaries);
            case ReadRequest::Type::FindRecompiledShaderSource:
                return FindRequestedPayload(request.ShaderHandle, detail::RecompiledSourcesFromBinaries);
            case ReadRequest::Type::FindAssemblyString:
                return FindRequestedPayload(request.ShaderHandle, detail::AssemblyStrings);
            case ReadRequest::Type::FindLastModificationTime:
                return FindRequestedPayload(request.ShaderHandle, detail::StageLastModificationTimes);
            case ReadRequest::Type::FindFullSourceString:
                return FindRequestedPayload(request.ShaderHandle, detail::FullSourceStrings);
            case ReadRequest::Type::FindOptimizationStatus:
                return FindRequestedPayload(request.ShaderHandle, detail::StageOptimizationDisabled);
            case ReadRequest::Type::FindShaderName:
                return FindShaderName(request.ShaderHandle);
            case ReadRequest::Type::HasFullSourceString:
                return HasFullSourceString(request.ShaderHandle);
            case ReadRequest::Type::FindResourceGroupSetIndexMap:
                return std::unexpected{ ShaderToolsErrorCode::FileTrackerInvalidRequest };
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
