#include "core/ResourceUsage.hpp"
namespace st {

    ResourceUsage::ResourceUsage(const Shader& used_by, const ShaderResource * backing_resource, uint32_t binding, access_modifier _access_modifier, VkDescriptorType type) : backingResource(backing_resource),
        bindingIdx(std::move(binding)), type(std::move(type)), usedBy(used_by), accessModifier(std::move(_access_modifier)), stages(used_by.GetStage()) {}

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

}