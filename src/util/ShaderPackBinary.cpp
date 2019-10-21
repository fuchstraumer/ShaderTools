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

namespace st
{

    namespace fs = std::filesystem;
    constexpr static uint32_t SHADER_TOOLS_VERSION = VK_MAKE_VERSION(0, 5, 0);
    constexpr static uint32_t SHADER_BINARY_MAGIC_VALUE{ 0x70cd20ae };
    constexpr static uint32_t SHADER_PACK_BINARY_MAGIC_VALUE{ 0x9db3bb66 };

    using BitseryBuffer = std::vector<uint8_t>;
    using OutputAdapter = bitsery::OutputBufferAdapter<BitseryBuffer>;
    using InputAdapter = bitsery::InputBufferAdapter<BitseryBuffer>;

    struct ShaderStageBinary
    {
        ShaderStageBinary() = default;
        ShaderStageBinary(const ShaderStage& stage);
        uint64_t ID{ std::numeric_limits<uint64_t>::max() };
        int64_t LastWriteTime{ std::numeric_limits<int64_t>::max() };
        std::string BodyPath;
        std::string SourceStr;
        std::vector<uint32_t> Binary;
    };

    ShaderStageBinary::ShaderStageBinary(const ShaderStage& stage) : ID(stage.ID)
    {
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        const fs::path& stage_path = ftracker.BodyPaths.at(stage);
        BodyPath = stage_path.string();

        fs::file_time_type full_last_write_time = fs::last_write_time(stage_path);
        auto true_time = full_last_write_time.time_since_epoch();
        LastWriteTime = int64_t(true_time.count());

        SourceStr = ftracker.FullSourceStrings.at(stage);
        Binary = ftracker.Binaries.at(stage);
    }

    struct ShaderBinary
    {
        ShaderBinary() = default;
        ShaderBinary(const Shader* src);
        uint32_t ShaderBinaryMagic{ SHADER_BINARY_MAGIC_VALUE };
        uint32_t NumShaderStages;
        std::vector<uint64_t> StageIDs;
    };

    ShaderBinary::ShaderBinary(const Shader* src)
    {

        size_t num_stages{ 0 };
        src->GetShaderStages(&num_stages, nullptr);
        std::vector<st::ShaderStage> stages(num_stages, ShaderStage(0u));
        src->GetShaderStages(&num_stages, stages.data());

        NumShaderStages = static_cast<uint32_t>(num_stages);
        StageIDs.resize(num_stages, 0u);

        auto& ftracker = ShaderFileTracker::GetFileTracker();
        for (size_t i = 0; i < num_stages; ++i)
        {
            StageIDs[i] = stages[i].ID;
        }

    }

    struct ShaderPackBinary
    {
        ShaderPackBinary() = default;
        ShaderPackBinary(const ShaderPack* src);
        uint32_t MagicBits{ SHADER_PACK_BINARY_MAGIC_VALUE };
        uint32_t ShaderToolsVersion{ 0 };
        std::string PackPath{};
        uint32_t NumStages{ 0u };
        std::vector<ShaderStageBinary> Stages{};
        uint32_t NumShaders{ 0u };
        std::vector<ShaderBinary> Shaders{};
        uint32_t NumResourceGroups{ 0u };
    };

    ShaderPackBinary::ShaderPackBinary(const ShaderPack* initial_pack) : ShaderToolsVersion{ SHADER_TOOLS_VERSION }
    {
        const ShaderPackImpl* src = initial_pack->impl.get();
        PackPath = src->packPath;

        NumStages = static_cast<uint32_t>(src->filePack->stages.size());
        for (const auto& stage : src->filePack->stages)
        {
            Stages.emplace_back(stage.second);
        }

        for (const auto& shader_group : src->groups)
        {
            Shaders.emplace_back(ShaderBinary(shader_group.second.get()));
        }

        NumShaders = static_cast<uint32_t>(Shaders.size());

    }

    template<typename S>
    void serialize(S& s, ShaderStageBinary& stage)
    {
        s.value8b(stage.ID);
        s.value8b(stage.LastWriteTime);
        s.text1b(stage.BodyPath, 512);
        LOG_IF(stage.SourceStr.size() > 32768, WARNING) << "Stage full source string is over 32768 characters, half the max available size!";
        s.text1b(stage.SourceStr, 65536);
        s.container4b(stage.Binary, 65536);
    }

    template<typename S>
    void serialize(S& s, ShaderBinary& bin)
    {
        s.value4b(bin.ShaderBinaryMagic);
        s.value4b(bin.NumShaderStages);
        s.container8b(bin.StageIDs, 8);
    }

    template<typename S>
    void serialize(S& s, ShaderPackBinary& pack)
    {
        s.value4b(pack.MagicBits);
        s.value4b(pack.ShaderToolsVersion);
        s.text1b(pack.PackPath, 512);
        s.value4b(pack.NumStages);
        s.container(pack.Stages, 64, [&s](ShaderStageBinary& stg) { serialize<S>(s, stg); });
        s.value4b(pack.NumShaders);
        s.container(pack.Shaders, 32, [&s](ShaderBinary& bin) { serialize<S>(s, bin); });
    }

    ShaderBinary* CreateShaderBinary(const Shader* src)
    {
        ShaderBinary* result = new ShaderBinary(src);
        return result;
    }

    void DestroyShaderBinary(ShaderBinary * binary)
    {
        delete binary;
    }

    ShaderPackBinary* CreateShaderPackBinary(const ShaderPack* src)
    {
        ShaderPackBinary* result = new ShaderPackBinary(src);
        return result;
    }

    void DestroyShaderPackBinary(ShaderPackBinary* shader_pack)
    {
        delete shader_pack;
    }

    ShaderPackBinary* LoadShaderPackBinary(const char* fname)
    {

        std::ifstream input_stream(fname, std::ios::binary);
        if (!input_stream.is_open())
        {
            LOG(ERROR) << "Failed to open output stream for loading shader pack binary!";
            throw std::runtime_error("Failed to open output stream for loading shader pack binary!");
        }

        ShaderPackBinary* result = new ShaderPackBinary;
        auto deserialized_state = bitsery::quickDeserialization<bitsery::InputStreamAdapter>(input_stream, *result);
        if (deserialized_state.first != bitsery::ReaderError::NoError)
        {
            LOG(ERROR) << "Error deserializing shader pack binary file!";
            throw std::runtime_error("Error deserializing shader pack binary file!");
        }

        return result;
    }

    void SaveBinaryToFile(ShaderPackBinary* binary, const char* fname)
    {
        
        std::ofstream output_stream(fname, std::ios::binary | std::ios::trunc);
        if (!output_stream.is_open())
        {
            LOG(ERROR) << "Failed to open output stream for writing shader pack binary!";
            throw std::runtime_error("Failed to open output stream for writing shader pack binary!");
        }

        bitsery::Serializer<bitsery::OutputBufferedStreamAdapter> serializer{ output_stream };
        serializer.object(*binary);
        bitsery::AdapterAccess::getWriter(serializer).flush();
        output_stream.close();

    }

}

void st::Experimental::LoadPackFromBinary(ShaderPackImpl* pack, ShaderPackBinary* bin)
{

    if (bin->ShaderToolsVersion != SHADER_TOOLS_VERSION)
    {
        LOG(WARNING) << "ShaderToolsVersion in loaded binary does not match: loading may fault or cause errors!";
    }

    pack->packPath = bin->PackPath;

    if (!fs::exists(fs::path(pack->packPath)))
    {
        LOG(WARNING) << "Path script path stored in binary doesn't exist!";
    }

    pack->createPackScript(pack->packPath.c_str());
    fs::path working_dir{ pack->packPath };
    pack->workingDir = fs::canonical(working_dir.remove_filename());

    auto& ftracker = ShaderFileTracker::GetFileTracker();
    for (auto&& stage : bin->Stages)
    {
        ftracker.BodyPaths.emplace(stage.ID, std::move(stage.BodyPath));
        ftracker.StageLastModificationTimes.emplace(stage.ID, fs::file_time_type::clock::duration{ stage.LastWriteTime });
        ftracker.FullSourceStrings.emplace(stage.ID, std::move(stage.SourceStr));
        ftracker.Binaries.emplace(stage.ID, std::move(stage.Binary));
    }

}
