#include "ShaderFileTracker.hpp"
#include "../lua/ResourceFile.hpp"
#include <fstream>
#include <experimental/filesystem>
#include "easyloggingpp/src/easylogging++.h"
namespace fs = std::experimental::filesystem;
namespace st {

    const std::unordered_map<std::string, size_t> fallback_resource_sizes = std::unordered_map<std::string, size_t>{
        { "vec1", 4 },
        { "vec2", 8 },
        { "vec3", 12 },
        { "vec4", 16 },
        { "ivec1", 4 },
        { "ivec2", 8 },
        { "ivec3", 12 },
        { "ivec4", 16 },
        { "uvec1", 4 },
        { "uvec2", 8 },
        { "uvec3", 12 },
        { "uvec4", 16 },
        { "bvec1", 4 },
        { "bvec2", 8 },
        { "bvec3", 12 },
        { "bvec4", 16 },
        { "dvec1", 8 },
        { "dvec2", 16 },
        { "dvec3", 24 },
        { "dvec4", 32 },
        { "float", 4 },
        { "double", 8 },
        { "int", 4 },
        { "uint", 4 },
        { "mat2", 16 },
        { "mat3", 36 },
        { "mat4", 64 }
    };

    std::unordered_map<std::string, size_t> GetBuiltInResourceSizes(const fs::path& built_ins_path = fs::path("../fragments/builtins/ObjectSizes.lua")) {
        if (!fs::exists(built_ins_path)) {
            return fallback_resource_sizes;
        }
        else {
            std::unique_ptr<LuaEnvironment> environment;
            const std::string path = built_ins_path.string();
            if (luaL_dofile(environment->GetState(), path.c_str())) {
                std::string err = lua_tostring(environment->GetState(), -1);
                LOG(WARNING) << "Failed to execute built-in script for baseline (GLSL) object sizes:\n ";
                LOG(WARNING) << err.c_str() << "\n";
                return fallback_resource_sizes;
            }

            using namespace luabridge;
            LuaRef object_ref = getGlobal(environment->GetState(), "ObjectSizes");
            auto object_table = environment->GetTableMap(object_ref);

            std::unordered_map<std::string, size_t> results{};
            for (const auto& entry : object_table) {
                size_t size = static_cast<size_t>(entry.second.cast<int>());
                results.emplace(entry.first, std::move(size));
            }
            return results;
        }
    }

    ShaderFileTracker::ShaderFileTracker(const std::string & initial_directory) {
        if (!initial_directory.empty()) {
            cacheDir = fs::absolute(fs::path(initial_directory));
        }
        else {
            cacheDir = fs::temp_directory_path() / fs::path("ShaderToolsCache");

        }
        if (!fs::exists(cacheDir)) {
            if (!fs::create_directories(cacheDir)) {
                LOG(WARNING) << "Couldn't create cache directory. File caching will not be possible.";
            }
        }

        ObjectSizes = GetBuiltInResourceSizes();
    }

    ShaderFileTracker::~ShaderFileTracker() {
        DumpContentsToCacheDir();
    }

    void ShaderFileTracker::DumpContentsToCacheDir() {

        auto write_output = [&](const ShaderStage& handle, const std::string& contents, const std::string& extension) {
            const std::string output_name = ShaderNames.at(handle) + extension;
            const fs::path output_path = cacheDir / output_name;
            std::ofstream output_stream(output_path);
            if (output_stream.is_open()) {
                output_stream << contents;
            }
        };

        for (const auto& handle : ShaderBodies) {
            write_output(handle.first, handle.second, std::string("Body.glsl"));
        }

        for (const auto& handle : FullSourceStrings) {
            write_output(handle.first, handle.second, std::string("Generated.glsl"));
        }

        for (const auto& handle : AssemblyStrings) {
            write_output(handle.first, handle.second, std::string(".spvasm"));
        }

        for (const auto& handle : RecompiledSourcesFromBinaries) {
            write_output(handle.first, handle.second, std::string("FromBinary.glsl"));
        }
    }

    bool ShaderFileTracker::FindShaderBody(const ShaderStage & handle, std::string & dest_str) {
        if (ShaderBodies.count(handle) != 0) {
            dest_str = ShaderBodies.at(handle);
            return true;
        }
        else if (BodyPaths.count(handle) != 0) {
            // Load source string into memory
            std::ifstream input_file(BodyPaths.at(handle));
            if (!input_file.is_open()) {
                LOG(ERROR) << "Path to source of shader existed in program map, but source file itself could not be opened!\n";
                throw std::runtime_error("Failed to open shader source file.");
            }

            auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>() });
            if (!iter.second) {
                LOG(ERROR) << "Failed to add read shader source file to programs source string map!\n";
                throw std::runtime_error("Could not add source file string to program map!");
            }

            dest_str = iter.first->second;
            return true;
        } 
        else {
            return false;
        }
    }

    bool ShaderFileTracker::AddShaderBodyPath(const ShaderStage & handle, const std::string & shader_body_path) {
        if (BodyPaths.count(handle) != 0) {
            // Already had this path registered. Shouldn't really reach this point.
            return true;
        }
        else {
            fs::path source_body_path(shader_body_path);
            if (!fs::exists(source_body_path)) {
                LOG(ERROR) << "Given path does not exist.";
                throw std::runtime_error("Failed to open given file: invalid path.");
            }

            BodyPaths.emplace(handle, fs::absolute(source_body_path));

            std::ifstream input_stream(source_body_path);
            if (!input_stream.is_open()) {
                LOG(ERROR) << "Given shader body path exists, but opening a file stream for this path failed!";
                throw std::runtime_error("Failed to open input stream for given shader body path.");
            }

            auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>() });
            if (iter.second) {
                return true;
            }
            else {
                return false;
            }
        }
    }

    bool ShaderFileTracker::FindShaderBinary(const ShaderStage & handle, std::vector<uint32_t>& dest_binary_vector) {
        if (Binaries.count(handle) != 0) {
            dest_binary_vector = Binaries.at(handle);
            return true;
        }
        else if (BinaryPaths.count(handle) != 0) {
            
            std::ifstream input_file(BinaryPaths.at(handle), std::ios::binary | std::ios::in | std::ios::ate);
            if (!input_file.is_open()) {
                LOG(ERROR) << "Path to binary of shader existed in programs map, but binary file itself could not be read!\n";
                throw std::runtime_error("Failed to open shader source file.");
            }

            size_t code_size = static_cast<size_t>(input_file.tellg());
            std::vector<char> buffer(code_size);
            input_file.seekg(0, std::ios::beg);
            input_file.read(buffer.data(), code_size);
            input_file.close();

            std::vector<uint32_t> imported_binary(code_size / sizeof(uint32_t) + 1);
            memcpy(imported_binary.data(), buffer.data(), buffer.size());
            auto iter = Binaries.emplace(handle, imported_binary);

            if (!iter.second) {
                LOG(ERROR) << "Failed to add shader binary to programs shader binary map!\n";
                throw std::runtime_error("Failed to emplace shader binary into program binary map.");
            }

            dest_binary_vector = iter.first->second;
            return true;
        }
        else {
            return false;
        }
    }

    bool ShaderFileTracker::FindResourceScript(const std::string & fname, const ResourceFile * dest_ptr) {
        namespace fs = std::experimental::filesystem;
        if (fs::exists(fs::path(fname))) {
            std::string absolute_file_path = fs::absolute(fs::path(fname)).string();
            if (ResourceScripts.count(absolute_file_path) != 0) {
                dest_ptr = ResourceScripts.at(absolute_file_path).get();
                return true;
            }
            else {
                
                auto iter = ResourceScripts.emplace(absolute_file_path, std::make_unique<ResourceFile>());

                if (!iter.second) {
                    LOG(ERROR) << "Failed to create a new resource script at path " << absolute_file_path.c_str() << " !\n";
                    throw std::runtime_error("Could not create a new resource script/file!");
                }

                iter.first->second->Execute(absolute_file_path.c_str());
                dest_ptr = ResourceScripts.at(absolute_file_path).get();
                return true;
            }
        }
        else {
            return false;
        }

    }

    bool ShaderFileTracker::FindRecompiledShaderSource(const ShaderStage & handle, std::string & destination_str) {
        if (RecompiledSourcesFromBinaries.count(handle) != 0) {
            destination_str = RecompiledSourcesFromBinaries.at(handle);
            return true;
        }
        return false;
    }

    bool ShaderFileTracker::FindAssemblyString(const ShaderStage & handle, std::string & destination_str) {
        if (AssemblyStrings.count(handle) != 0) {
            destination_str = AssemblyStrings.at(handle);
            return true;
        }
        return false;
    }

    ShaderFileTracker & ShaderFileTracker::GetFileTracker() {
        static ShaderFileTracker fileTracker;
        return fileTracker;
    }

}
