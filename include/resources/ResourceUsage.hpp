#pragma once
#ifndef SHADER_TOOLS_RESOURCE_USAGE_HPP
#define SHADER_TOOLS_RESOURCE_USAGE_HPP
#include "common/ShaderStage.hpp"
#include "common/CommonInclude.hpp"

namespace st
{

    class ShaderResource;

    enum class access_modifier : uint8_t
    {
        Read = 0,
        Write,
        ReadWrite,
        INVALID = std::numeric_limits<uint32_t>::max()
    };

    class ST_API ResourceUsage
    {
    public:

        ResourceUsage() noexcept;
        ~ResourceUsage() noexcept = default;
        ResourceUsage(const ShaderStage& used_by, const ShaderResource* backing_resource, access_modifier _access_modifier, VkDescriptorType type);
        ResourceUsage(const ResourceUsage& other) noexcept;
        ResourceUsage(ResourceUsage&& other) noexcept;

        ResourceUsage& operator=(const ResourceUsage& other) noexcept;
        ResourceUsage& operator=(ResourceUsage&& other) noexcept;

        explicit operator VkDescriptorSetLayoutBinding() const;
        bool operator<(const ResourceUsage& other) const noexcept;
        bool operator==(const ResourceUsage& other) const noexcept;

        VkShaderStageFlags& Stages() noexcept;
        const ShaderStage& UsedBy() const noexcept;
        const ShaderResource* BackingResource() const noexcept;
        const VkDescriptorType& Type() const noexcept;
        uint32_t BindingIdx() const noexcept;
        uint32_t SetIdx() const noexcept;
        const access_modifier& AccessModifier() const noexcept;
        bool ReadOnly() const noexcept;
        bool WriteOnly() const noexcept;
        bool ReadWrite() const noexcept;

    private:
        friend class ShaderReflectorImpl;
        uint32_t setIdx;
        uint32_t bindingIdx;
        access_modifier accessModifier;
        const ShaderResource* backingResource;
        ShaderStage usedBy;
        VkDescriptorType type;
        VkShaderStageFlags stages;
    };

}


#endif // !SHADER_TOOLS_RESOURCE_USAGE_HPP
