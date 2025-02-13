#include "yamlFile.hpp"
#include "yaml-cpp/yaml.h"
#include "../util/ResourceFormats.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "shaderc/env.h"
#include "shaderc/shaderc.h"
#include <filesystem>
#include <cassert>
#include <iterator>

namespace st
{

    namespace fs = std::filesystem;

    constexpr static VkDescriptorType ARRAY_TYPE_FLAG_BITS = static_cast<VkDescriptorType>(1 << 16);
    constexpr VkDescriptorType MakeArrayType(VkDescriptorType type) noexcept
    {
        return static_cast<VkDescriptorType>(type | ARRAY_TYPE_FLAG_BITS);
    }

    static const std::unordered_map<std::string, VkDescriptorType> descriptor_type_from_str_map =
    {
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

    static const std::unordered_map<std::string, glsl_qualifier> qualifier_from_str_map =
    {
        { "coherent", glsl_qualifier::Coherent },
        { "readonly", glsl_qualifier::ReadOnly },
        { "writeonly", glsl_qualifier::WriteOnly },
        { "volatile", glsl_qualifier::Volatile },
        { "restrict", glsl_qualifier::Restrict }
    };

    static const std::unordered_map<std::string, ShaderCompilerOptions::OptimizationLevel> optimization_level_from_str_map =
    {
        { "Disabled", ShaderCompilerOptions::OptimizationLevel::Disabled },
        { "Size", ShaderCompilerOptions::OptimizationLevel::Size },
        { "Performance", ShaderCompilerOptions::OptimizationLevel::Performance }
    };

    static const std::unordered_map<std::string, ShaderCompilerOptions::TargetVersionEnum> env_version_from_str_map =
    {
        { "Vulkan_1_0", ShaderCompilerOptions::TargetVersionEnum::Vulkan1_0 },
        { "Vulkan_1_1", ShaderCompilerOptions::TargetVersionEnum::Vulkan1_1 },
        { "Vulkan_1_2", ShaderCompilerOptions::TargetVersionEnum::Vulkan1_2 },
        { "Vulkan_1_3", ShaderCompilerOptions::TargetVersionEnum::Vulkan1_3 },
        { "Vulkan_1_4", ShaderCompilerOptions::TargetVersionEnum::Vulkan1_4 },
        { "VulkanLatest", ShaderCompilerOptions::TargetVersionEnum::VulkanLatest }
    };

    glsl_qualifier singleQualifierFromString(const std::string& single_qualifier)
    {
        auto iter = qualifier_from_str_map.find(single_qualifier);
        if (iter != qualifier_from_str_map.cend())
        {
            return iter->second;
        }
        else
        {
            return glsl_qualifier::InvalidQualifier;
        }
    }

    std::vector<glsl_qualifier> qualifiersFromString(std::string qualifiers_str)
    {
        // find if we have multiple qualifiers
        if (qualifiers_str.find_first_of(' ') != std::string::npos)
        {
            std::vector<std::string> substrings;

            while (!qualifiers_str.empty())
            {
                size_t idx = qualifiers_str.find_first_of(' ');
                if (idx == std::string::npos)
                {
                    substrings.emplace_back(qualifiers_str);
                    qualifiers_str.clear();
                }
                else
                {
                    substrings.emplace_back(std::string{ qualifiers_str.cbegin(), qualifiers_str.cbegin() + idx });
                    qualifiers_str.erase(qualifiers_str.begin(), qualifiers_str.begin() + idx + 1);
                }
            }

            std::vector<glsl_qualifier> result;
            for (const auto& substr : substrings)
            {
                result.emplace_back(singleQualifierFromString(substr));
            }

            return result;
        }
        else
        {
            return std::vector<glsl_qualifier>{ singleQualifierFromString(qualifiers_str) };
        }
    }

    // Impl here exists just so we can keep the YAML library entirely in this source, not in a single darn header!
    struct yamlFileImpl
    {
        yamlFileImpl(const char* fname) : rootFileNode{ YAML::LoadFile(fname) } {}
        ~yamlFileImpl() {}
        YAML::Node rootFileNode;
    };

    yamlFile::yamlFile(const char* fname, Session& session)
    {
        try
        {
            impl = std::make_unique<yamlFileImpl>(fname);
        }
        catch (const std::exception& e)
        {
            throw e;
        }

        ShaderToolsErrorCode parseGroupsStatus = parseGroups(session);
        ShaderToolsErrorCode parseResourcesStatus = parseResources(session);

        if (parseGroupsStatus == ShaderToolsErrorCode::Success &&
            parseResourcesStatus == ShaderToolsErrorCode::Success) 
        {
            sortResourcesAndSetBindingIndices();
        }
    }


	yamlFile::yamlFile(yamlFile&& other) noexcept :
        stages{ std::move(other.stages) },
        shaderGroups{ std::move(other.shaderGroups) },
        groupTags{ std::move(other.groupTags) },
        stageExtensions{ std::move(other.stageExtensions) },
        resourceGroups{ std::move(other.resourceGroups) },
        compilerOptions{ std::move(other.compilerOptions) },
        packName{ std::move(other.packName) }
	{
	}


	yamlFile& yamlFile::operator=(yamlFile&& other) noexcept
	{
        stages = std::move(other.stages);
        shaderGroups = std::move(other.shaderGroups);
        groupTags = std::move(other.groupTags);
        stageExtensions = std::move(other.stageExtensions);
        resourceGroups = std::move(other.resourceGroups);
        compilerOptions = std::move(other.compilerOptions);
        packName = std::move(other.packName);
        return *this;
	}

	yamlFile::~yamlFile() {}

    // Used during reflection stage to make sure mappings of client resources to compiled binary resources match
    ShaderResource* yamlFile::FindResource(const std::string& name)
    {
        // Despite primitive linear search, probably not an issue. Resource count usually no more than 8-10?
        auto find_single_resource = [&name](std::vector<st::ShaderResource>& resources)->ShaderResource*
        {
            for (auto iter = resources.begin(); iter != resources.end(); ++iter)
            {
                if (strcmp(iter->Name(), name.c_str()) == 0)
                {
                    return &(*iter);
                }
            }
            return nullptr;
        };

        for (auto& group : resourceGroups)
        {
            ShaderResource* result = find_single_resource(group.second);
            if (result != nullptr)
            {
                return result;
            }
        }

        return nullptr;
    }

    ShaderToolsErrorCode yamlFile::parseGroups(Session& session)
    {
        using namespace YAML;

        auto& file_node = impl->rootFileNode;
        if (!file_node["shader_groups"])
        {
            session.AddError(this, ShaderToolsErrorSource::Parser, ShaderToolsErrorCode::ParserHadNoShaderGroups, nullptr);
            return ShaderToolsErrorCode::ParserHadNoShaderGroups;
        }

        auto groups = file_node["shader_groups"];

        for (auto iter = groups.begin(); iter != groups.end(); ++iter)
        {
            std::string group_name = iter->first.as<std::string>();

            auto add_stage = [this, &group_name](std::string shader_name, VkShaderStageFlags stage)->ShaderStage
            {
                if (stages.count(shader_name) == 0)
                {
                    auto iter = stages.emplace(shader_name, ShaderStage{ shader_name.c_str(), stage });
                    shaderGroups[group_name].emplace(iter.first->second);
                    return iter.first->second;
                }
                else
                {
                    shaderGroups[group_name].emplace(stages.at(shader_name));
                    return stages.at(shader_name);
                }
            };

            {
                std::vector<ShaderStage> stages_added;
                auto shaders = iter->second["Shaders"];

                if (std::distance(shaders.begin(), shaders.end()) == 0)
                {
                    return ShaderToolsErrorCode::ParserYamlFileHadNoShadersInGroup;
                }

                if (shaders["Vertex"])
                {
                    stages_added.emplace_back(add_stage(shaders["Vertex"].as<std::string>(), VK_SHADER_STAGE_VERTEX_BIT));
                }

                if (shaders["Geometry"])
                {
                    stages_added.emplace_back(add_stage(shaders["Geometry"].as<std::string>(), VK_SHADER_STAGE_GEOMETRY_BIT));
                }

                if (shaders["Fragment"])
                {
                    stages_added.emplace_back(add_stage(shaders["Fragment"].as<std::string>(), VK_SHADER_STAGE_FRAGMENT_BIT));
                }

                if (shaders["Compute"])
                {
                    stages_added.emplace_back(add_stage(shaders["Compute"].as<std::string>(), VK_SHADER_STAGE_COMPUTE_BIT));
                }

                // Defined inline with list of shaders. Disables optimization for all shaders in this group (so, compute/render shaders)
                if (shaders["OptimizationDisabled"])
                {
                    auto& sft = ShaderFileTracker::GetFileTracker();
                    const bool disabled_value = shaders["OptimizationDisabled"].as<bool>();
                    for (const auto& stage : stages_added)
                    {
                        sft.StageOptimizationDisabled.emplace(stage, disabled_value);
                    }
                }

                if (iter->second["Extensions"])
                {
                    std::vector<std::string> extension_strs;
                    for (const auto& ext : iter->second["Extensions"])
                    {
                        extension_strs.emplace_back(ext.as<std::string>());
                    }

                    for (auto& stage : stages_added)
                    {
                        std::unique_copy(std::begin(extension_strs), std::end(extension_strs), std::back_inserter(stageExtensions[stage]));
                    }
                }
            }

            // Tags are our way for users to insert custom tags that we don't act on: we just preserve them throughout the process,
            // adding them to the completed output. Makes it possible to define shaderpacks as closer to pipeline specs
            if (iter->second["Tags"])
            {

                if (!iter->second["Tags"].IsSequence())
                {
                    return ShaderToolsErrorCode::ParserYamlFileHadInvalidOrEmptyTagsArray;
                }
                else
                {
                    for (const auto& tag : iter->second["Tags"])
                    {
                        groupTags[group_name].emplace_back(tag.as<std::string>());
                    }
                }
            }

        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode yamlFile::parseCompilerOptions(Session& session)
    {
        using namespace YAML;
        ShaderToolsErrorCode mostRecentError = ShaderToolsErrorCode::Success;

        auto& file_node = impl->rootFileNode;
        if (!file_node["compiler_options"])
        {
            // TODO: This is somewhere we could probably start using warnings
            compilerOptions = ShaderCompilerOptions();
        }
        else
        {
            const YAML::Node compiler_options = file_node["compiler_options"];

            if (compiler_options["GenerateDebugInfo"])
            {
                compilerOptions.GenerateDebugInfo = compiler_options["GenerateDebugInfo"].as<bool>();
            }

            if (compiler_options["Optimization"])
            {
                const std::string opt_str = compiler_options["Optimization"].as<std::string>();
                auto opt_iter = optimization_level_from_str_map.find(opt_str);
                if (opt_iter == optimization_level_from_str_map.end())
                {
                    mostRecentError = ShaderToolsErrorCode::ParserYamlFileHadInvalidOptimizationLevel;
                    session.AddError(this, ShaderToolsErrorSource::Parser, mostRecentError, opt_str.c_str());
                }
                else
                {
                    compilerOptions.Optimization = opt_iter->second;
                }
            }

            if (compiler_options["TargetVersion"])
            {
                const std::string target_version_str = compiler_options["TargetVersion"].as<std::string>();
                auto target_version_iter = env_version_from_str_map.find(target_version_str);
                if (target_version_iter == env_version_from_str_map.end())
                {
                    mostRecentError = ShaderToolsErrorCode::ParserYamlFileHadInvalidTargetVersion;
                    session.AddError(this, ShaderToolsErrorSource::Parser, mostRecentError, target_version_str.c_str());
                }
                else
                {
                    compilerOptions.TargetVersion = target_version_iter->second;
                }
            }

        }

        return mostRecentError;
    }

    // Sorts resources by type, and sets binding indices
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

    ShaderToolsErrorCode yamlFile::parseResources(Session& session)
    {
        using namespace YAML;
        // Store most recent error, so we can return that one. We will store every error that occurs in the session,
        // so that users can see all errors but we still stop processing after any error occurs.
        ShaderToolsErrorCode mostRecentError = ShaderToolsErrorCode::Success;

        if (!impl->rootFileNode["resource_groups"])
        {
            mostRecentError = ShaderToolsErrorCode::ParserHadNoResourceGroups;
        }

        auto groups = impl->rootFileNode["resource_groups"];

        for (auto iter = groups.begin(); iter != groups.end(); ++iter)
        {
            std::string group_name = iter->first.as<std::string>();
            std::vector<st::ShaderResource>& group_resources = resourceGroups[group_name];

            auto& curr_node = iter->second;
            for (auto member_iter = curr_node.begin(); member_iter != curr_node.end(); ++member_iter)
            {

                std::string rsrc_name = member_iter->first.as<std::string>();
                auto& rsrc_node = member_iter->second;

                if (!rsrc_node["Type"])
                {
                    const std::string errorMsg = "Group '" + group_name + "' has resource '" + rsrc_name + "' with no type specifier.";
                    session.AddError(this, ShaderToolsErrorSource::Parser, ShaderToolsErrorCode::ParserMissingResourceTypeSpecifier, errorMsg.c_str());
                    mostRecentError = ShaderToolsErrorCode::ParserMissingResourceTypeSpecifier;
                }

                auto type_iter = descriptor_type_from_str_map.find(rsrc_node["Type"].as<std::string>());
                if (type_iter == descriptor_type_from_str_map.end())
                {
                    const std::string errorMsg = "Group '" + group_name + "' has resource '" + rsrc_name + "' with type specifier '" + rsrc_node["Type"].as<std::string>() + "' which has no corresponding Vulkan equivalent.";
                    session.AddError(this, ShaderToolsErrorSource::Parser, ShaderToolsErrorCode::ParserResourceTypeSpecifierNoVulkanEquivalent, errorMsg.c_str());
                    mostRecentError = ShaderToolsErrorCode::ParserResourceTypeSpecifierNoVulkanEquivalent;
                }

                ShaderResource rsrc;
                rsrc.SetParentGroupName(group_name.c_str());
                rsrc.SetName(rsrc_name.c_str());
                if (const VkDescriptorType rsrc_type = type_iter->second; rsrc_type & ARRAY_TYPE_FLAG_BITS)
                {
                    // We shove these bits in to make it clear elsewhere that something is a descriptor array
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

                if (rsrc_node["ImageSubtype"])
                {
                    auto subtype_str = rsrc_node["ImageSubtype"].as<std::string>();
                    rsrc.SetImageSamplerSubtype(subtype_str.c_str());
                }
                else if (rsrc_node["SamplerSubtype"])
                {
                    auto subtype_str = rsrc_node["SamplerSubtype"].as<std::string>();
                    rsrc.SetImageSamplerSubtype(subtype_str.c_str());
                }

                if (rsrc_node["Format"])
                {
                    VkFormat format = VkFormatFromString(rsrc_node["Format"].as<std::string>());
                    if (format == VK_FORMAT_UNDEFINED)
                    {
                        const std::string errorMsg = "Group '" + group_name + "' has resource '" + rsrc_name + "' with format '" + rsrc_node["Format"].as<std::string>() + "' which has no corresponding Vulkan equivalent.";
                        session.AddError(this, ShaderToolsErrorSource::Parser, ShaderToolsErrorCode::ParserResourceFormatNoVulkanEquivalent, errorMsg.c_str());
                        mostRecentError = ShaderToolsErrorCode::ParserResourceFormatNoVulkanEquivalent;
                    }
                    rsrc.SetFormat(format);
                }

                if (rsrc_node["Members"])
                {
                    rsrc.SetMembersStr(rsrc_node["Members"].as<std::string>().c_str());
                }

                if (rsrc_node["Tags"])
                {
                    std::vector<std::string> tag_strs;
                    for (auto tag : rsrc_node["Tags"])
                    {
                        tag_strs.emplace_back(tag.as<std::string>());
                    }

                    std::vector<const char*> tag_ptrs;
                    for (auto& tag_str : tag_strs)
                    {
                        tag_ptrs.emplace_back(tag_str.c_str());
                    }

                    rsrc.SetTags(tag_ptrs.size(), tag_ptrs.data());
                }

                if (rsrc_node["Qualifiers"])
                {
                    std::vector<glsl_qualifier> qualifiers = qualifiersFromString(rsrc_node["Qualifiers"].as<std::string>());
                    rsrc.SetQualifiers(qualifiers.size(), qualifiers.data());
                }

                group_resources.emplace_back(std::move(rsrc));

            }
        }

        return mostRecentError;
    }

}
