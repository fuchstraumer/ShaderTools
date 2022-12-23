#pragma once
#ifndef SHADER_TOOLS_UTILITY_STRUCTS_HPP
#define SHADER_TOOLS_UTILITY_STRUCTS_HPP
#include "CommonInclude.hpp"

namespace st {

    struct ST_API dll_retrieved_strings_t
    {
        dll_retrieved_strings_t(const dll_retrieved_strings_t&) = delete;
        dll_retrieved_strings_t& operator=(const dll_retrieved_strings_t&) = delete;
        // Names are retrieved using strdup(), so we need to free the duplicated names once done with them.
        // Use this structure to "buffer" the names, and copy them over.
        // Once this structure exits scope the memory should be cleaned up.
        dll_retrieved_strings_t();
        ~dll_retrieved_strings_t();
        dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept;
        dll_retrieved_strings_t& operator=(dll_retrieved_strings_t&& other) noexcept;
        void SetNumStrings(const size_t& num_names);
        void FreeMemory();
        const char* operator[](const size_t& idx) const;
        char** Strings{ nullptr };
        size_t NumStrings{ 0 };
    };

    struct ST_API descriptor_type_counts_t
    {
        uint32_t Samplers{ 0u };
        uint32_t CombinedImageSamplers{ 0u };
        uint32_t SampledImages{ 0u };
        uint32_t StorageImages{ 0u };
        uint32_t UniformTexelBuffers{ 0u };
        uint32_t StorageTexelBuffers{ 0u };
        uint32_t UniformBuffers{ 0u };
        uint32_t StorageBuffers{ 0u };
        uint32_t UniformBuffersDynamic{ 0u };
        uint32_t StorageBuffersDynamic{ 0u };
        uint32_t InputAttachments{ 0u };
        uint32_t InlineUniformBlockEXT{ 0u };
        uint32_t AccelerationStructureNVX{ 0u };
    };

    struct ST_API SpecializationConstant
    {
        enum class constant_type : uint32_t
        {
            b32,
            ui32,
            i32,
            ui64,
            i64,
            f32,
            f64,
            invalid
        } Type{ constant_type::invalid };
        uint32_t ConstantID = 0u;
        union
        {
            VkBool32 value_b32 = VK_FALSE;
            float value_f32;
            int32_t value_i32;
            uint32_t value_ui32;
            double value_f64;
            int64_t value_i64;
            uint64_t value_ui64;
        };
    };

    struct ST_API ShaderResourceSubObject {
        ShaderResourceSubObject() = default;
        ShaderResourceSubObject(const ShaderResourceSubObject& other) noexcept;
        ShaderResourceSubObject(ShaderResourceSubObject&& other) noexcept;
        ShaderResourceSubObject& operator=(const ShaderResourceSubObject& other) noexcept;
        ShaderResourceSubObject& operator=(ShaderResourceSubObject&& other) noexcept;
        ~ShaderResourceSubObject();
        char* Name{ nullptr };
        char* Type{ nullptr };
        union
        {
            uint32_t Size{ 0u };
            uint32_t NumElements;
        };
        uint32_t Offset{ 0 };
        bool isComplex{ false };
    };

}

#endif //!SHADER_TOOLS_UTILITY_STRUCTS_HPP
