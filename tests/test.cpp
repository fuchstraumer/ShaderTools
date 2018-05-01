#include <iostream>
#include <vector>
#include "core/ShaderGroup.hpp"
#include "core/ShaderPack.hpp"
#include <array>
#include "easyloggingpp/src/easylogging++.h"
INITIALIZE_EASYLOGGINGPP
int screen_x() {
    return 1920;
}

int screen_y() {
    return 1080;
}

double z_near() {
    return 0.1;
}

double z_far() {
    return 3000.0;
}

double fov_y() {
    return 75.0;
}

int main(int argc, char* argv[]) {
    using namespace st; 
    
    ShaderGroup::RetrievalCallbacks.GetScreenSizeX = &screen_x;
    ShaderGroup::RetrievalCallbacks.GetScreenSizeY = &screen_y;
    ShaderGroup::RetrievalCallbacks.GetZNear = &z_near;
    ShaderGroup::RetrievalCallbacks.GetZFar = &z_far;
    ShaderGroup::RetrievalCallbacks.GetFOVY = &fov_y;
    
    ShaderPack pack("../fragments/volumetric_forward/ShaderPack.lua");
    std::vector<std::string> group_names;
    {
        auto names = pack.GetGroupNames();
        for (size_t i = 0; i < names.NumStrings; ++i) {
            group_names.emplace_back(names.Strings[i]);
        }

    }

    using bindings_map_t = std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>;
    using pack_bindings_map_t = std::unordered_map<std::string, bindings_map_t>;
    using pack_resources_per_group_t = std::unordered_map<std::string, std::vector<std::string>>;
    pack_bindings_map_t all_bindings;

    for (const auto& group_name : group_names) {
        ShaderGroup* group = pack.GetShaderGroup(group_name.c_str());
        size_t num_sets = group->GetNumSetsRequired();
        bindings_map_t group_bindings;
        for (size_t i = 0; i < num_sets; ++i) {
            size_t num_bindings = 0;
            group->GetSetLayoutBindings(i, &num_bindings, nullptr);
            std::vector<VkDescriptorSetLayoutBinding> bindings(num_bindings);
            group->GetSetLayoutBindings(i, &num_bindings, bindings.data());
            group_bindings.emplace(static_cast<uint32_t>(i), bindings);
        }

        std::vector<std::string> block_names;
        {
            auto retrieved_block_names = group->GetUsedResourceBlocks();
            for (size_t i = 0; i < retrieved_block_names.NumStrings; ++i) {
                block_names.emplace_back(retrieved_block_names.Strings[i]);
            }
        }

        all_bindings.emplace(group_name, group_bindings);
    }

    std::cerr << "Tests complete.";
}