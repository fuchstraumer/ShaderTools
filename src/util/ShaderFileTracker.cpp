#include "ShaderFileTracker.hpp"
#include <iostream>
#include <fstream>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
namespace st {

    

    ShaderFileTracker::ShaderFileTracker(const std::string & initial_directory) {
        if (!initial_directory.empty()) {
            cacheDir = fs::absolute(fs::path(initial_directory));
        }
        else {
            cacheDir = fs::temp_directory_path() / fs::path("ShaderToolsCache");

        }
        if (!fs::exists(cacheDir)) {
            if (!fs::create_directories(cacheDir)) {
                throw std::runtime_error("Failed to create cache directories, lack user permissions...");
            }
        }
    }

    ShaderFileTracker::~ShaderFileTracker() {
        DumpContentsToCacheDir();
    }

    void ShaderFileTracker::DumpContentsToCacheDir() {

        auto write_output = [&](const Shader& handle, const std::string& contents, const std::string& extension) {
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

    bool ShaderFileTracker::FindShaderBody(const Shader & handle, std::string & dest_str) {
        if (ShaderBodies.count(handle) != 0) {
            dest_str = ShaderBodies.at(handle);
            return true;
        }
        else if (BodyPaths.count(handle) != 0) {
            // Load source string into memory
            std::ifstream input_file(BodyPaths.at(handle));
            if (!input_file.is_open()) {
                std::cerr << "Path to source of shader existed in program map, but source file itself could not be opened!\n";
                throw std::runtime_error("Failed to open shader source file.");
            }

            auto iter = ShaderBodies.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>() });
            if (!iter.second) {
                std::cerr << "Failed to add read shader source file to programs source string map!\n";
                throw std::runtime_error("Could not add source file string to program map!");
            }

            dest_str = iter.first->second;
            return true;
        } 
        else {
            return false;
        }
    }

    bool ShaderFileTracker::AddShaderBodyPath(const Shader & handle, const std::string & shader_body_path) {
        if (BodyPaths.count(handle) != 0) {
            // Already had this path registered. Shouldn't really reach this point.
            return true;
        }
        else {
            fs::path source_body_path(shader_body_path);
            if (!fs::exists(source_body_path)) {
                std::cerr << "Given path does not exist.";
                throw std::runtime_error("Failed to open given file: invalid path.");
            }

            BodyPaths.emplace(handle, fs::absolute(source_body_path));

            std::ifstream input_stream(source_body_path);
            if (!input_stream.is_open()) {
                std::cerr << "Given shader body path exists, but opening a file stream for this path failed!";
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

    bool ShaderFileTracker::FindShaderBinary(const Shader & handle, std::vector<uint32_t>& dest_binary_vector) {
        if (Binaries.count(handle) != 0) {
            dest_binary_vector = Binaries.at(handle);
            return true;
        }
        else if (BinaryPaths.count(handle) != 0) {
            
            std::ifstream input_file(BinaryPaths.at(handle), std::ios::binary | std::ios::in | std::ios::ate);
            if (!input_file.is_open()) {
                std::cerr << "Path to binary of shader existed in programs map, but binary file itself could not be read!\n";
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
                std::cerr << "Failed to add shader binary to programs shader binary map!\n";
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
                
                try {
                    auto iter = ResourceScripts.emplace(absolute_file_path, std::make_unique<ResourceFile>());

                    if (!iter.second) {
                        std::cerr << "Failed to create a new resource script at path " << absolute_file_path.c_str() << " !\n";
                        throw std::runtime_error("Could not create a new resource script/file!");
                    }

                    iter.first->second->Execute(absolute_file_path.c_str());

                    dest_ptr = ResourceScripts.at(absolute_file_path).get();
                    return true;
                }
                catch (const std::logic_error& e) {
                    dest_ptr = nullptr;
                    std::cerr << "Failed to create ResourceFile using given script: check console for script errors and try again.\n";
                    std::cerr << e.what();
                    throw e;
                }
            }
        }
        else {
            return false;
        }

    }

    bool ShaderFileTracker::FindRecompiledShaderSource(const Shader & handle, std::string & destination_str) {
        if (RecompiledSourcesFromBinaries.count(handle) != 0) {
            destination_str = RecompiledSourcesFromBinaries.at(handle);
            return true;
        }
        return false;
    }

    bool ShaderFileTracker::FindAssemblyString(const Shader & handle, std::string & destination_str) {
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