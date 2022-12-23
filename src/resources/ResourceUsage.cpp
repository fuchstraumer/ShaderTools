#include "resources/ResourceUsage.hpp"
#include "resources/ShaderResource.hpp"

namespace st
{

    constexpr const char* const INVALID_SHADER_NAME = "INVALID_SHADER";
    const static ShaderStage INVALID_SHADER(INVALID_SHADER_NAME, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

    ResourceUsage::ResourceUsage() noexcept :
        setIdx(std::numeric_limits<uint32_t>::max()),
        bindingIdx(std::numeric_limits<uint32_t>::max()),
        accessModifier(access_modifier::INVALID),
        backingResource(nullptr),
        usedBy(INVALID_SHADER),
        type(VK_DESCRIPTOR_TYPE_MAX_ENUM),
        stages(VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM)
    {}

    ResourceUsage::ResourceUsage(const ShaderStage& used_by, const ShaderResource* backing_resource, access_modifier _access_modifier, VkDescriptorType type) :
        setIdx(std::numeric_limits<uint32_t>::max()),
        bindingIdx(static_cast<uint32_t>(backing_resource->BindingIndex())),
        accessModifier(std::move(_access_modifier)),
        backingResource(backing_resource),
        usedBy(used_by),
        type(std::move(type)),
        stages(used_by.GetStage())
    {}

    ResourceUsage::ResourceUsage(const ResourceUsage& other) noexcept :
        setIdx(other.setIdx),
        bindingIdx(other.bindingIdx),
        accessModifier(other.accessModifier),
        backingResource(other.backingResource),
        usedBy(other.usedBy),
        type(other.type),
        stages(other.stages) {}

    ResourceUsage::ResourceUsage(ResourceUsage&& other) noexcept :
        setIdx(std::move(other.setIdx)),
        bindingIdx(std::move(other.bindingIdx)),
        accessModifier(std::move(other.accessModifier)),
        backingResource(std::move(other.backingResource)),
        usedBy(std::move(other.usedBy)),
        type(std::move(other.type)),
        stages(std::move(other.stages))
    {}

    ResourceUsage& ResourceUsage::operator=(const ResourceUsage& other) noexcept
    {
        setIdx = other.setIdx;
        bindingIdx = other.bindingIdx;
        accessModifier = other.accessModifier;
        backingResource = other.backingResource;
        usedBy = other.usedBy;
        type = other.type;
        stages = other.stages;
        return *this;
    }

    ResourceUsage& ResourceUsage::operator=(ResourceUsage&& other) noexcept
    {
        setIdx = std::move(other.setIdx);
        bindingIdx = std::move(other.bindingIdx);
        accessModifier = std::move(other.accessModifier);
        backingResource = other.backingResource;
        usedBy = std::move(other.usedBy);
        type = std::move(other.type);
        stages = std::move(other.stages);
        return *this;
    }

    ResourceUsage::operator VkDescriptorSetLayoutBinding() const
    {
        return VkDescriptorSetLayoutBinding{ bindingIdx, type, 1, stages, nullptr };
    }

    bool ResourceUsage::operator<(const ResourceUsage& other) const noexcept
    {
        // sort by set and binding, so all end up in ascending proper order
        if (setIdx == other.setIdx)
        {
            return bindingIdx < other.bindingIdx;
        }
        else
        {
            return setIdx < other.setIdx;
        }
    }

    // briefly wondered if set index was actually relevant, but yes it is - for a higher level backing resource it wouldn't
    // matter, but we're tracking and comparing
    bool ResourceUsage::operator==(const ResourceUsage& other) const noexcept
    {
        return
            (setIdx == other.setIdx) &&
            (bindingIdx == other.bindingIdx) &&
            (accessModifier == other.accessModifier) &&
            (backingResource == other.backingResource) &&
            (usedBy == other.usedBy) &&
            (type == other.type) &&
            (stages == other.stages);
    }

    VkShaderStageFlags& ResourceUsage::Stages() noexcept
    {
        return stages;
    }

    const ShaderStage& ResourceUsage::UsedBy() const noexcept
    {
        return usedBy;
    }

    const ShaderResource* ResourceUsage::BackingResource() const noexcept
    {
        return backingResource;
    }

    const VkDescriptorType& ResourceUsage::Type() const noexcept
    {
        return type;
    }

    uint32_t ResourceUsage::BindingIdx() const noexcept
    {
        return bindingIdx;
    }

    uint32_t ResourceUsage::SetIdx() const noexcept
    {
        return setIdx;
    }

    const access_modifier & ResourceUsage::AccessModifier() const noexcept
    {
        return accessModifier;
    }

    bool ResourceUsage::ReadOnly() const noexcept
    {
        return accessModifier == access_modifier::Read;
    }

    bool ResourceUsage::WriteOnly() const noexcept
    {
        return accessModifier == access_modifier::Write;
    }

    bool ResourceUsage::ReadWrite() const noexcept
    {
        return accessModifier == access_modifier::ReadWrite;
    }

}
