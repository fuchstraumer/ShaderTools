#pragma once
#ifndef SHADER_TOOLS_UTILITY_STRUCTS_HPP
#define SHADER_TOOLS_UTILITY_STRUCTS_HPP
#include "CommonInclude.hpp"

namespace st
{

    /**
     * @brief A structure to allow for easier retrieval and iteration of strings retrieved from a DLL while still keeping a C ABI
     * 
     * This structure allows one to retrieve C-style strings from a DLL, but still gain some nice-to-have features for accessing and working with them
     * relative to actual C code. This class does manually allocate memory for the strings, but uses RAII to make cleanup easier and more reliable. Be
     * careful with how you store and use this object - ideally you should use it as a temp buffer, copy into your programs storage, and then let it go out of scope
     * to free the memory.
     */
    struct ST_API dll_retrieved_strings_t
    {
        dll_retrieved_strings_t(const dll_retrieved_strings_t&) = delete;
        dll_retrieved_strings_t& operator=(const dll_retrieved_strings_t&) = delete;
        dll_retrieved_strings_t();
        ~dll_retrieved_strings_t();
        dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept;
        dll_retrieved_strings_t& operator=(dll_retrieved_strings_t&& other) noexcept;
        /**
         * @brief Used by internal functions to pre-allocate the array for the strings.
         * @note Not for use by end-users or clients of this library
         */
        void SetNumStrings(const size_t& num_names);
        /**
         * @brief Frees the memory used by the strings and the array the strings are stored in.
         * @note This is called automatically in the destructor, so you should not need to call it manually. Don't reuse this object, just let it go out of scope
         *       to free the memory.
         */
        void FreeMemory();
        /**
         * @brief Accessor for strings - intended to make it easier to copy the strings out of this structure into your own storage
         */
        const char* operator[](const size_t& idx) const;
        char** Strings{ nullptr };
        size_t NumStrings{ 0 };
    };

    /**
     * @brief Provides information about quantity of descriptor types used at various grains, based on where you retrieve it from.
     * @note This object is much less useful in bindless mode, when most of these counts are just going to be 1 since everything will be a single descriptor array
     */
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
        uint32_t InlineUniformBlocks{ 0u };
        uint32_t AccelerationStructureKHR{ 0u };
        uint32_t AccelerationStructureNV{ 0u };
    };
    
    /**
     * @brief Represents a specialization constant recovered from reflection data, meaning one actually used in the shader at some point.
     * 
     * This structure is used to store the information about a specialization constant, including its type, ID, value, and name. The type
     * lets frontends know how large the value is and how to interpret it, while the ID is used to identify the binding location that will
     * be needed to set the value in the shader during pipeline creation. The name is the name used in the shader code, but can also be used however
     * the frontend wants to use it.
     */
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
        char* Name{ nullptr };

        SpecializationConstant() = default;
        ~SpecializationConstant();
        SpecializationConstant(const SpecializationConstant& other) noexcept;
        SpecializationConstant(SpecializationConstant&& other) noexcept;
        SpecializationConstant& operator=(const SpecializationConstant& other) noexcept;
        SpecializationConstant& operator=(SpecializationConstant&& other) noexcept;

        void SetName(const char* name);
    };

    /**
     * @brief Represents a subobject of a shader resource, i.e. a member of a struct or an array element.
     * 
     * All of the information provided is intended to make it possible to reconstruct the struct/shader resource in full
     * in the frontend, and then be able to copy and write to this value appropriately if name associativity can be established.
     */
    struct ST_API ShaderResourceSubObject
    {
        ShaderResourceSubObject() = default;
        ShaderResourceSubObject(const ShaderResourceSubObject& other) noexcept;
        ShaderResourceSubObject(ShaderResourceSubObject&& other) noexcept;
        ShaderResourceSubObject& operator=(const ShaderResourceSubObject& other) noexcept;
        ShaderResourceSubObject& operator=(ShaderResourceSubObject&& other) noexcept;
        ~ShaderResourceSubObject();
        void SetName(const char* name);
        void SetType(const char* type);
        /** Name of the resource as it is used in the shader */
        char* Name{ nullptr };
        char* Type{ nullptr };
        uint32_t Size{ 0u };
        uint32_t NumElements{ 0u };
        uint32_t Offset{ 0u };
        /** @brief Indicates whether this subobject itself is complex (e.g. a struct or array), which means it can also contain subobjects */
        bool isComplex{ false };
    };

}

#endif //!SHADER_TOOLS_UTILITY_STRUCTS_HPP
