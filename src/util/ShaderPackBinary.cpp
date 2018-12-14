#include "ShaderPackBinary.hpp"
#include "core/Shader.hpp"
#include <vector>

namespace st {

    void CreateShaderBinary(const Shader* src, ShaderBinary* binary_dst) {

        size_t num_stages{ 0 };
        src->GetShaderStages(&num_stages, nullptr);
        std::vector<st::ShaderStage> stages(num_stages, st::ShaderStage("NULL", VK_SHADER_STAGE_ALL));
        src->GetShaderStages(&num_stages, stages.data());

        binary_dst->NumShaderStages = num_stages;
        binary_dst->StageIDs = new uint64_t[num_stages];

        for (size_t i = 0; i < num_stages; ++i) {
            binary_dst->StageIDs[i] = stages[i].ID;
        }


    }

}
