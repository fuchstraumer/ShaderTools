#include "core/ResourceUsage.hpp"
#include "core/ShaderResource.hpp"
namespace st {

    VkShaderStageFlags GetActualStages(VkShaderStageFlags in, const ShaderResource* parent) {
        if (in == VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM) {
            return parent->GetStages();
        }
        else {
            return in;
        }
    }

    VkDescriptorType IsAlternateType(VkDescriptorType in, const ShaderResource* parent) {
        if (in == VK_DESCRIPTOR_TYPE_MAX_ENUM) {
            return parent->GetType();
        }
        else {
            return in;
        }
    }

    ResourceUsage::ResourceUsage(const ShaderResource * backing_resource, uint32_t binding, VkShaderStageFlags stages, VkDescriptorType type) : BackingResource(backing_resource),
        BindingIdx(std::move(binding)), Stages(GetActualStages(stages, backing_resource)), Type(IsAlternateType(type, backing_resource)) {}

    ResourceUsage::operator VkDescriptorSetLayoutBinding() const {
        return VkDescriptorSetLayoutBinding{ BindingIdx, Type, 1, Stages, nullptr };
    }

    bool ResourceUsage::operator<(const ResourceUsage & other) const noexcept {
        return BindingIdx < other.BindingIdx;
    }

    bool ResourceUsage::operator==(const ResourceUsage & other) const noexcept {
        return(BindingIdx == other.BindingIdx) && (BackingResource == other.BackingResource);
    }

}