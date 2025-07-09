#pragma once
#ifndef SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#define SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"

namespace st
{

    struct PushConstantInfoImpl;
    struct VertexAttributeInfoImpl;

    /**
     * @brief Represents a push constant range in a single pipeline (compute or graphics).
     * Contains meta-information, along with everything you need to construct a `VkPushConstantRange` object (which can be done with the conversion operator).
     * Also allows you to query the members of the push constant range, and with `ShaderResourceSubObject` you can potentially dynamically fill and construct
     * buffers for the push constant range at runtime (instead of baking in structs). Does require retrieving data by name, though.
     * @ingroup Reflection
     */
    struct ST_API PushConstantInfo
    {
        PushConstantInfo() noexcept;
        ~PushConstantInfo() noexcept;
        PushConstantInfo(const PushConstantInfo& other) noexcept;
        PushConstantInfo& operator=(const PushConstantInfo& other) noexcept;
        void SetStages(VkShaderStageFlags flags) noexcept;
        void SetName(const char* name) noexcept;
        void SetOffset(uint32_t amt) noexcept;
        void SetMembers(const size_t num_members, ShaderResourceSubObject* members);
        const VkShaderStageFlags& Stages() const noexcept;
        const char* Name() const noexcept;
        const uint32_t& Offset() const noexcept;
        void GetMembers(size_t* num_members, ShaderResourceSubObject* members) const;
        explicit operator VkPushConstantRange() const noexcept;
    private:
        std::unique_ptr<PushConstantInfoImpl> impl;
    };

    /**
     * @brief Represents a single vertex attribute, which can be directly converted to a `VkVertexInputAttributeDescription` object.
     * Contains meta-information about the attribute, such as its name, location, offset, and format. Users may also retrieve the type of the attribute as a string, if desired.
     * @ingroup Reflection
     */
    struct ST_API VertexAttributeInfo
    {
        VertexAttributeInfo() noexcept;
        ~VertexAttributeInfo() noexcept;
        VertexAttributeInfo(const VertexAttributeInfo& other) noexcept;
        VertexAttributeInfo& operator=(const VertexAttributeInfo& other) noexcept;
        void SetName(const char* name);
        void SetFormatFromSpvReflectFlags(uint32_t spv_reflect_format_flags);
        void SetLocation(uint32_t loc) noexcept;
        void SetOffset(uint32_t _offset) noexcept;
        const char* Name() const noexcept;
        const char* TypeAsText() const noexcept;
        uint32_t Location() const noexcept;
        uint32_t Offset() const noexcept;
        // Returns VK_FORMAT_UNDEFINED if matching of type to a Vulkan format doesn't work.
        VkFormat GetAsFormat() const noexcept;
        explicit operator VkVertexInputAttributeDescription() const noexcept;
    private:
        std::unique_ptr<VertexAttributeInfoImpl> impl;
    };

}

#endif //!SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
