#include "core/ResourceUsage.hpp"
#include "core/ShaderResource.hpp"
namespace st {

    constexpr const char* const INVALID_SHADER_NAME = "INVALID_SHADER";
    const static Shader INVALID_SHADER(INVALID_SHADER_NAME, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

    ResourceUsage::ResourceUsage() noexcept : usedBy(INVALID_SHADER), accessModifier(access_modifier::INVALID), stages(VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM), backingResource(nullptr),
        bindingIdx(std::numeric_limits<uint32_t>::max()), type(VK_DESCRIPTOR_TYPE_MAX_ENUM) {}

    ResourceUsage::~ResourceUsage() {}

    ResourceUsage::ResourceUsage(const Shader& used_by, const ShaderResource * backing_resource, access_modifier _access_modifier, VkDescriptorType type) : backingResource(backing_resource),
        bindingIdx(static_cast<uint32_t>(backing_resource->BindingIndex())), type(std::move(type)), usedBy(used_by), accessModifier(std::move(_access_modifier)), stages(used_by.GetStage()) {}

    ResourceUsage::ResourceUsage(const ResourceUsage& other) noexcept : backingResource(other.backingResource), bindingIdx(other.bindingIdx), type(other.type), usedBy(other.usedBy), accessModifier(other.accessModifier),
        stages(other.stages) {}

    ResourceUsage::ResourceUsage(ResourceUsage&& other) noexcept : backingResource(std::move(other.backingResource)), bindingIdx(std::move(other.bindingIdx)), type(std::move(other.type)),
        usedBy(std::move(other.usedBy)), accessModifier(std::move(other.accessModifier)), stages(std::move(other.stages)) {}

    ResourceUsage& ResourceUsage::operator=(const ResourceUsage& other) noexcept {
        backingResource = other.backingResource;
        bindingIdx = other.bindingIdx;
        type = other.type;
        usedBy = other.usedBy;
        accessModifier = other.accessModifier;
        stages = other.stages;
        return *this;
    }

    ResourceUsage& ResourceUsage::operator=(ResourceUsage&& other) noexcept {
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

    bool ResourceUsage::operator<(const ResourceUsage& other) const noexcept {
        return bindingIdx < other.bindingIdx;
    }

    bool ResourceUsage::operator==(const ResourceUsage& other) const noexcept {
        return  (bindingIdx == other.bindingIdx) && 
                (backingResource == other.backingResource) && 
                (usedBy == other.usedBy) &&
                (type == other.type);
    }

    VkShaderStageFlags& ResourceUsage::Stages() noexcept {
        return stages;
    }

    const Shader& ResourceUsage::UsedBy() const noexcept {
        return usedBy;
    }

    const ShaderResource* ResourceUsage::BackingResource() const noexcept {
        return backingResource;
    }

    const VkDescriptorType& ResourceUsage::Type() const noexcept {
        return type;
    }

    const uint32_t& ResourceUsage::BindingIdx() const noexcept {
        return bindingIdx;
    }

    const access_modifier & ResourceUsage::AccessModifier() const noexcept {
        return accessModifier;
    }

    bool ResourceUsage::ReadOnly() const noexcept {
        return accessModifier == access_modifier::Read;
    }

    bool ResourceUsage::WriteOnly() const noexcept {
        return accessModifier == access_modifier::Write;
    }

    bool ResourceUsage::ReadWrite() const noexcept {
        return accessModifier == access_modifier::ReadWrite;
    }

}
