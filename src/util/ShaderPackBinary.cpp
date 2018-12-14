#include "ShaderPackBinary.hpp"
#include "ShaderFileTracker.hpp"
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../core/impl/ShaderImpl.hpp"
#include "../core/impl/ShaderPackImpl.hpp"
#include <vector>
#include <string>
#include <fstream>
#include "easyloggingpp/src/easylogging++.h"

namespace st {

    namespace fs = std::experimental::filesystem;

    constexpr static uint32_t SHADER_TOOLS_VERSION = VK_MAKE_VERSION(0, 4, 0);

    void CreateShaderBinary(const Shader* src, ShaderBinary* binary_dst) {

        size_t num_stages{ 0 };
        src->GetShaderStages(&num_stages, nullptr);
        std::vector<st::ShaderStage> stages(num_stages, st::ShaderStage("NULL", VK_SHADER_STAGE_ALL));
        src->GetShaderStages(&num_stages, stages.data());

        binary_dst->NumShaderStages = static_cast<uint32_t>(num_stages);

        // can now allocate a bunch of our arrays in the ShaderBinary struct
        binary_dst->StageIDs = new uint64_t[num_stages];
        binary_dst->LastWriteTimes = new uint64_t[num_stages];
        binary_dst->PathLengths = new uint32_t[num_stages];
        binary_dst->SrcStringLengths = new uint32_t[num_stages];
        binary_dst->BinaryLengths = new uint32_t[num_stages];

        // copy shader stage handles
        for (size_t i = 0; i < num_stages; ++i) {
            binary_dst->StageIDs[i] = stages[i].ID;
        }

        // now get data we need from ftracker
        auto& ftracker = ShaderFileTracker::GetFileTracker();

        uint32_t total_req_path_length{ 0 };
        uint32_t total_req_src_length{ 0 };
        uint32_t total_req_binary_length{ 0 };

        for (size_t i = 0; i < num_stages; ++i) {
            // write time first
            // TODO: validate this converts right. need to do UTC too. 
            const fs::path& stage_path = ftracker.BodyPaths.at(stages[i]);
            auto file_write_time = fs::file_time_type::clock::to_time_t(fs::last_write_time(stage_path));
            binary_dst->LastWriteTimes[i] = uint64_t(file_write_time);

            // get lengths of paths and source strings, so we can allocate memory, then write strings
            binary_dst->PathLengths[i] = static_cast<uint32_t>(stage_path.string().length());
            binary_dst->SrcStringLengths[i] = static_cast<uint32_t>(ftracker.FullSourceStrings.at(stages[i]).length());
            binary_dst->BinaryLengths[i] = static_cast<uint32_t>(ftracker.Binaries.at(stages[i]).size());

            total_req_path_length += binary_dst->PathLengths[i];
            total_req_src_length += binary_dst->SrcStringLengths[i];
            total_req_binary_length += binary_dst->BinaryLengths[i];
        }

        binary_dst->Paths = new char[total_req_path_length + 1];
        binary_dst->SourceStrings = new char[total_req_src_length + 1];
        binary_dst->Binaries = new uint32_t[total_req_binary_length];

        uint32_t current_path_offset{ 0 };
        uint32_t current_src_offset{ 0 };
        uint32_t current_binary_offset{ 0 };

        for (size_t i = 0; i < num_stages; ++i) {

            const std::string path_str = ftracker.BodyPaths.at(stages[i]).string();
            std::memcpy(binary_dst->Paths + current_path_offset, path_str.data(), path_str.length());
            current_path_offset += static_cast<uint32_t>(path_str.length());

            const std::string& full_src_str = ftracker.FullSourceStrings.at(stages[i]);
            std::memcpy(binary_dst->SourceStrings + current_src_offset, full_src_str.data(), full_src_str.length());
            current_src_offset += static_cast<uint32_t>(full_src_str.length());

            const auto& stage_binary = ftracker.Binaries.at(stages[i]);
            std::memcpy(binary_dst->Binaries + current_binary_offset, stage_binary.data(), stage_binary.size());
            current_binary_offset += static_cast<uint32_t>(stage_binary.size());

        }

        binary_dst->Paths[total_req_path_length] = '\0';
        binary_dst->SourceStrings[total_req_src_length] = '\0';

        binary_dst->TotalLength = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t); // magic + length field + num_stages field
        binary_dst->TotalLength += sizeof(uint64_t) * num_stages; // stage handles
        binary_dst->TotalLength += sizeof(uint64_t) * num_stages; // last write times
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // path length count
        binary_dst->TotalLength += (total_req_path_length + 1) * sizeof(char); // path strings
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // source string count
        binary_dst->TotalLength += (total_req_src_length + 1) * sizeof(char); // source strings
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // binary count
        binary_dst->TotalLength += total_req_binary_length * sizeof(uint32_t); // binaries

    }

    void CleanupShaderBinaryMembers(ShaderBinary* binary) {
        delete[] binary->StageIDs;
        delete[] binary->LastWriteTimes;
        delete[] binary->PathLengths;
        delete[] binary->Paths;
        delete[] binary->SrcStringLengths;
        delete[] binary->SourceStrings;
        delete[] binary->BinaryLengths;
        delete[] binary->Binaries;
    }

    void DestroyShaderBinary(ShaderBinary * binary) {
        CleanupShaderBinaryMembers(binary);
        delete binary;
    }

    void CreateShaderPackBinary(const ShaderPack* src, ShaderPackBinary* binary_dst) {
        const ShaderPackImpl* src_impl = src->impl.get();
        
        binary_dst->ShaderToolsVersion = SHADER_TOOLS_VERSION;

        const std::string absolute_script_path_string = src_impl->resourceScriptAbsolutePath;
        binary_dst->PackPathLength = static_cast<uint32_t>(absolute_script_path_string.length());
        binary_dst->PackPath = new char[absolute_script_path_string.length() + 1];
        std::memcpy(binary_dst->PackPath, absolute_script_path_string.c_str(), absolute_script_path_string.length());
        binary_dst->PackPath[binary_dst->PackPathLength] = '\0';

        binary_dst->NumShaders = static_cast<uint32_t>(src_impl->groups.size());
        binary_dst->Shaders = new ShaderBinary[binary_dst->NumShaders];
        binary_dst->OffsetsToShaders = new uint64_t[binary_dst->NumShaders];

        size_t idx{ 0 };
        for (const auto& shader_group : src_impl->groups) {
            const st::Shader* curr_shader = shader_group.second.get();
            CreateShaderBinary(curr_shader, &binary_dst->Shaders[idx]);
            ++idx;
        }

        uint64_t running_offset{ 0 };
        for (size_t i = 0; i <= idx; ++i) {
            binary_dst->OffsetsToShaders[i] = running_offset;
            running_offset += binary_dst->Shaders[i].TotalLength;
        }

        binary_dst->TotalLength = static_cast<uint32_t>(running_offset);
    }

    void DestroyShaderPackBinary(ShaderPackBinary * shader_pack) {
        delete[] shader_pack->PackPath;
        delete[] shader_pack->ResourceScriptPath;
        for (uint32_t i = 0; i < shader_pack->NumShaders; ++i) {
            CleanupShaderBinaryMembers(&shader_pack->Shaders[i]);
        }
        delete[] shader_pack->Shaders;
        delete[] shader_pack->OffsetsToShaders;
        delete shader_pack;
    }

    ShaderPackBinary* LoadShaderPackBinary(const char * fname) {
        ShaderPackBinary* result = new ShaderPackBinary;

        std::ifstream input_stream(fname, std::ios::binary | std::ios::ate);
        size_t total_length = input_stream.tellg();
        input_stream.seekg(0, std::ios::beg);
        if (!input_stream.is_open()) {
            throw std::runtime_error("Failed to open input file for reading!");
        }

        input_stream.read((char*)&result->MagicBits, sizeof(result->MagicBits));
        input_stream.read((char*)&result->ShaderToolsVersion, sizeof(result->ShaderToolsVersion));
        input_stream.read((char*)&result->TotalLength, sizeof(result->TotalLength));
        input_stream.read((char*)&result->PackPathLength, sizeof(result->PackPathLength));
        input_stream.read((char*)&result->PackPath, sizeof(char) * (result->PackPathLength + 1));

        return result;
    }

    void SaveBinaryToFile(ShaderPackBinary * binary, const char * fname) {

        fs::path output_path(fname);
        std::ofstream output_stream(output_path, std::ios::binary);
        if (!output_stream.is_open()) {
            LOG(ERROR) << "Failed to open output stream for writing binarized shader pack!";
            throw std::runtime_error("Failed to open output stream.");
        }

        auto write_fn = [&output_stream](void* ptr, size_t len) {
            output_stream.write((const char*)ptr, len);
        };

        output_stream.write((const char*)&binary->MagicBits, sizeof(uint32_t));
        //output_stream << binary->MagicBits;
        output_stream.write((const char*)&binary->ShaderToolsVersion, sizeof(binary->ShaderToolsVersion));
        output_stream.write((const char*)&binary->TotalLength, sizeof(binary->TotalLength));

        write_fn(&binary->PackPathLength, sizeof(binary->PackPathLength));
        write_fn(&binary->PackPath, sizeof(char) * (binary->PackPathLength + 1));

        write_fn(&binary->ResourceScriptPathLength, sizeof(binary->ResourceScriptPathLength));
        write_fn(&binary->ResourceScriptPath, sizeof(char) * (binary->ResourceScriptPathLength + 1));

        write_fn(&binary->NumShaders, sizeof(binary->NumShaders));
        write_fn(&binary->OffsetsToShaders, sizeof(uint64_t) * binary->NumShaders);

        for (uint32_t i = 0; i < binary->NumShaders; ++i) {
            const ShaderBinary* curr_shader = &binary->Shaders[i];

            write_fn((void*)&curr_shader->ShaderBinaryMagic, sizeof(curr_shader->ShaderBinaryMagic));
            write_fn((void*)&curr_shader->TotalLength, sizeof(curr_shader->TotalLength));
            write_fn((void*)&curr_shader->NumShaderStages, sizeof(curr_shader->NumShaderStages));

            const uint32_t& stages = curr_shader->NumShaderStages;
            write_fn((void*)&curr_shader->StageIDs, sizeof(uint64_t) * stages);
            write_fn((void*)&curr_shader->LastWriteTimes, sizeof(uint64_t) * stages);

            size_t path_length{ 1 };
            for (uint32_t j = 0; j < stages; ++j) {
                output_stream << curr_shader->PathLengths[j];
                path_length += curr_shader->PathLengths[j];
            }
            for (size_t j = 0; j < path_length; ++j) {
                output_stream << curr_shader->Paths[j];
            }

            size_t str_length{ 1 };
            for (uint32_t j = 0; j < stages; ++j) {
                output_stream << curr_shader->SrcStringLengths[j];
                str_length += curr_shader->SrcStringLengths[j];
            }
            for (size_t j = 0; j < str_length; ++j) {
                output_stream << curr_shader->SourceStrings[j];
            }

            size_t bin_length{ 0 };
            for (uint32_t j = 0; j < stages; ++j) {
                output_stream << curr_shader->BinaryLengths[j];
                bin_length += curr_shader->BinaryLengths[j];
            }
            for (size_t j = 0; j < bin_length; ++j) {
                output_stream << curr_shader->Binaries[j];
            }
        }

        output_stream.close();
    }

}
