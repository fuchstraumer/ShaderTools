#include "GLSLTypeAttributes.hpp"
#include "common/ShaderToolsErrors.hpp"
#include "../common/impl/SessionImpl.hpp"
#include <unordered_map>
#include <array>
#include <string>
#include <charconv>
#include <stdexcept>
#include <format>
#include <expected>

constexpr static std::array<const char*, 119> glsl_builtin_type_names
{
    "float",
    "double",
    "int",
    "uint",
    "bool",
    "vec2",
    "vec3",
    "vec4",
    "dvec2",
    "dvec3",
    "dvec4",
    "ivec2",
    "ivec3",
    "ivec4",
    "uvec2",
    "uvec3",
    "uvec4",
    "bvec2",
    "bvec3",
    "bvec4",
    "mat2",
    "mat2x3",
    "mat2x4",
    "mat3",
    "mat3x2",
    "mat3x3",
    "mat3x4",
    "mat4",
    "mat4x2",
    "mat4x3",
    "dmat2",
    "dmat2x3",
    "dmat2x4",
    "dmat3",
    "dmat3x4",
    "dmat4",
    "dmat4x2",
    "dmat4x3",
    "dmat4x4",
    // start extension types, GL_EXT_shader_explicit_arithmetic_types
    // GL_EXT_shader_explicit_arithmetic_types_float16 
    "float16_t",
    "f16vec2",
    "f16vec3",
    "f16vec4",
    "f16mat2",
    "f16mat3",
    "f16mat4",
    "f16mat2x2",
    "f16mat2x3",
    "f16mat2x4",
    "f16mat3x2",
    "f16mat3x3",
    "f16mat3x4",
    "f16mat4x2",
    "f16mat4x3",
    "f16mat4x4",
    // GL_EXT_shader_explicit_arithmetic_types_float32
    "float32_t",
    "f32vec2",
    "f32vec3",
    "f32vec4",
    "f32mat2",
    "f32mat3",
    "f32mat4",
    "f32mat2x2",
    "f32mat2x3",
    "f32mat2x4",
    "f32mat3x2",
    "f32mat3x3",
    "f32mat3x4",
    "f32mat4x2",
    "f32mat4x3",
    "f32mat4x4",
    // GL_EXT_shader_explicit_arithmetic_types_float64
    "float64_t",
    "f64vec2",
    "f64vec3",
    "f64vec4",
    "f64mat2",
    "f64mat3",
    "f64mat4",
    "f64mat2x2",
    "f64mat2x3",
    "f64mat2x4",
    "f64mat3x2",
    "f64mat3x3",
    "f64mat3x4",
    "f64mat4x2",
    "f64mat4x3",
    "f64mat4x4",
    // GL_EXT_shader_explicit_arithmetic_types_int64
    "int64_t",
    "i64vec2",
    "i64vec3",
    "i64vec4",
    "uint64_t",
    "u64vec2",
    "u64vec3",
    "u64vec4",
    // GL_EXT_shader_explicit_arithmetic_types_int32
    "int32_t",
    "i32vec2",
    "i32vec3",
    "i32vec4",
    "uint32_t",
    "u32vec2",
    "u32vec3",
    "u32vec4",
    // GL_EXT_shader_explicit_arithmetic_types_int16
    "int16_t",
    "i16vec2",
    "i16vec3",
    "i16vec4",
    "uint16_t",
    "u16vec2",
    "u16vec3",
    "u16vec4",
    // GL_EXT_shader_explicit_arithmetic_types_int8
    "int8_t",
    "i8vec2",
    "i8vec3",
    "i8vec4",
    "uint8_t",
    "u8vec2",
    "u8vec3",
    "u8vec4"
};

namespace st
{

    /**
    * From https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-alignment-requirements, rewritten here to make myself think about
    * it more and have a quick reference to look back at as I implement this alignment calculator.
    * 
    * Scalar alignment of OpTypeStruct member is defined recursively as follows:
    * - A scalar of size N has alignment N.
    * - A vector type has a scalar alignment equal to that of it's component type.
    * - An array type has a scalar alignment equal to that of its element type.
    * - A structure has a scalar alignment equal to the largest scalar alignment of its members.
    * - A matrix type inherits scalar alignment from the equivalent array declaration (e.g, mat2x2 is vec2[2], mat2x3 is vec4[2], etc.)
    * 
    * The base alignment of the type of an OpTypeStructMember is defined recursively as follows:
    * - A scalar has a base alignment equal to its scalar alignment.
    * - A two component vector has a base alignment equal to twice it's scalar alignment.
    * - A three- or four-component vector has a base alignment equal to four times its scalar alignment.
    * - An array has a base alignment equal to the base alignment of its element type.
    * - A structure has a base alignment equal to the largest base alignment of any of its members.
    * - A matrix type inherits base alignment from the equivalent array declaration
    * 
    * The extended alignment of the type of an OpTypeStructMember is defined recursively as follows:
    * - A scalar or vector type has an extended alignment equal to its base alignment.
    * - An array or structure type has an extended alignment equal to the largest extended alignment of any of its members, rounded up to a multiple of 16.
    * 
    * "Scalar" alignment qualifier is just scalar alignment of type, i.e. first ruleset.
    * "Std430" alignment uses the base alignment of the type, which is the second ruleset.
    * "Std140" alignment uses the extended alignment of the type, which is the third ruleset.
    * 
    * Thus, we can calculate the alignment and size of a GLSL type based on these rules
    */

    /**
    * @brief Underlying type of any component of a GLSL type, including explicit width
    */
    enum class UnderlyingComponentType : uint8_t
    {
        i8, // signed 8-bit integer
        ui8, // unsigned 8-bit integer
        i16, // signed 16-bit integer
        ui16, // unsigned 16-bit integer
        f16, // 16-bit floating point
        i32, // signed 32-bit integer
        ui32, // unsigned 32-bit integer
        f32, // 32-bit floating point
        i64, // signed 64-bit integer
        ui64, // unsigned 64-bit integer
        f64, // 64-bit floating point,
        NUM_COMPONENT_TYPES,
        Invalid = NUM_COMPONENT_TYPES // Invalid type, used for error handling
    };

    /**
    * @brief Size of each ComponentType in bytes, which usually just becomes the alignment of the type.
    */
    constexpr static std::array<size_t, static_cast<size_t>(UnderlyingComponentType::NUM_COMPONENT_TYPES)> component_type_sizes
    {
        1, // i8
        1, // ui8
        2, // i16
        2, // ui16
        2, // f16
        4, // i32
        4, // ui32
        4, // f32
        8, // i64
        8, // ui64
        8  // f64
    };

    /**
    * @brief The dimensionality of a GLSL type, with matrices equal to arrays for the purpose of alignment calculations.
    */
    enum class TypeDimensionality : uint8_t
    {
        Scalar,
        Vector2,
        Vector3, // Vector3 will be treated as a 4-component vector for alignment purposes
        Vector4,
        Array,
        Matrix, // Matrix is treated as an array of column vectors, but can't be same enum value since we need to differentiate slightly (column length)
        Struct,
        Invalid
    };

    /**
    * @brief Key attributes we need to do the actual alignment calculation for a given ruleset for a GLSL type extracted from a string.
    */
    struct TypeAlignmentAttributes
    {
        constexpr TypeAlignmentAttributes() noexcept = default;
        constexpr ~TypeAlignmentAttributes() noexcept = default;
 
        constexpr TypeAlignmentAttributes(
            UnderlyingComponentType component_type,
            size_t component_size,
            TypeDimensionality dimensionality,
            size_t array_size,
            size_t matrix_column_length) noexcept
            : ComponentType(std::move(component_type)),
              ComponentSize(std::move(component_size)),
              Dimensionality(std::move(dimensionality)),
              ArraySize(std::move(array_size)),
              MatrixColumnLength(std::move(matrix_column_length)),
              ElementTypename(""),
              ElementTypeAttributes(nullptr)
        {
        }

        UnderlyingComponentType ComponentType{ UnderlyingComponentType::Invalid };
        /** 
        * @brief Value read from component_type_sizes for everything except struct and array types.
        */
        size_t ComponentSize{ 0u };
        TypeDimensionality Dimensionality{ TypeDimensionality::Invalid };
        /** Number of elements in array for Array types, or number of columns for Matrix type */
        size_t ArraySize{ 0u };
        /** Number of elements in the column vectors that make up matrix arrays */
        size_t MatrixColumnLength{ 0u };
        /** For array types, parsed name of contained element. Can be a name of a struct type, in which case we'll have to refer back to the current parsing run to back-reference. */
        std::string_view ElementTypename;
        /** For array types, this is the TypeAlignmentAttributes for the contained type */
        const TypeAlignmentAttributes* ElementTypeAttributes{ nullptr };
    };

    constexpr size_t GetBuiltInTypeSizeFromAttributes(const TypeAlignmentAttributes& attributes) noexcept
    {
        switch (attributes.Dimensionality)
        {
        case TypeDimensionality::Scalar:
            return attributes.ComponentSize;
        case TypeDimensionality::Vector2:
            return attributes.ComponentSize * 2;
        case TypeDimensionality::Vector3:
            [[fallthrough]];
        case TypeDimensionality::Vector4:
            return attributes.ComponentSize * 4;
        case TypeDimensionality::Matrix:
            // Matrices are treated as arrays of column vectors
            return attributes.ComponentSize * attributes.MatrixColumnLength * attributes.ArraySize;
        default:
            // We only call this when evaluating terminal built in nodes, so this can't be hit
            return 0;
        }
    }

    /**
    * @brief Map of GLSL builtin type names to their alignment attributes. 
    * We use this to quickly look up builtin attributes when we can, falling back to manual parsing when we can't.
    * @note Only the first 39 entries are true builtins, the following entries are from extensions and will cause issues if relevant extensions are not enabled as well.
    */
    const static std::unordered_map<std::string_view, TypeAlignmentAttributes> glsl_type_alignment_attributes = std::unordered_map<std::string_view, TypeAlignmentAttributes>
    {
        // Basic scalar types
        { std::string_view(glsl_builtin_type_names[0]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Scalar, 0, 0 } }, // float
        { std::string_view(glsl_builtin_type_names[1]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Scalar, 0, 0 } }, // double
        { std::string_view(glsl_builtin_type_names[2]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Scalar, 0, 0 } }, // int
        { std::string_view(glsl_builtin_type_names[3]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // uint
        { std::string_view(glsl_builtin_type_names[4]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // bool (treated as uint32)
        
        // Basic vector types
        { std::string_view(glsl_builtin_type_names[5]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector2, 0, 0 } }, // vec2
        { std::string_view(glsl_builtin_type_names[6]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector3, 0, 0 } }, // vec3
        { std::string_view(glsl_builtin_type_names[7]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector4, 0, 0 } }, // vec4
        { std::string_view(glsl_builtin_type_names[8]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector2, 0, 0 } }, // dvec2
        { std::string_view(glsl_builtin_type_names[9]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector3, 0, 0 } }, // dvec3
        { std::string_view(glsl_builtin_type_names[10]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector4, 0, 0 } }, // dvec4
        { std::string_view(glsl_builtin_type_names[11]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector2, 0, 0 } }, // ivec2
        { std::string_view(glsl_builtin_type_names[12]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector3, 0, 0 } }, // ivec3
        { std::string_view(glsl_builtin_type_names[13]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector4, 0, 0 } }, // ivec4
        { std::string_view(glsl_builtin_type_names[14]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // uvec2
        { std::string_view(glsl_builtin_type_names[15]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // uvec3
        { std::string_view(glsl_builtin_type_names[16]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // uvec4
        { std::string_view(glsl_builtin_type_names[17]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // bvec2 (treated as uint32)
        { std::string_view(glsl_builtin_type_names[18]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // bvec3 (treated as uint32)
        { std::string_view(glsl_builtin_type_names[19]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // bvec4 (treated as uint32)
        
        // Base float matrix types
        { std::string_view(glsl_builtin_type_names[20]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // mat2: 2 columns of vec2
        { std::string_view(glsl_builtin_type_names[21]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // mat2x3: 2 columns of vec3 (padded to vec4)
        { std::string_view(glsl_builtin_type_names[22]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // mat2x4: 2 columns of vec4
        { std::string_view(glsl_builtin_type_names[23]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3: 3 columns of vec3 (padded to vec4)
        { std::string_view(glsl_builtin_type_names[24]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 2 } }, // mat3x2: 3 columns of vec2
        { std::string_view(glsl_builtin_type_names[25]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3x3: 3 columns of vec3 (padded to vec4)
        { std::string_view(glsl_builtin_type_names[26]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3x4: 3 columns of vec4
        { std::string_view(glsl_builtin_type_names[27]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // mat4: 4 columns of vec4
        { std::string_view(glsl_builtin_type_names[28]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 2 } }, // mat4x2: 4 columns of vec2
        { std::string_view(glsl_builtin_type_names[29]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // mat4x3: 4 columns of vec3 (padded to vec4)
        
        // Double matrix types
        { std::string_view(glsl_builtin_type_names[30]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // dmat2: 2 columns of dvec2
        { std::string_view(glsl_builtin_type_names[31]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // dmat2x3: 2 columns of dvec3 (padded to dvec4)
        { std::string_view(glsl_builtin_type_names[32]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // dmat2x4: 2 columns of dvec4
        { std::string_view(glsl_builtin_type_names[33]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // dmat3: 3 columns of dvec3 (padded to dvec4)
        { std::string_view(glsl_builtin_type_names[34]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // dmat3x4: 3 columns of dvec4
        { std::string_view(glsl_builtin_type_names[35]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4: 4 columns of dvec4
        { std::string_view(glsl_builtin_type_names[36]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 2 } }, // dmat4x2: 4 columns of dvec2
        { std::string_view(glsl_builtin_type_names[37]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4x3: 4 columns of dvec3 (padded to dvec4)
        { std::string_view(glsl_builtin_type_names[38]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4x4: 4 columns of dvec4
        
        // float16_t types
        { std::string_view(glsl_builtin_type_names[39]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Scalar, 0, 0 } }, // float16_t
        { std::string_view(glsl_builtin_type_names[40]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Vector2, 0, 0 } }, // f16vec2
        { std::string_view(glsl_builtin_type_names[41]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Vector3, 0, 0 } }, // f16vec3
        { std::string_view(glsl_builtin_type_names[42]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Vector4, 0, 0 } }, // f16vec4
        { std::string_view(glsl_builtin_type_names[43]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 2, 2 } }, // f16mat2: 2 columns of f16vec2
        { std::string_view(glsl_builtin_type_names[44]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3: 3 columns of f16vec3 (padded to f16vec4)
        { std::string_view(glsl_builtin_type_names[45]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4: 4 columns of f16vec4
        { std::string_view(glsl_builtin_type_names[46]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 2, 2 } }, // f16mat2x2: 2 columns of f16vec2
        { std::string_view(glsl_builtin_type_names[47]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 2, 4 } }, // f16mat2x3: 2 columns of f16vec3 (padded to f16vec4)
        { std::string_view(glsl_builtin_type_names[48]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 2, 4 } }, // f16mat2x4: 2 columns of f16vec4
        { std::string_view(glsl_builtin_type_names[49]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 3, 2 } }, // f16mat3x2: 3 columns of f16vec2
        { std::string_view(glsl_builtin_type_names[50]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3x3: 3 columns of f16vec3 (padded to f16vec4)
        { std::string_view(glsl_builtin_type_names[51]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3x4: 3 columns of f16vec4
        { std::string_view(glsl_builtin_type_names[52]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 4, 2 } }, // f16mat4x2: 4 columns of f16vec2
        { std::string_view(glsl_builtin_type_names[53]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4x3: 4 columns of f16vec3 (padded to f16vec4)
        { std::string_view(glsl_builtin_type_names[54]), TypeAlignmentAttributes{ UnderlyingComponentType::f16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4x4: 4 columns of f16vec4
        
        // float32_t types
        { std::string_view(glsl_builtin_type_names[55]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Scalar, 0, 0 } }, // float32_t
        { std::string_view(glsl_builtin_type_names[56]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector2, 0, 0 } }, // f32vec2
        { std::string_view(glsl_builtin_type_names[57]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector3, 0, 0 } }, // f32vec3
        { std::string_view(glsl_builtin_type_names[58]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Vector4, 0, 0 } }, // f32vec4
        { std::string_view(glsl_builtin_type_names[59]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // f32mat2: 2 columns of f32vec2
        { std::string_view(glsl_builtin_type_names[60]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3: 3 columns of f32vec3 (padded to f32vec4)
        { std::string_view(glsl_builtin_type_names[61]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4: 4 columns of f32vec4
        { std::string_view(glsl_builtin_type_names[62]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // f32mat2x2: 2 columns of f32vec2
        { std::string_view(glsl_builtin_type_names[63]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // f32mat2x3: 2 columns of f32vec3 (padded to f32vec4)
        { std::string_view(glsl_builtin_type_names[64]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // f32mat2x4: 2 columns of f32vec4
        { std::string_view(glsl_builtin_type_names[65]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 2 } }, // f32mat3x2: 3 columns of f32vec2
        { std::string_view(glsl_builtin_type_names[66]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3x3: 3 columns of f32vec3 (padded to f32vec4)
        { std::string_view(glsl_builtin_type_names[67]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3x4: 3 columns of f32vec4
        { std::string_view(glsl_builtin_type_names[68]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 2 } }, // f32mat4x2: 4 columns of f32vec2
        { std::string_view(glsl_builtin_type_names[69]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4x3: 4 columns of f32vec3 (padded to f32vec4)
        { std::string_view(glsl_builtin_type_names[70]), TypeAlignmentAttributes{ UnderlyingComponentType::f32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4x4: 4 columns of f32vec4
        
        // float64_t types
        { std::string_view(glsl_builtin_type_names[71]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Scalar, 0, 0 } }, // float64_t
        { std::string_view(glsl_builtin_type_names[72]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector2, 0, 0 } }, // f64vec2
        { std::string_view(glsl_builtin_type_names[73]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector3, 0, 0 } }, // f64vec3
        { std::string_view(glsl_builtin_type_names[74]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Vector4, 0, 0 } }, // f64vec4
        { std::string_view(glsl_builtin_type_names[75]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // f64mat2: 2 columns of f64vec2
        { std::string_view(glsl_builtin_type_names[76]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3: 3 columns of f64vec3 (padded to f64vec4)
        { std::string_view(glsl_builtin_type_names[77]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4: 4 columns of f64vec4
        { std::string_view(glsl_builtin_type_names[78]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // f64mat2x2: 2 columns of f64vec2
        { std::string_view(glsl_builtin_type_names[79]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // f64mat2x3: 2 columns of f64vec3 (padded to f64vec4)
        { std::string_view(glsl_builtin_type_names[80]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // f64mat2x4: 2 columns of f64vec4
        { std::string_view(glsl_builtin_type_names[81]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 2 } }, // f64mat3x2: 3 columns of f64vec2
        { std::string_view(glsl_builtin_type_names[82]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3x3: 3 columns of f64vec3 (padded to f64vec4)
        { std::string_view(glsl_builtin_type_names[83]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3x4: 3 columns of f64vec4
        { std::string_view(glsl_builtin_type_names[84]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 2 } }, // f64mat4x2: 4 columns of f64vec2
        { std::string_view(glsl_builtin_type_names[85]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4x3: 4 columns of f64vec3 (padded to f64vec4)
        { std::string_view(glsl_builtin_type_names[86]), TypeAlignmentAttributes{ UnderlyingComponentType::f64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4x4: 4 columns of f64vec4
        
        // int64_t types
        { std::string_view(glsl_builtin_type_names[87]), TypeAlignmentAttributes{ UnderlyingComponentType::i64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i64)], TypeDimensionality::Scalar, 0, 0 } }, // int64_t
        { std::string_view(glsl_builtin_type_names[88]), TypeAlignmentAttributes{ UnderlyingComponentType::i64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i64)], TypeDimensionality::Vector2, 0, 0 } }, // i64vec2
        { std::string_view(glsl_builtin_type_names[89]), TypeAlignmentAttributes{ UnderlyingComponentType::i64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i64)], TypeDimensionality::Vector3, 0, 0 } }, // i64vec3
        { std::string_view(glsl_builtin_type_names[90]), TypeAlignmentAttributes{ UnderlyingComponentType::i64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i64)], TypeDimensionality::Vector4, 0, 0 } }, // i64vec4
        { std::string_view(glsl_builtin_type_names[91]), TypeAlignmentAttributes{ UnderlyingComponentType::ui64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui64)], TypeDimensionality::Scalar, 0, 0 } }, // uint64_t
        { std::string_view(glsl_builtin_type_names[92]), TypeAlignmentAttributes{ UnderlyingComponentType::ui64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui64)], TypeDimensionality::Vector2, 0, 0 } }, // u64vec2
        { std::string_view(glsl_builtin_type_names[93]), TypeAlignmentAttributes{ UnderlyingComponentType::ui64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui64)], TypeDimensionality::Vector3, 0, 0 } }, // u64vec3
        { std::string_view(glsl_builtin_type_names[94]), TypeAlignmentAttributes{ UnderlyingComponentType::ui64, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui64)], TypeDimensionality::Vector4, 0, 0 } }, // u64vec4
        
        // int32_t types
        { std::string_view(glsl_builtin_type_names[95]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Scalar, 0, 0 } }, // int32_t
        { std::string_view(glsl_builtin_type_names[96]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector2, 0, 0 } }, // i32vec2
        { std::string_view(glsl_builtin_type_names[97]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector3, 0, 0 } }, // i32vec3
        { std::string_view(glsl_builtin_type_names[98]), TypeAlignmentAttributes{ UnderlyingComponentType::i32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i32)], TypeDimensionality::Vector4, 0, 0 } }, // i32vec4
        { std::string_view(glsl_builtin_type_names[99]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // uint32_t
        { std::string_view(glsl_builtin_type_names[100]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // u32vec2
        { std::string_view(glsl_builtin_type_names[101]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // u32vec3
        { std::string_view(glsl_builtin_type_names[102]), TypeAlignmentAttributes{ UnderlyingComponentType::ui32, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // u32vec4
        
        // int16_t types
        { std::string_view(glsl_builtin_type_names[103]), TypeAlignmentAttributes{ UnderlyingComponentType::i16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i16)], TypeDimensionality::Scalar, 0, 0 } }, // int16_t
        { std::string_view(glsl_builtin_type_names[104]), TypeAlignmentAttributes{ UnderlyingComponentType::i16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i16)], TypeDimensionality::Vector2, 0, 0 } }, // i16vec2
        { std::string_view(glsl_builtin_type_names[105]), TypeAlignmentAttributes{ UnderlyingComponentType::i16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i16)], TypeDimensionality::Vector3, 0, 0 } }, // i16vec3
        { std::string_view(glsl_builtin_type_names[106]), TypeAlignmentAttributes{ UnderlyingComponentType::i16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i16)], TypeDimensionality::Vector4, 0, 0 } }, // i16vec4
        { std::string_view(glsl_builtin_type_names[107]), TypeAlignmentAttributes{ UnderlyingComponentType::ui16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui16)], TypeDimensionality::Scalar, 0, 0 } }, // uint16_t
        { std::string_view(glsl_builtin_type_names[108]), TypeAlignmentAttributes{ UnderlyingComponentType::ui16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui16)], TypeDimensionality::Vector2, 0, 0 } }, // u16vec2
        { std::string_view(glsl_builtin_type_names[109]), TypeAlignmentAttributes{ UnderlyingComponentType::ui16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui16)], TypeDimensionality::Vector3, 0, 0 } }, // u16vec3
        { std::string_view(glsl_builtin_type_names[110]), TypeAlignmentAttributes{ UnderlyingComponentType::ui16, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui16)], TypeDimensionality::Vector4, 0, 0 } }, // u16vec4
        
        // int8_t types
        { std::string_view(glsl_builtin_type_names[111]), TypeAlignmentAttributes{ UnderlyingComponentType::i8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i8)], TypeDimensionality::Scalar, 0, 0 } }, // int8_t
        { std::string_view(glsl_builtin_type_names[112]), TypeAlignmentAttributes{ UnderlyingComponentType::i8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i8)], TypeDimensionality::Vector2, 0, 0 } }, // i8vec2
        { std::string_view(glsl_builtin_type_names[113]), TypeAlignmentAttributes{ UnderlyingComponentType::i8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i8)], TypeDimensionality::Vector3, 0, 0 } }, // i8vec3
        { std::string_view(glsl_builtin_type_names[114]), TypeAlignmentAttributes{ UnderlyingComponentType::i8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::i8)], TypeDimensionality::Vector4, 0, 0 } }, // i8vec4
        { std::string_view(glsl_builtin_type_names[115]), TypeAlignmentAttributes{ UnderlyingComponentType::ui8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui8)], TypeDimensionality::Scalar, 0, 0 } }, // uint8_t
        { std::string_view(glsl_builtin_type_names[116]), TypeAlignmentAttributes{ UnderlyingComponentType::ui8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui8)], TypeDimensionality::Vector2, 0, 0 } }, // u8vec2
        { std::string_view(glsl_builtin_type_names[117]), TypeAlignmentAttributes{ UnderlyingComponentType::ui8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui8)], TypeDimensionality::Vector3, 0, 0 } }, // u8vec3
        { std::string_view(glsl_builtin_type_names[118]), TypeAlignmentAttributes{ UnderlyingComponentType::ui8, component_type_sizes[static_cast<size_t>(UnderlyingComponentType::ui8)], TypeDimensionality::Vector4, 0, 0 } } // u8vec4
    };

    constexpr size_t CalculateScalarAlignment(const TypeAlignmentAttributes& type_attributes) noexcept
    {
        return type_attributes.ComponentSize;
    }

     constexpr size_t CalculateBaseAlignment(const TypeAlignmentAttributes& type_attributes) noexcept
     {
        switch (type_attributes.Dimensionality)
        {
            case TypeDimensionality::Scalar:
                return CalculateScalarAlignment(type_attributes);
            case TypeDimensionality::Vector2:
                return CalculateScalarAlignment(type_attributes) * 2;
            case TypeDimensionality::Vector3:
                [[fallthrough]]; // vec3 aligns same as vec4
            case TypeDimensionality::Vector4:
                return CalculateScalarAlignment(type_attributes) * 4;
            case TypeDimensionality::Array:
                // Arrays align to base alignment of their component type
                if (type_attributes.ElementTypeAttributes != nullptr)
                {
                    return CalculateBaseAlignment(*type_attributes.ElementTypeAttributes);
                }
                else
                {
                    // if ElementTypeAttributes is not set, we can't get a valid alignment for this array type
                    return 0;
                }
            case TypeDimensionality::Matrix:
               // matrix alignment comes from alignment of component type (i.e., what types its column vectors are)
                if (type_attributes.MatrixColumnLength == 2)
                {
                    return CalculateScalarAlignment(type_attributes) * 2;
                }
                else // column length is 3 or 4 (and if 3, it's really 4)
                {
                    return CalculateScalarAlignment(type_attributes) * 4;
                }
            case TypeDimensionality::Struct:
                // Structs align to the largest members base alignment, so we'll handle this case separately before we get here
                [[fallthrough]];
            case TypeDimensionality::Invalid:
                [[fallthrough]];
            default:
                return 0; // Invalid or unsupported dimensionality
        }
    }

    constexpr size_t CalculateExtendedAlignment(const TypeAlignmentAttributes& type_attributes) noexcept
    {
        switch (type_attributes.Dimensionality)
        {
        case TypeDimensionality::Scalar:
            [[fallthrough]];
        case TypeDimensionality::Vector2:
            [[fallthrough]];
        case TypeDimensionality::Vector3:
            [[fallthrough]];
        case TypeDimensionality::Vector4:
            return CalculateBaseAlignment(type_attributes);
        case TypeDimensionality::Matrix:
            [[fallthrough]];
        case TypeDimensionality::Struct:
            [[fallthrough]];
        case TypeDimensionality::Array:
            // Matrices, structs, and arrays all align up to nearest multiple of 16 bytes
            return ((CalculateBaseAlignment(type_attributes) + 15) / 16) * 16;
        }
    }

    constexpr size_t CalculateLayoutForRuleset(const TypeAlignmentAttributes& type_attributes, st::MemoryLayout memory_layout) noexcept
    {
        switch (memory_layout)
        {
            case st::MemoryLayout::Std140:
                return CalculateExtendedAlignment(type_attributes);
            case st::MemoryLayout::Std430:
                return CalculateBaseAlignment(type_attributes);
            case st::MemoryLayout::Scalar:
                return CalculateScalarAlignment(type_attributes);
            default:
                return 0; // Unsupported memory layout
        }
    }

    constexpr size_t CalculateArrayOrMatrixStride(const size_t element_size, const size_t element_alignment, st::MemoryLayout memory_layout) noexcept
    {
        size_t stride = ((element_size + element_alignment - 1) / element_alignment) * element_alignment;
        if (memory_layout == st::MemoryLayout::Std140)
        {
            // Apply std140 layout rules, round up to nearest multiple of 16 bytes
            stride = ((stride + 15) / 16) * 16;
        }
        return stride;
    }

    /**
    * @brief Context or "session" for parsing alignment attributes of types declared in schema definition part of YAML config file.
    * This context is used to store memory layout rules, cache of already-parsed struct types, and whether we are allowed to optimize alignment.
    */
    struct AlignmentParserContext
    {
        /** @brief MemoryLayout rule from user input governs how alignment will work: see lines 143-166 of GLSLTypeAttributes.cpp.
         *  @note Defaults to Std430.
         */
        st::MemoryLayout LayoutRules{ st::MemoryLayout::Std430 };
        /** 
         * @brief Cache of already-parsed struct types that we can check. 
         * Intended to be populated with user-defined structs, in cases of schema definitions that are recursive (i.e., user declares a struct and then uses it in another struct).
         * */
        std::unordered_map<std::string, TypeAlignmentAttributes> StructCache;
        /** 
         * @brief If the alignment parser is allowed to rearrange members of structs to save space and improve alignment. Default is true.
         * Frontends can query ST to still find pointers to relocated members, or get back a struct describing the new layout. We also preserve the original layout
         * for debugging or visualization purposes.
         * @note Not really applicable outside of `Std430` memory layout, `Std140` rounds everything up to 16 bytes and `Scalar` is just a bucket of bytes with no real alignment.
         */
        bool AllowOptimizingAlignment{ true };
    };

    struct AlignmentParserResult
    {
        /** @brief Variable name of the type represented here. */
        std::string_view VarName;
        /** @brief Name of the underlying type represented here. */
        std::string_view TypeName;
        /** @brief Dimensionality of this type (gives us important context for alignment) */
        TypeDimensionality Dimensionality{ TypeDimensionality::Invalid };
        /** @brief Total memory size of this object in bytes */
        size_t Size{ 0u };
        size_t Alignment{ 0u };
        bool IsArrayType{ false };
        /** @brief Stride between consecutive elements in the array (or matrix) in bytes. */
        size_t ArrayStride{ 0u };
        /** @brief Number of elements in the array */
        size_t ArrayLength{ 0u };
        /** @brief If this result is part of a struct, this is the offset of this particular attribute in the struct */
        size_t Offset{ 0u };
        /** @brief If this result is a struct, this contains it's member variables. Not applicable for matrices or arrays of fundamental types, however. */
        std::optional<std::vector<AlignmentParserResult>> Members{ std::nullopt };
    };

    class TypeAlignmentParser
    {
    public:
        
        /**
         * @brief Entrypoint parsing function. Recursively parses `type` string and returns alignment attributes.
         * @param type GLSL type string to parse, e.g. "vec3[2]", "mat4x2", "myStruct[3]", etc.
         * @param context Context containing memory layout rules and struct cache.
         * @return `AlignmentParserResult` containing everything needed to understand memory layout of the type
         */
        AlignmentParserResult ParseType(std::string_view type, AlignmentParserContext& context) noexcept;
    
    private:

        /**
         * @brief Finds TypeAlignmentAttributes for a given type string, first checking struct cache then builtin type cache.
         * @param type_str string of type we're trying to find
         * @return TypeAlignmentAttributes for the type, or an empty optional if not found.
         */
        std::expected<const TypeAlignmentAttributes, ShaderToolsErrorCode> FindTypeAlignmentAttributes(std::string_view type_str) noexcept
        {
            // First check the struct cache
            auto iter = context.StructCache.find(std::string(type_str));
            if (iter != context.StructCache.end())
            {
                return iter->second;
            }

            // Then check the builtin types
            auto builtin_iter = glsl_type_alignment_attributes.find(type_str);
            if (builtin_iter != glsl_type_alignment_attributes.end())
            {
                const TypeAlignmentAttributes result = builtin_iter->second;
                return builtin_iter->second;
            }

            // If not found, return an error
            const std::string error_msg = std::format(
                "Unknown GLSL type: '{}', couldn't retrieve alignment attributes from internal cache.",
                type_str
            );
            return std::unexpected(ShaderToolsErrorCode::AlignmentParserUnknownGLSLType);
        }
        
        /**
         * @brief Parses GLSL type string and extracts base type and array dimensions.
         * @param type_str GLSL type string, e.g. "vec3[2][4]", "myStruct[3]", etc.
         * @return Pair containing base type as string_view and vector of array dimensions.
        */
        std::pair<std::string_view, std::vector<size_t>> parseArrayNotation(std::string_view type_str) noexcept
        {
            std::vector<size_t> dimensions;

            std::string_view baseTypeStr = type_str;

            size_t pos = type_str.find('[');
            if (pos != std::string_view::npos)
            {
                baseTypeStr = type_str.substr(0, pos);

                while (pos < type_str.size())
                {
                    size_t start = pos + 1;
                    size_t end = type_str.find(']', start);

                    if (end == std::string_view::npos)
                    {
                        const std::string error_msg = std::format(
                            "Invalid array notation in type string: '{}'. Missing closing bracket.",
                            type_str
                        );

                        errorSession.AddError(
                            this,
                            ShaderToolsErrorSource::SchemaAlignmentParser,
                            ShaderToolsErrorCode::AlignmentParserMissingEndBracketInArrayDeclaration,
                            error_msg.c_str());
                        
                        break;
                    }

                    size_t dimension = 0;
                    std::string_view dimension_str = type_str.substr(start, end - start);
                    auto from_chars_result = std::from_chars(dimension_str.data(), dimension_str.data() + dimension_str.size(), dimension);
                    if (from_chars_result.ec != std::errc())
                    {
                        const std::string error_msg = std::format(
                            "Invalid array dimension in type string: '{}'.",
                            type_str
                        );

                        errorSession.AddError(
                            this,
                            ShaderToolsErrorSource::SchemaAlignmentParser,
                            ShaderToolsErrorCode::AlignmentParserInvalidArrayDimension,
                            error_msg.c_str()
                        );

                        break;
                    }
                    else
                    {
                        dimensions.emplace_back(dimension);
                        pos = type_str.find('[', end + 1);
                        if (pos == std::string_view::npos)
                        {
                            break; // No more array dimensions
                        }
                    }

                }

                return { baseTypeStr, dimensions };
            }
            else
            {
                return { baseTypeStr, {} };
            }

        }

        /** @brief Parse known GLSL type (terminal node in this mini-grammar). */
        AlignmentParserResult ParseBuiltInType(std::string_view type_str, std::string_view name_str) noexcept
        {
            AlignmentParserResult result;
            result.TypeName = type_str;
            result.IsArrayType = false;
            result.ArrayLength = 0;
            result.ArrayStride = 0;

            auto iter = glsl_type_alignment_attributes.find(type_str);
            if (iter != glsl_type_alignment_attributes.end())
            {
                const TypeAlignmentAttributes& type_attributes = iter->second;
                result.Dimensionality = type_attributes.Dimensionality;
                result.Size = GetBuiltInTypeSizeFromAttributes(type_attributes);
                result.Alignment = CalculateLayoutForRuleset(type_attributes, context.LayoutRules);
                result.Offset = 0;
                result.VarName = name_str;

                if (type_attributes.Dimensionality == TypeDimensionality::Matrix)
                {
                    // Matrices are effectively arrays of column vectors, so we need to set relevant information
                    result.IsArrayType = true;
                    result.ArrayLength = type_attributes.ArraySize;
                    result.ArrayStride = CalculateArrayOrMatrixStride(type_attributes.ComponentSize * type_attributes.MatrixColumnLength, result.Alignment, context.LayoutRules);
                    result.Size = result.ArrayStride * result.ArrayLength;
                }
            }
            else
            {
                const std::string error_msg = std::format(
                    "Unknown GLSL type: '{}', couldn't retrieve alignment attributes from internal cache.",
                    type_str
                );
                errorSession.AddError(
                    this,
                    ShaderToolsErrorSource::SchemaAlignmentParser,
                    ShaderToolsErrorCode::AlignmentParserUnknownGLSLType,
                    error_msg.c_str()
                );
            }

        }

        /** @brief Parses an array type. */
        AlignmentParserResult ParseArrayType(std::string_view type_str, std::string_view name_str) noexcept
        {
            AlignmentParserResult result;
            result.VarName = name_str;

            auto [base_type, dimensions] = parseArrayNotation(type_str);
            
            result.IsArrayType = true;
            result.ArrayLength = dimensions.back();
            result.ArrayStride = 0; // Will be calculated later

            // Parse the base type
            AlignmentParserResult base_result = ParseType(base_type, context);
            if (base_result.Dimensionality == TypeDimensionality::Invalid)
            {
                return result; // Invalid base type, return empty result
            }

            // Find alignment attributes for the base type

            result.TypeName = base_result.TypeName;
            result.Dimensionality = TypeDimensionality::Array;
            result.Size = base_result.Size * result.ArrayLength;
            result.Alignment = CalculateLayoutForRuleset(base_result, context.LayoutRules);
            result.Offset = 0;

            // Calculate array stride based on the base type's size and alignment
            result.ArrayStride = CalculateArrayOrMatrixStride(base_result.Size, base_result.Alignment, context.LayoutRules);

            return result;
        }

        /** @brief Recursively parses a struct type: if struct type contains other user defined structs, we'll need to check the cache. */
        void ParseStructType(std::string_view type_str, std::string_view name_str) noexcept
        {
            // Implementation for parsing struct types
        }

        st::SessionImpl& errorSession;
        /** @brief "session" struct for parsing type data, configures how we parse and what we do. */
        AlignmentParserContext context;
    };

    bool IsArrayType(std::string_view type) noexcept
    {
        size_t pos = type.find('[');
        return pos != std::string_view::npos && type.find(']', pos) != std::string_view::npos;
    }

    std::optional<GLSLTypeAttributes> GetGLSLTypeAttributes(std::string_view type, MemoryLayout memory_layout) noexcept
    {
        return std::nullopt;
    }
}