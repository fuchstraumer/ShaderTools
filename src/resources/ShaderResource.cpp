#include "resources/ShaderResource.hpp"
#include "common/UtilityStructs.hpp"
#include <set>
#include <unordered_map>

namespace st
{

    class ShaderResourceImpl
    {
    public:

        ShaderResourceImpl() = default;
        ~ShaderResourceImpl() {};
        ShaderResourceImpl(const ShaderResourceImpl& other) noexcept = default;
        ShaderResourceImpl(ShaderResourceImpl&& other) noexcept = default;
        ShaderResourceImpl& operator=(const ShaderResourceImpl& other) noexcept = default;
        ShaderResourceImpl& operator=(ShaderResourceImpl&& other) noexcept = default;

        uint32_t bindingIdx{ std::numeric_limits<uint32_t>::max() };
        std::string name{ "" };
        VkFormat format{ VK_FORMAT_UNDEFINED };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        size_t inputAttachmentIdx{ std::numeric_limits<size_t>::max() };
        std::string parentSetName{ "" };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        std::string imageSamplerSubtype{ "2D" };
        std::string membersStr;
        std::vector<ShaderResourceSubObject> members;
        std::vector<std::string> tags;
        std::set<glsl_qualifier> qualifiers;
        std::unordered_map<std::string, std::set<glsl_qualifier>> perUsageQualifiers;
        bool needsMipMaps{ false };
        bool isDescriptorArray{ false };
        uint32_t arraySize{ 0u };

    };

    ShaderResource::ShaderResource() : impl(std::make_unique<ShaderResourceImpl>()) {}

    ShaderResource::~ShaderResource() {}

    ShaderResource::ShaderResource(const ShaderResource & other) noexcept : impl(std::make_unique<ShaderResourceImpl>(*other.impl)) { }

    ShaderResource::ShaderResource(ShaderResource && other) noexcept : impl(std::move(other.impl)) {}

    ShaderResource & ShaderResource::operator=(const ShaderResource& other) noexcept
    {
        impl = std::make_unique<ShaderResourceImpl>(*other.impl);
        return *this;
    }

    ShaderResource & ShaderResource::operator=(ShaderResource && other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }

    size_t ShaderResource::BindingIndex() const noexcept
    {
        return impl->bindingIdx;
    }

    size_t ShaderResource::InputAttachmentIndex() const noexcept
    {
        if (impl->type != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
            return std::numeric_limits<decltype(impl->inputAttachmentIdx)>::max();
        }
        return impl->inputAttachmentIdx;
    }

    VkFormat ShaderResource::Format() const noexcept
    {
        return impl->format;
    }

    const char* ShaderResource::Name() const
    {
        return impl->name.c_str();
    }

    const char* ShaderResource::ParentGroupName() const
    {
        return impl->parentSetName.c_str();
    }

    VkShaderStageFlags ShaderResource::ShaderStages() const noexcept
    {
        return impl->stages;
    }

    VkDescriptorType ShaderResource::DescriptorType() const noexcept
    {
        return impl->type;
    }

    ShaderResource::operator VkDescriptorSetLayoutBinding() const noexcept
    {
        return VkDescriptorSetLayoutBinding
        {
            impl->bindingIdx,
            impl->type,
            impl->isDescriptorArray ? impl->arraySize : 1u,
            impl->stages,
            nullptr
        };
    }

    VkDescriptorSetLayoutBinding ShaderResource::AsLayoutBinding() const noexcept
    {
        return this->operator VkDescriptorSetLayoutBinding();
    }

    const char* ShaderResource::ImageSamplerSubtype() const
    {
        return impl->imageSamplerSubtype.c_str();
    }

    bool ShaderResource::HasQualifiers() const noexcept
    {
        return !impl->qualifiers.empty();
    }

    void ShaderResource::GetQualifiers(size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept
    {
        *num_qualifiers = impl->qualifiers.size();
        if (qualifiers != nullptr)
        {
            std::vector<glsl_qualifier> vector_copy;
            for (const auto& qual : impl->qualifiers)
            {
                vector_copy.emplace_back(qual);
            }
            std::copy(vector_copy.begin(), vector_copy.end(), qualifiers);
        }
    }

    void ShaderResource::GetPerUsageQualifiers(const char* shader_name, size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept
    {
        if (auto iter = impl->perUsageQualifiers.find(shader_name); iter != std::end(impl->perUsageQualifiers))
        {
            *num_qualifiers = iter->second.size();
            if (qualifiers != nullptr)
            {
                std::vector<glsl_qualifier> vector_copy;
                for (const auto& qual : iter->second)
                {
                    vector_copy.emplace_back(qual);
                }
                std::copy(vector_copy.begin(), vector_copy.end(), qualifiers);
            }
        }
    }

    glsl_qualifier ShaderResource::GetReadWriteQualifierForShader(const char* shader_name) const noexcept
    {
        // First just check general qualifiers for R/W options
        for (const auto& qual : impl->qualifiers)
        {
            if (qual == glsl_qualifier::ReadOnly || qual == glsl_qualifier::WriteOnly)
            {
                return qual;
            }
        }

        auto& stage_qualifiers = impl->perUsageQualifiers[std::string(shader_name)];
        for (const auto& qual : stage_qualifiers)
        {
            if (qual == glsl_qualifier::ReadOnly || qual == glsl_qualifier::WriteOnly)
            {
                return qual;
            }
        }

        return glsl_qualifier::InvalidQualifier;
    }

    dll_retrieved_strings_t ShaderResource::GetTags() const noexcept
    {
        dll_retrieved_strings_t result;
        result.SetNumStrings(impl->tags.size());
        for (size_t i = 0; i < impl->tags.size(); ++i)
        {
            result.Strings[i] = strdup(impl->tags[i].c_str());
        }
        return result;
    }

    const char* ShaderResource::GetMembersStr() const noexcept
    {
        return impl->membersStr.c_str();
    }

    bool ShaderResource::IsDescriptorArray() const noexcept
    {
        return impl->isDescriptorArray;
    }

    uint32_t ShaderResource::ArraySize() const noexcept
    {
        return impl->arraySize;
    }

    uint32_t ShaderResource::BindingIdx() const noexcept
    {
        return impl->bindingIdx;
    }

    void ShaderResource::SetBindingIndex(uint32_t idx)
    {
        impl->bindingIdx = std::move(idx);
    }

    void ShaderResource::SetInputAttachmentIndex(size_t idx)
    {
        impl->inputAttachmentIdx = idx;
    }

    void ShaderResource::SetStages(VkShaderStageFlags stages)
    {
        impl->stages = std::move(stages);
    }

    void ShaderResource::SetType(VkDescriptorType _type)
    {
        impl->type = std::move(_type);
    }
    void ShaderResource::SetName(const char* name)
    {
        impl->name = std::string{ name };
    }

    void ShaderResource::SetMembersStr(const char* members_str)
    {
        impl->membersStr = members_str;
    }

    void ShaderResource::SetParentGroupName(const char* parent_group_name)
    {
        impl->parentSetName = parent_group_name;
    }

    void ShaderResource::SetQualifiers(const size_t num_qualifiers, glsl_qualifier* qualifiers)
    {
        for (size_t i = 0; i < num_qualifiers; ++i)
        {
            impl->qualifiers.emplace(qualifiers[i]);
        }
    }

    void ShaderResource::AddPerUsageQualifier(const char* shader_name, glsl_qualifier qualifier)
    {
        impl->perUsageQualifiers[std::string(shader_name)].emplace(qualifier);
    }

    void ShaderResource::AddPerUsageQualifiers(const char* shader_name, const size_t num_qualifiers, const glsl_qualifier* qualifiers)
    {
        auto& qualifier_set = impl->perUsageQualifiers[std::string(shader_name)];
        for (size_t i = 0; i < num_qualifiers; ++i)
        {
            qualifier_set.emplace(qualifiers[i]);
        }
    }

    void ShaderResource::SetFormat(VkFormat fmt)
    {
        impl->format = std::move(fmt);
    }

    void ShaderResource::SetTags(const size_t num_tags, const char** tags)
    {
        impl->tags.clear();
        impl->tags.resize(num_tags);
        for (size_t i = 0; i < num_tags; ++i)
        {
            impl->tags.emplace_back(tags[i]);
        }
    }

    void ShaderResource::SetImageSamplerSubtype(const char* subtype)
    {
        impl->imageSamplerSubtype = std::string(subtype);
    }

    void ShaderResource::SetDescriptorArray(bool val)
    {
        impl->isDescriptorArray = val;
    }

    void ShaderResource::SetArraySize(uint32_t val)
    {
        impl->arraySize = val;
    }

}
