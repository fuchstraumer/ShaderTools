#pragma once
#ifndef SHADER_TOOLS_RESOURCE_USAGE_HPP
#define SHADER_TOOLS_RESOURCE_USAGE_HPP
#include "common/CommonInclude.hpp"

namespace st {

    class ShaderResource;

    struct ST_API ResourceUsage {
    public:
        ResourceUsage(const ShaderResource* backing_resource, uint32_t binding, VkShaderStageFlags flags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,
            VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM);

        explicit operator VkDescriptorSetLayoutBinding() const;
        bool operator<(const ResourceUsage& other) const noexcept;
        bool operator==(const ResourceUsage& other) const noexcept;

        const uint32_t BindingIdx;
        const ShaderResource* BackingResource;
        const VkDescriptorType Type;
        const VkShaderStageFlags Stages;
    };

}


#endif // !SHADER_TOOLS_RESOURCE_USAGE_HPP
