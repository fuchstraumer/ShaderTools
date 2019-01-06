#include "yamlFile.hpp"
#include "yaml-cpp/yaml.h"
#include <experimental/filesystem>

namespace st {

    namespace fs = std::experimental::filesystem;

    static const std::unordered_map<std::string, VkDescriptorType> descriptor_type_from_str_map = {
        { "UniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        { "DynamicUniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
        { "StorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
        { "DynamicStorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC },
        { "StorageImage", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        { "UniformTexelBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
        { "StorageTexelBuffer", VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER },
        { "Sampler", VK_DESCRIPTOR_TYPE_SAMPLER },
        { "SampledImage", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { "CombinedImageSampler", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        { "InputAttachment", VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT }
    };

    static const std::unordered_map<std::string, glsl_qualifier> qualifier_from_str_map = {
        { "coherent", glsl_qualifier::Coherent },
        { "readonly", glsl_qualifier::ReadOnly },
        { "writeonly", glsl_qualifier::WriteOnly },
        { "volatile", glsl_qualifier::Volatile },
        { "restrict", glsl_qualifier::Restrict }
    };

    std::string extractNameCheckPathValid(const YAML::Node& node) {
        fs::path file_path(node.as<std::string>());
        return file_path.filename().string();
    }
    
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

    yamlFile::yamlFile(const char * fname) : impl{ std::make_unique<yamlFileImpl>(fname) } {
        parseGroups();
        parseResources();
    }

    yamlFile::~yamlFile() {}

    void yamlFile::parseGroups() {
        using namespace YAML;
        
        auto& file_node = impl->rootFileNode;
        if (!file_node["shader_groups"]) {
            throw std::runtime_error("YAML file had no shader groups specified!");
        }

        auto& groups = file_node["shader_groups"];

        for (auto iter = groups.begin(); iter != groups.end(); ++iter) {
            std::string group_name = iter->first.as<std::string>();

            auto add_stage = [this, &group_name](std::string shader_name, VkShaderStageFlagBits stage) {
                if (stages.count(shader_name) == 0) {
                    auto iter = stages.emplace(shader_name, ShaderStage{ shader_name.c_str(), stage });
                    shaderGroups[group_name].emplace(iter.first->second);
                }
                else {
                    shaderGroups[group_name].emplace(stages.at(shader_name));
                }
            };

            {
                auto shaders = iter->second["Shaders"];
                if (shaders["Vertex"]) {
                    add_stage(extractNameCheckPathValid(shaders["Vertex"]), VK_SHADER_STAGE_VERTEX_BIT);
                }

                if (shaders["Geometry"]) {
                    add_stage(extractNameCheckPathValid(shaders["Geometry"]), VK_SHADER_STAGE_GEOMETRY_BIT);
                }

                if (shaders["Fragment"]) {
                    add_stage(extractNameCheckPathValid(shaders["Fragment"]), VK_SHADER_STAGE_FRAGMENT_BIT);
                }

                if (shaders["Compute"]) {
                    add_stage(extractNameCheckPathValid(shaders["Compute"]), VK_SHADER_STAGE_COMPUTE_BIT);
                }
            }

            if (iter->second["Extensions"]) {
                std::vector<std::string> extension_strs;
                for (const auto& ext : iter->second["Extensions"]) {
                    extension_strs.emplace_back(ext.as<std::string>());
                }
                std::unique_copy(std::begin(extension_strs), std::end(extension_strs), std::back_inserter(groupExtensions[group_name]));
            }

            if (iter->second["Tags"]) {
                for (const auto& tag : iter->second["Tags"]) {
                    groupTags[group_name].emplace_back(tag.as<std::string>());
                }
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
                rsrc.SetType(type_iter->second);


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

                rsrc.SetBindingIndex(group_resources.size());
                group_resources.emplace_back(std::move(rsrc));

            }
        }
    }

}
