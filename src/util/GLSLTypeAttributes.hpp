#pragma once
#ifndef SHADERTOOLS_GLSL_TYPE_ATTRIBUTES_HPP
#define SHADERTOOLS_GLSL_TYPE_ATTRIBUTES_HPP
#include <optional>
#include <string_view>

namespace st
{

    /**
    * @ingroup Utility
    * @brief Attributes for GLSL builtin types, used to understand more about them during structure schema parsing and source code generation.
    */
    struct GLSLTypeAttributes
    {
        size_t Size{ 0u };
        size_t Alignment{ 0u };
        /** If type is an array type, alignment and size just become the base type's alignment and size */
        bool IsArrayType{ false };
        /** Name of the type of individual elements when this is an array type */
        std::string_view ElementTypename;
        /** Size of a singular element when this is an array type */
        size_t ElementSize{ 0u };
        size_t ArraySize{ 0u };
    };

    /**
     * @brief Memory layout used for GLSL structs. Currently only support Std430 because it is widely supported and still performant, but a little more flexible than Std140.
     */
    enum class MemoryLayout
    {
        Std140, // Standard 140 layout, used in OpenGL
        Std430,  // Standard 430 layout, used in Vulkan and OpenGL compute shaders
        Scalar // Scalar layout, same as CPU alignment with no padding. Lower performance.
    };

    /**
    * @brief Get the attributes for a given GLSL type.
    * @param type The GLSL type to get attributes for.
    * @param memory_layout The memory layout to use for the type attributes.
    * @return The attributes for the given GLSL type.
    */
    std::optional<GLSLTypeAttributes> GetGLSLTypeAttributes(std::string_view type, MemoryLayout memory_layout = MemoryLayout::Std430) noexcept;

} // namespace st

#endif // !SHADERTOOLS_GLSL_TYPE_ATTRIBUTES_HPP