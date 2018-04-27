#include "core/ResourceUsage.hpp"
namespace st {

    ResourceUsage::ResourceUsage(const Shader& used_by, const ShaderResource * backing_resource, uint32_t binding, access_modifier _access_modifier, VkDescriptorType type) : backingResource(backing_resource),
        bindingIdx(std::move(binding)), type(std::move(type)), usedBy(used_by), accessModifier(std::move(_access_modifier)), stages(used_by.GetStage()) {}

    ResourceUsage::ResourceUsage(const ResourceUsage& other) noexcept : backingResource(other.backingResource), bindingIdx(other.bindingIdx), type(other.type), usedBy(other.usedBy), accessModifier(other.accessModifier),
        stages(other.stages) {}

    ResourceUsage::ResourceUsage(ResourceUsage&& other) noexcept : backingResource(std::move(other.backingResource)), bindingIdx(std::move(other.bindingIdx)), type(std::move(other.type)),
        usedBy(std::move(other.usedBy)), accessModifier(std::move(other.accessModifier)), stages(std::move(other.stages)) {}

    ResourceUsage & ResourceUsage::operator=(const ResourceUsage & other) noexcept {
        backingResource = other.backingResource;
        bindingIdx = other.bindingIdx;
        type = other.type;
        usedBy = other.usedBy;
        accessModifier = other.accessModifier;
        stages = other.stages;
        return *this;
    }

    ResourceUsage & ResourceUsage::operator=(ResourceUsage && other) noexcept {
        backingResource = other.backingResource;
        bindingIdx = std::move(other.bindingIdx);
        type = std::move(other.type);
        usedBy = std::move(other.usedBy);
        accessModifier = std::move(other.accessModifier);
        stages = std::move(other.stages);
        return *this;
    }

    ResourceUsage::operator VkDescriptorSetLayoutBinding() const {
        return VkDescriptorSetLayoutBinding{ bindingIdx, type, 1, stages, nullptr };
    }

    bool ResourceUsage::operator<(const ResourceUsage & other) const noexcept {
        return bindingIdx < other.bindingIdx;
    }

    bool ResourceUsage::operator==(const ResourceUsage & other) const noexcept {
        return  (bindingIdx == other.bindingIdx) && 
                (backingResource == other.backingResource) && 
                (usedBy == other.usedBy) &&
                (type == other.type);
    }

    VkShaderStageFlags & ResourceUsage::Stages() noexcept {
        return stages;
    }

    const Shader & ResourceUsage::UsedBy() const noexcept {
        return usedBy;
    }

    const ShaderResource * ResourceUsage::BackingResource() const noexcept {
        return backingResource;
    }

    const VkDescriptorType & ResourceUsage::Type() const noexcept {
        return type;
    }

    const uint32_t & ResourceUsage::BindingIdx() const noexcept {
        return bindingIdx;
    }

}