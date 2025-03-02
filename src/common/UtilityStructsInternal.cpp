#include "UtilityStructsInternal.hpp"

namespace st
{

    ShaderToolsErrorCode st::CountDescriptorType(const VkDescriptorType& type, descriptor_type_counts_t& typeCounts)
    {
        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            typeCounts.Samplers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            typeCounts.CombinedImageSamplers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            typeCounts.SampledImages++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            typeCounts.StorageImages++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            typeCounts.UniformTexelBuffers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            typeCounts.StorageTexelBuffers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            typeCounts.UniformBuffers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            typeCounts.StorageBuffers++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            typeCounts.UniformBuffersDynamic++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            typeCounts.StorageBuffersDynamic++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            typeCounts.InputAttachments++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            typeCounts.InlineUniformBlocks++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            typeCounts.AccelerationStructureKHR++;
            return ShaderToolsErrorCode::Success;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            typeCounts.AccelerationStructureNV++;
            return ShaderToolsErrorCode::Success;
        default:
            return ShaderToolsErrorCode::ResourceInvalidDescriptorType;
        }
    }

}
