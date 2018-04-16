#include "ShaderFileTracker.hpp"
#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include "util/FileObserver.hpp"

namespace st {

    ShaderFileTracker::ShaderFileTracker()
    {
    }

    ShaderFileTracker::~ShaderFileTracker()
    {
    }

    bool ShaderFileTracker::FindShaderSource(const Shader & handle, std::string & dest_str) {
        if (Sources.count(handle) != 0) {
            dest_str = Sources.at(handle);
            return true;
        }
        else if (SourcePaths.count(handle) != 0) {
            // Load source string into memory
            std::ifstream input_file(SourcePaths.at(handle));
            if (!input_file.is_open()) {
                std::cerr << "Path to source of shader existed in program map, but source file itself could not be opened!\n";
                throw std::runtime_error("Failed to open shader source file.");
            }

            auto iter = Sources.emplace(handle, std::string{ std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>() });
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
                    auto iter = ResourceScripts.emplace(absolute_file_path, std::make_unique<ResourceFile>(LuaEnvironment::GetCurrentLuaEnvironment()));

                    if (!iter.second) {
                        std::cerr << "Failed to create a new resource script at path " << absolute_file_path.c_str() << " !\n";
                        throw std::runtime_error("Could not create a new resource script/file!");
                    }

                    iter.first->second->Execute(absolute_file_path.c_str());

                    dest_ptr = iter.first->second.get();
                    return true;
                }
                catch (const std::logic_error& e) {
                    dest_ptr = nullptr;
                    std::cerr << "Failed to create ResourceFile using given script: check console for script errors and try again.\n";
                    std::cerr << e.what();
                    ResourceFile* ptr = ResourceScripts.at(absolute_file_path).get();

                    auto delegate_fn = delegate_t<void(const char*)>::create<ResourceFile, &ResourceFile::Execute>(ptr);
                    auto& observer = FileObserver::GetFileObserver();
                    observer.WatchFile(absolute_file_path.c_str(), delegate_fn);
                    return false;
                }
            }
        }
        else {
            return false;
        }

    }

}