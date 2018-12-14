#include "ShaderPackBinary.hpp"
#include "ShaderFileTracker.hpp"
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../core/impl/ShaderImpl.hpp"
#include "../core/impl/ShaderPackImpl.hpp"
#include <vector>
#include <string>

namespace st {

    namespace fs = std::experimental::filesystem;

    void CreateShaderBinary(const Shader* src, ShaderBinary* binary_dst) {

        size_t num_stages{ 0 };
        src->GetShaderStages(&num_stages, nullptr);
        std::vector<st::ShaderStage> stages(num_stages, st::ShaderStage("NULL", VK_SHADER_STAGE_ALL));
        src->GetShaderStages(&num_stages, stages.data());

        binary_dst->NumShaderStages = num_stages;

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
            binary_dst->PathLengths[i] = stage_path.string().length();
            binary_dst->SrcStringLengths[i] = ftracker.FullSourceStrings.at(stages[i]).length();
            binary_dst->BinaryLengths[i] = ftracker.Binaries.at(stages[i]).size();

            total_req_path_length += binary_dst->PathLengths[i] + 1;
            total_req_src_length += binary_dst->SrcStringLengths[i] + 1;
            total_req_binary_length += binary_dst->BinaryLengths[i];
        }

        binary_dst->Paths = new char[total_req_path_length];
        binary_dst->SourceStrings = new char[total_req_src_length];
        binary_dst->Binaries = new uint32_t[total_req_binary_length];

        uint32_t current_path_offset{ 0 };
        uint32_t current_src_offset{ 0 };
        uint32_t current_binary_offset{ 0 };

        for (size_t i = 0; i < num_stages; ++i) {

            const std::string path_str = ftracker.BodyPaths.at(stages[i]).string();
            std::memcpy(binary_dst->Paths + current_path_offset, path_str.data(), path_str.length());
            current_path_offset += path_str.length();

            const std::string& full_src_str = ftracker.FullSourceStrings.at(stages[i]);
            std::memcpy(binary_dst->SourceStrings + current_src_offset, full_src_str.data(), full_src_str.length());
            current_src_offset += full_src_str.length();

            const auto& stage_binary = ftracker.Binaries.at(stages[i]);
            std::memcpy(binary_dst->Binaries + current_binary_offset, stage_binary.data(), stage_binary.size());
            current_binary_offset += stage_binary.size();

        }

        binary_dst->TotalLength = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t); // magic + length field + num_stages field
        binary_dst->TotalLength += sizeof(uint64_t) * num_stages; // stage handles
        binary_dst->TotalLength += sizeof(uint64_t) * num_stages; // last write times
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // path length count
        binary_dst->TotalLength += total_req_path_length * sizeof(char); // path strings
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // source string count
        binary_dst->TotalLength += total_req_src_length * sizeof(char); // source strings
        binary_dst->TotalLength += sizeof(uint32_t) * num_stages; // binary count
        binary_dst->TotalLength += total_req_binary_length * sizeof(uint32_t); // binaries

    }

    void DestroyShaderBinary(ShaderBinary * binary) {
        delete[] binary->StageIDs;
        delete[] binary->LastWriteTimes;
        delete[] binary->PathLengths;
        delete[] binary->Paths;
        delete[] binary->SrcStringLengths;
        delete[] binary->SourceStrings;
        delete[] binary->BinaryLengths;
        delete[] binary->Binaries;
        delete binary;
    }

    void CreateShaderPackBinary(const ShaderPack* src, ShaderPackBinary* binary_dst) {
        const ShaderPackImpl* src_impl = src->impl.get();
        
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

    }

    void DestroyShaderPackBinary(ShaderPackBinary * shader_pack) {
        delete[] shader_pack->PackPath;
        delete[] shader_pack->ResourceScriptPath;
        for (uint32_t i = 0; i < shader_pack->NumShaders; ++i) {
            DestroyShaderBinary(&shader_pack->Shaders[i]);
        }
        delete[] shader_pack->Shaders;
        delete[] shader_pack->OffsetsToShaders;
        delete shader_pack;
    }

}
