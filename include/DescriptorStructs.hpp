#pragma once
#ifndef SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#define SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#include "CommonInclude.hpp"
#include <string>
#include <vector>
#include <map>

namespace st {

    struct ShaderResourceSubObject {
        std::string Name;
        uint32_t Size;
        uint32_t Offset;
    };

    struct ShaderResource {
        std::string Name{ "" };
        uint32_t Binding{ std::numeric_limits<uint32_t>::max() };
        uint32_t ParentSet{ std::numeric_limits<uint32_t>::max() };
        VkShaderStageFlags Stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType Type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> Members;
        VkFormat Format{ VK_FORMAT_UNDEFINED };
        enum class storage_class {
            Read,
            Write,
            ReadWrite
        } StorageClass{ storage_class::Read };
        enum class size_class {
            Absolute,
            SwapchainRelative,
            ViewportRelative
        } SizeClass{ size_class::Absolute };
        
        bool operator==(const ShaderResource& other);
        bool operator<(const ShaderResource& other);
        explicit operator VkDescriptorSetLayoutBinding() const;
        std::string GetType() const;
        void SetType(std::string type_str);
    };

    struct SpecializationConstant {
        enum class constant_type : uint32_t {
            u32,
            i32,
            u64,
            i64,
            f32,
            f64,
            vector,
            matrix,
            invalid
        } Type{ constant_type::u32 };
        constant_type MemberType{ constant_type::f32 };
        uint32_t Rows{ 1 };
        uint32_t Columns{ 1 };
        bool UsedForArrayLength{ false };
    };

    struct DescriptorSetInfo {
        uint32_t Index = std::numeric_limits<uint32_t>::max();
        std::map<uint32_t, ShaderResource> Members = std::map<uint32_t, DescriptorObject>{};
    };

    struct PushConstantInfo {
        VkShaderStageFlags Stages;
        std::string Name;
        std::vector<ShaderResourceSubObject> Members;
        uint32_t Offset;
        explicit operator VkPushConstantRange() const noexcept;
    };

}

#endif //!SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP