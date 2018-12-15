#include "ShaderPackBinary.hpp"
#include "ShaderFileTracker.hpp"
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../core/impl/ShaderImpl.hpp"
#include "../core/impl/ShaderPackImpl.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include "easyloggingpp/src/easylogging++.h"
#include "bitsery/bitsery.h"
#include "bitsery/adapter/buffer.h"
#include "bitsery/traits/vector.h"
#include "bitsery/traits/string.h"
#include "bitsery/adapter/stream.h"

namespace st {

    namespace fs = std::experimental::filesystem;
    constexpr static uint32_t SHADER_TOOLS_VERSION = VK_MAKE_VERSION(0, 4, 1);
    constexpr static uint32_t SHADER_BINARY_MAGIC_VALUE{ 0x70cd20ae };
    constexpr static uint32_t SHADER_PACK_BINARY_MAGIC_VALUE{ 0x9db3bb66 };

    using BitseryBuffer = std::vector<uint8_t>;
    using OutputAdapter = bitsery::OutputBufferAdapter<BitseryBuffer>;
    using InputAdapter = bitsery::InputBufferAdapter<BitseryBuffer>;

    struct ShaderBinary {
        ShaderBinary() = default;
        ShaderBinary(const Shader* src);
        uint32_t ShaderBinaryMagic{ SHADER_BINARY_MAGIC_VALUE };
        uint32_t NumShaderStages;
        std::vector<uint64_t> StageIDs;
        std::vector<long long> LastWriteTimes;
        std::vector<std::string> Paths;
        std::vector<std::string> SourceStrings;
        std::vector<std::string> AssemblyStrs;
        std::vector<std::string> RecompiledStrs;
        std::vector<std::vector<uint32_t>> Binaries;
    };

    ShaderBinary::ShaderBinary(const Shader * src) {

        size_t num_stages{ 0 };
        src->GetShaderStages(&num_stages, nullptr);
        std::vector<st::ShaderStage> stages(num_stages, ShaderStage(0u));
        src->GetShaderStages(&num_stages, stages.data());

        NumShaderStages = static_cast<uint32_t>(num_stages);
        StageIDs.resize(num_stages, 0u);
        LastWriteTimes.resize(num_stages);
        Paths.resize(num_stages);
        SourceStrings.resize(num_stages);
        AssemblyStrs.resize(num_stages);
        RecompiledStrs.resize(num_stages);
        Binaries.resize(num_stages);

        auto& ftracker = ShaderFileTracker::GetFileTracker();
        for (size_t i = 0; i < num_stages; ++i) {
            StageIDs[i] = stages[i].ID;
            const fs::path& stage_path = ftracker.BodyPaths.at(stages[i]);
            Paths[i] = stage_path.string();

            fs::file_time_type full_last_write_time = fs::last_write_time(stage_path);
            auto true_time = full_last_write_time.time_since_epoch();
            LastWriteTimes[i] = int64_t(true_time.count());

            SourceStrings[i] = ftracker.FullSourceStrings.at(stages[i]);
            AssemblyStrs[i] = ftracker.AssemblyStrings.at(stages[i]);
            RecompiledStrs[i] = ftracker.RecompiledSourcesFromBinaries.at(stages[i]);
            Binaries[i] = ftracker.Binaries.at(stages[i]);
        }

    }

    struct ShaderPackBinary {
        ShaderPackBinary() = default;
        ShaderPackBinary(const ShaderPack* src);
        uint32_t MagicBits{ SHADER_PACK_BINARY_MAGIC_VALUE };
        uint32_t ShaderToolsVersion{ 0 };
        std::string PackPath{};
        std::string ResourceScriptPath{};
        uint32_t NumShaders{ 0 };
        std::vector<ShaderBinary> Shaders{};
    };

    ShaderPackBinary::ShaderPackBinary(const ShaderPack * initial_pack) : ShaderToolsVersion{ SHADER_TOOLS_VERSION } {
        const ShaderPackImpl* src = initial_pack->impl.get();
        PackPath = src->packScriptPath;
        ResourceScriptPath = src->resourceScriptPath;

        for (const auto& shader_group : src->groups) {
            Shaders.emplace_back(ShaderBinary(shader_group.second.get()));
        }

        NumShaders = static_cast<uint32_t>(Shaders.size());

    }


    template<typename S>
    void serialize(S& s, std::vector<std::string>& strs) {
        for (auto& str : strs) {
            s.text1b(str, 512);
        }
    }

    template<typename S>
    void serialize(S& s, std::vector<std::vector<uint32_t>>& binaries) {
        for (auto& bin : binaries) {
            s.container4b(bin, 4092);
        }
    }

    template<typename S>
    void serialize(S& s, ShaderBinary& bin) {
        s.value4b(bin.ShaderBinaryMagic);
        s.value4b(bin.NumShaderStages);
        s.container8b(bin.StageIDs, 8);
        s.container8b(bin.LastWriteTimes, 8);
        s.container(bin.Paths, 8, [&s](std::string& str) { s.text1b(str, 512); });
        s.container(bin.SourceStrings, 8, [&s](std::string& str) {
            LOG_IF(str.size() > 16384, WARNING) << "Warning! Serializing a large source string of over 16384 characters - " << str.size() << " characters in current string!";
            s.text1b(str, 65536);
        });
        s.container(bin.AssemblyStrs, 8, [&s](std::string& str) {
            LOG_IF(str.size() > 16384, WARNING) << "Warning! Serializing a large source string of over 16384 characters - " << str.size() << " characters in current string!";
            s.text1b(str, 65536);
        });
        s.container(bin.RecompiledStrs, 8, [&s](std::string& str) {
            LOG_IF(str.size() > 16384, WARNING) << "Warning! Serializing a large source string of over 16384 characters - " << str.size() << " characters in current string!";
            s.text1b(str, 65536);
        });
        s.container(bin.Binaries, 8, [&s](std::vector<uint32_t>& bin) { s.container4b(bin, 32768); });
    }

    template<typename S>
    void serialize(S& s, ShaderPackBinary& pack) {
        s.value4b(pack.MagicBits);
        s.value4b(pack.ShaderToolsVersion);
        s.text1b(pack.PackPath, 512);
        s.text1b(pack.ResourceScriptPath, 512);
        s.value4b(pack.NumShaders);
        s.container(pack.Shaders, 32, [&s](ShaderBinary& bin) { serialize<S>(s, bin); });
    }

    ShaderBinary* CreateShaderBinary(const Shader* src) {
        ShaderBinary* result = new ShaderBinary(src);
        return result;
    }

    void DestroyShaderBinary(ShaderBinary * binary) {
        delete binary;
    }

    ShaderPackBinary* CreateShaderPackBinary(const ShaderPack* src) {
        ShaderPackBinary* result = new ShaderPackBinary(src);
        return result;
    }

    void DestroyShaderPackBinary(ShaderPackBinary* shader_pack) {
        delete shader_pack;
    }

    ShaderPackBinary* LoadShaderPackBinary(const char * fname) {

        std::ifstream input_stream(fname, std::ios::binary);
        if (!input_stream.is_open()) {
            LOG(ERROR) << "Failed to open output stream for loading shader pack binary!";
            throw std::runtime_error("Failed to open output stream for loading shader pack binary!");
        }

        ShaderPackBinary* result = new ShaderPackBinary;
        auto deserialized_state = bitsery::quickDeserialization<bitsery::InputStreamAdapter>(input_stream, *result);
        if (deserialized_state.first != bitsery::ReaderError::NoError) {
            LOG(ERROR) << "Error deserializing shader pack binary file!";
            throw std::runtime_error("Error deserializing shader pack binary file!");
        }

        return result;
    }

    void SaveBinaryToFile(ShaderPackBinary * binary, const char * fname) {
        
        std::ofstream output_stream(fname, std::ios::binary | std::ios::trunc);
        if (!output_stream.is_open()) {
            LOG(ERROR) << "Failed to open output stream for writing shader pack binary!";
            throw std::runtime_error("Failed to open output stream for writing shader pack binary!");
        }

        bitsery::Serializer<bitsery::OutputBufferedStreamAdapter> serializer{ output_stream };
        serializer.object(*binary);
        bitsery::AdapterAccess::getWriter(serializer).flush();
        output_stream.close();

    }

}

void st::detail::LoadPackFromBinary(ShaderPackImpl * pack, ShaderPackBinary * bin) {

    pack->filePack = std::make_unique<shader_pack_file_t>(bin->PackPath.c_str());
    pack->packScriptPath = bin->PackPath;
    pack->workingDir = fs::canonical(fs::path(bin->PackPath).remove_filename());

    auto& ftracker = ShaderFileTracker::GetFileTracker();
    for (size_t i = 0; i < bin->NumShaders; ++i) {

        const ShaderBinary& curr_shader = bin->Shaders[i];
        for (size_t j = 0; j < curr_shader.NumShaderStages; ++j) {
            ShaderStage curr_stage{ curr_shader.StageIDs[j] };

            auto stored_last_write_time = fs::file_time_type::clock::duration{ curr_shader.LastWriteTimes[j] };
            fs::path stage_path{ curr_shader.Paths[j] };
            if (!fs::exists(stage_path)) {
                throw std::runtime_error("Given path in binary for a shader body string does not exist!");
            }
            auto actual_last_write_time = fs::last_write_time(stage_path).time_since_epoch();

            if (actual_last_write_time == stored_last_write_time) {
                ftracker.StageLastModificationTimes.emplace(curr_stage, stored_last_write_time);
                ftracker.FullSourceStrings.emplace(curr_stage, curr_shader.SourceStrings[j]);
                ftracker.Binaries.emplace(curr_stage, curr_shader.Binaries[j]);
                ftracker.AssemblyStrings.emplace(curr_stage, curr_shader.AssemblyStrs[j]);
                ftracker.RecompiledSourcesFromBinaries.emplace(curr_stage, curr_shader.RecompiledStrs[j]);
                ftracker.BodyPaths.emplace(curr_stage, stage_path);
            }
            else {
                LOG(WARNING) << "Loaded binary cannot be entirely used, as shader has been updated!";
                // need to also clear this item from the map
                ftracker.ShaderBodies.erase(curr_stage);
                ftracker.FullSourceStrings.erase(curr_stage);
                ftracker.Binaries.erase(curr_stage);
                ftracker.AssemblyStrings.erase(curr_stage);
                ftracker.RecompiledSourcesFromBinaries.erase(curr_stage);
                ftracker.StageLastModificationTimes[curr_stage] = fs::last_write_time(stage_path);
            }
        }

    }

}
