#include "yamlFile.hpp"
#include "yaml-cpp/yaml.h"
#include "../util/ResourceFormats.hpp"
#include "../util/ShaderFileTracker.hpp"
#include <filesystem>
#include <cassert>
#include "easyloggingpp/src/easylogging++.h"
#ifdef FindResource
#undef FindResource
#endif

namespace st {

    namespace fs = std::filesystem;

    constexpr static VkDescriptorType ARRAY_TYPE_FLAG_BITS = static_cast<VkDescriptorType>(1 << 16);
    constexpr VkDescriptorType MakeArrayType(VkDescriptorType type)
    {
        return static_cast<VkDescriptorType>(type | ARRAY_TYPE_FLAG_BITS);
    }

    static const std::unordered_map<std::string, VkDescriptorType> descriptor_type_from_str_map = {
        { "UniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        { "UniformBufferArray", MakeArrayType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) },
        { "DynamicUniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
        { "StorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
        { "StorageBufferArray", MakeArrayType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) },
        { "DynamicStorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC },
        { "StorageImage", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        { "StorageImageArray", MakeArrayType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) },
        { "UniformTexelBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
        { "UniformTexelBufferArray", MakeArrayType(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) },
        { "StorageTexelBuffer", VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER },
        { "StorageTexelBufferArray", MakeArrayType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) },
        { "Sampler", VK_DESCRIPTOR_TYPE_SAMPLER },
        { "SamplerArray", MakeArrayType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) },
        { "SampledImage", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { "SampledImageArray", MakeArrayType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) },
        { "CombinedImageSampler", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        { "CombinedImageSamplerArray", MakeArrayType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) },
        { "InputAttachment", VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT }
    };

    static const std::unordered_map<std::string, glsl_qualifier> qualifier_from_str_map = {
        { "coherent", glsl_qualifier::Coherent },
        { "readonly", glsl_qualifier::ReadOnly },
        { "writeonly", glsl_qualifier::WriteOnly },
        { "volatile", glsl_qualifier::Volatile },
        { "restrict", glsl_qualifier::Restrict }
    };
    
    glsl_qualifier singleQualifierFromString(const std::string& single_qualifier) {
        auto iter = qualifier_from_str_map.find(single_qualifier);
        if (iter != qualifier_from_str_map.cend()) {
            return iter->second;
        }
        else {
            return glsl_qualifier::InvalidQualifier;
        }
    }
    
    std::vector<glsl_qualifier> qualifiersFromString(std::string qualifiers_str) {
        // find if we have multiple qualifiers
        if (qualifiers_str.find_first_of(' ') != std::string::npos) {
            std::vector<std::string> substrings;

            while (!qualifiers_str.empty()) {
                size_t idx = qualifiers_str.find_first_of(' ');
                if (idx == std::string::npos) {
                    substrings.emplace_back(qualifiers_str);
                    qualifiers_str.clear();
                }
                else {
                    substrings.emplace_back(std::string{ qualifiers_str.cbegin(), qualifiers_str.cbegin() + idx });
                    qualifiers_str.erase(qualifiers_str.begin(), qualifiers_str.begin() + idx + 1);
                }
            }

            std::vector<glsl_qualifier> result;
            for (const auto& substr : substrings) {
                result.emplace_back(singleQualifierFromString(substr));
            }

            return result;
        }
        else {
            return std::vector<glsl_qualifier>{ singleQualifierFromString(qualifiers_str) };
        }
    }

    struct yamlFileImpl {
        yamlFileImpl(const char* fname) : rootFileNode{ YAML::LoadFile(fname) } {}
        ~yamlFileImpl() {}
        YAML::Node rootFileNode;
    };

    yamlFile::yamlFile(const char * fname) {
        try
        {
            impl = std::make_unique<yamlFileImpl>(fname);
        }
        catch (const std::exception & e)
        {
            LOG(ERROR) << "Failed to create/open YAML file: " << e.what();
            throw e;
        }
        parseGroups();
        parseResources();
        sortResourcesAndSetBindingIndices();
    }

    yamlFile::~yamlFile() {}

    ShaderResource* yamlFile::FindResource(const std::string& name) {

        auto find_single_resource = [&name](std::vector<st::ShaderResource>& resources)->ShaderResource* {
            for (auto iter = resources.begin(); iter != resources.end(); ++iter) {
                if (strcmp(iter->Name(), name.c_str()) == 0) {
                    return &(*iter);
                }
            }
            return nullptr;
        };

        for (auto& group : resourceGroups) {
            ShaderResource* result = find_single_resource(group.second);
            if (result != nullptr) {
                return result;
            }
        }

        return nullptr;
    }

    void yamlFile::parseGroups() {
        using namespace YAML;
        
        auto& file_node = impl->rootFileNode;
        if (!file_node["shader_groups"]) {
            throw std::runtime_error("YAML file had no shader groups specified!");
        }

        auto& groups = file_node["shader_groups"];

        for (auto iter = groups.begin(); iter != groups.end(); ++iter) {
            std::string group_name = iter->first.as<std::string>();

            auto add_stage = [this, &group_name](std::string shader_name, VkShaderStageFlagBits stage)->ShaderStage {
                if (stages.count(shader_name) == 0) {
                    auto iter = stages.emplace(shader_name, ShaderStage{ shader_name.c_str(), stage });
                    shaderGroups[group_name].emplace(iter.first->second);
                    return iter.first->second;
                }
                else {
                    shaderGroups[group_name].emplace(stages.at(shader_name));
                    return stages.at(shader_name);
                }
            };

            {
                std::vector<ShaderStage> stages_added;
                auto shaders = iter->second["Shaders"];

                if (shaders["Vertex"]) {
                    stages_added.emplace_back(add_stage(shaders["Vertex"].as<std::string>(), VK_SHADER_STAGE_VERTEX_BIT));
                }

                if (shaders["Geometry"]) {
                    stages_added.emplace_back(add_stage(shaders["Geometry"].as<std::string>(), VK_SHADER_STAGE_GEOMETRY_BIT));
                }

                if (shaders["Fragment"]) {
                    stages_added.emplace_back(add_stage(shaders["Fragment"].as<std::string>(), VK_SHADER_STAGE_FRAGMENT_BIT));
                }

                if (shaders["Compute"]) {
                    stages_added.emplace_back(add_stage(shaders["Compute"].as<std::string>(), VK_SHADER_STAGE_COMPUTE_BIT));
                }


                if (shaders["OptimizationDisabled"])
                {
                    auto& sft = ShaderFileTracker::GetFileTracker();
                    const bool disabled_value = shaders["OptimizationDisabled"].as<bool>();
                    for (const auto& stage : stages_added)
                    {
                        sft.StageOptimizationDisabled.emplace(stage, disabled_value);
                    }
                }

                if (iter->second["Extensions"]) {
                    std::vector<std::string> extension_strs;
                    for (const auto& ext : iter->second["Extensions"]) {
                        extension_strs.emplace_back(ext.as<std::string>());
                    }

                    for (auto& stage : stages_added) {
                        std::unique_copy(std::begin(extension_strs), std::end(extension_strs), std::back_inserter(stageExtensions[stage]));
                    }
                }
            }

            if (iter->second["Tags"]) {
                assert(iter->second["Tags"].IsSequence());
                for (const auto& tag : iter->second["Tags"]) {
                    groupTags[group_name].emplace_back(tag.as<std::string>());
                }
            }

        }

    }

    void yamlFile::sortResourcesAndSetBindingIndices()
    {
        for (auto& resource_block : resourceGroups)
        {

            // Used to put bindless or array resources at the end of the array
            auto resource_block_sort = [](const st::ShaderResource& rsrc0, const st::ShaderResource& rsrc1)
            {
                const bool rsrc0isArray = rsrc0.IsDescriptorArray();
                const bool rsrc1isArray = rsrc1.IsDescriptorArray();
                if (rsrc0isArray && !rsrc1isArray)
                {
                    // this is used as if it was <, so we want array resources to never be lesser than
                    return false;
                }
                else if (!rsrc0isArray && rsrc1isArray)
                {
                    return true;
                }
                else if (rsrc0isArray && rsrc1isArray)
                {
                    // Both are arrays. Unbounded arrays must always be at the end, and otherwise we'll sort by array size
                    return rsrc0.ArraySize() < rsrc1.ArraySize();
                }
                else
                {
                    // In cases where neither is an array, just use descriptor type since it'll generally favor putting samplers in the root
                    return rsrc0.DescriptorType() < rsrc1.DescriptorType();
                }
            };

            std::sort(resource_block.second.begin(), resource_block.second.end(), resource_block_sort);

            for (uint32_t i = 0; i < static_cast<uint32_t>(resource_block.second.size()); ++i)
            {
                resource_block.second[i].SetBindingIndex(i);
            }
        }

    }

    void yamlFile::parseResources() {
        using namespace YAML;

        if (!impl->rootFileNode["resource_groups"]) {
            throw std::runtime_error("YAML file had no resource groups!");
        }

        auto& groups = impl->rootFileNode["resource_groups"];

        for (auto iter = groups.begin(); iter != groups.end(); ++iter) {
            std::string group_name = iter->first.as<std::string>();
            auto& curr_node = iter->second;

            auto& group_resources = resourceGroups[group_name];

            for (auto member_iter = curr_node.begin(); member_iter != curr_node.end(); ++member_iter) {

                std::string rsrc_name = member_iter->first.as<std::string>();
                auto& rsrc_node = member_iter->second;

                if (!rsrc_node["Type"]) {
                    throw std::runtime_error("Failed to find type field in resource entry in YAML file.");
                }

                auto type_iter = descriptor_type_from_str_map.find(rsrc_node["Type"].as<std::string>());
                if (type_iter == descriptor_type_from_str_map.end()) {
                    throw std::runtime_error("Could not match type str to valid VkDescriptorType value!");
                }

                ShaderResource rsrc;
                rsrc.SetParentGroupName(group_name.c_str());
                rsrc.SetName(rsrc_name.c_str());
                if (const VkDescriptorType rsrc_type = type_iter->second; rsrc_type & ARRAY_TYPE_FLAG_BITS)
                {
                    const VkDescriptorType actual_type = static_cast<VkDescriptorType>(uint32_t(rsrc_type) & ~uint32_t(ARRAY_TYPE_FLAG_BITS));
                    rsrc.SetType(actual_type);
                    rsrc.SetDescriptorArray(true); // defaults false
                    if (rsrc_node["ArraySize"])
                    {
                        const uint32_t arraySz = rsrc_node["ArraySize"].as<uint32_t>();
                        rsrc.SetArraySize(arraySz);
                    }
                    else
                    {
                        // If not provided, it's an unbounded descriptor array: set size as largest uint val so we 
                        // can check against it elsewhere (at higher levels, mostly)
                        rsrc.SetArraySize(std::numeric_limits<uint32_t>::max());
                    }
                }
                else
                {
                    rsrc.SetType(type_iter->second);
                }

                if (rsrc_node["ImageSubtype"]) {
                    auto subtype_str = rsrc_node["ImageSubtype"].as<std::string>();
                    rsrc.SetImageSamplerSubtype(subtype_str.c_str());
                }
                else if (rsrc_node["SamplerSubtype"]) {
                    auto subtype_str = rsrc_node["SamplerSubtype"].as<std::string>();
                    rsrc.SetImageSamplerSubtype(subtype_str.c_str());
                }

                if (rsrc_node["Format"]) {
                    rsrc.SetFormat(VkFormatFromString(rsrc_node["Format"].as<std::string>()));
                }

                if (rsrc_node["Members"]) {
                    rsrc.SetMembersStr(rsrc_node["Members"].as<std::string>().c_str());
                }

                if (rsrc_node["Tags"]) {
                    std::vector<std::string> tag_strs;
                    for (auto& tag : rsrc_node["Tags"]) {
                        tag_strs.emplace_back(tag.as<std::string>());
                    }
                    std::vector<const char*> tag_ptrs;
                    for (auto& tag_str : tag_strs) {
                        tag_ptrs.emplace_back(tag_str.c_str());
                    }
                    rsrc.SetTags(tag_ptrs.size(), tag_ptrs.data());
                }

                if (rsrc_node["Qualifiers"]) {
                    std::vector<glsl_qualifier> qualifiers = qualifiersFromString(rsrc_node["Qualifiers"].as<std::string>());
                    rsrc.SetQualifiers(qualifiers.size(), qualifiers.data());
                }

                group_resources.emplace_back(std::move(rsrc));

            }
        }
    }

}
