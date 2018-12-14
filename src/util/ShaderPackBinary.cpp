#include "ShaderPackBinary.hpp"
#include "ShaderFileTracker.hpp"
#include "core/Shader.hpp"
#include <vector>

namespace st {

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

        // now get last write times of files
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

}
