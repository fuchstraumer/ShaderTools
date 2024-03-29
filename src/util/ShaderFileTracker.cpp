#include "ShaderFileTracker.hpp"
#include "ResourceFormats.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>


namespace fs = std::filesystem;

namespace st
{

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

    void ShaderFileTracker::ClearAllContainers()
    {
        StageLastModificationTimes.clear();
        ShaderBodies.clear();
        RecompiledSourcesFromBinaries.clear();
        FullSourceStrings.clear();
        Binaries.clear();
        ShaderUsedResourceBlocks.clear();
        ResourceGroupSetIndexMaps.clear();
        BodyPaths.clear();
        BinaryPaths.clear();
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

    bool ShaderFileTracker::FindShaderBody(const ShaderStage& handle, std::string& dest_str)
    {
        if (ShaderBodies.count(handle) != 0)
        {
            dest_str = ShaderBodies.at(handle);
            return true;
        }
        else if (BodyPaths.count(handle) != 0)
        {

            std::lock_guard map_guard(mapMutex);
            // Load source string into memory
            std::ifstream input_file(BodyPaths.at(handle));
            if (!input_file.is_open())
            {
                throw std::runtime_error("Failed to open shader source file, path existed in program state but file couldn't be opened.");
            }

            auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>() });
            if (!iter.second)
            {
                throw std::runtime_error("Source path was valid, and file was read successfully: failed to embed into program state.");
            }

            dest_str = iter.first->second;
            return true;
        }
        else {
            return false;
        }
    }

    bool ShaderFileTracker::AddShaderBodyPath(const ShaderStage& handle, const std::string& shader_body_path) {
        if (BodyPaths.count(handle) != 0)
        {
            // Already had this path registered. Shouldn't really reach this point.
            return true;
        }
        else {

            std::lock_guard map_guard(mapMutex);
            fs::path source_body_path(shader_body_path);
            if (!fs::exists(source_body_path))
            {
                throw std::runtime_error("Failed to open given file: invalid path.");
            }

            BodyPaths.emplace(handle, fs::canonical(source_body_path));
            StageLastModificationTimes.emplace(handle, fs::last_write_time(BodyPaths.at(handle)));

            std::ifstream input_stream(source_body_path);
            if (!input_stream.is_open())
            {
                throw std::runtime_error("Failed to open input stream for given shader body path.");
            }

            auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>() });
            if (iter.second)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    bool ShaderFileTracker::FindShaderBinary(const ShaderStage& handle, std::vector<uint32_t>& dest_binary_vector)
    {
        if (Binaries.count(handle) != 0)
        {
            dest_binary_vector = Binaries.at(handle);
            return true;
        }
        else if (BinaryPaths.count(handle) != 0)
        {

            std::lock_guard map_guard(mapMutex);
            std::ifstream input_file(BinaryPaths.at(handle), std::ios::binary | std::ios::in | std::ios::ate);
            if (!input_file.is_open())
            {
                throw std::runtime_error("Shader binary path existed in program state, but we couldn't open the file referenced!");
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
                throw std::runtime_error("Path to shader binary valid, file read successfully - failure to emplace in internal state container.");
            }

            dest_binary_vector = iter.first->second;
            return true;
        }
        else
        {
            return false;
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

    ShaderFileTracker & ShaderFileTracker::GetFileTracker()
    {
        static ShaderFileTracker fileTracker;
        return fileTracker;
    }

    void ST_API ClearProgramState()
    {
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        ftracker.ClearAllContainers();
    }

    void ST_API DumpProgramStateToCache()
    {
        auto& ftrack = ShaderFileTracker::GetFileTracker();
        ftrack.DumpContentsToCacheDir();
    }

}
