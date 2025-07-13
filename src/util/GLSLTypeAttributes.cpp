#include "GLSLTypeAttributes.hpp"
#include <unordered_map>
#include <array>
#include <string>
#include <charconv>
#include <stdexcept>

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

namespace alignment_calculator
{

    /**
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
     enum class ComponentType : uint8_t
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
     constexpr static std::array<size_t, static_cast<size_t>(ComponentType::NUM_COMPONENT_TYPES)> component_type_sizes
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
        ComponentType ComponentType{ ComponentType::Invalid };
        size_t ComponentSize{ 0u };
        TypeDimensionality Dimensionality{ TypeDimensionality::Invalid };
        /** Number of elements in array for Array types, or number of columns for Matrix type */
        size_t ArraySize{ 0u };
        /** Number of elements in the column vectors that make up matrix arrays */
        size_t MatrixColumnLength{ 0u };
        /** For array types, parsed name of contained element. Can be a name of a struct type, in which case we'll have to refer back to the current parsing run to back-reference. */
        std::string_view ElementTypename;
     };

     /**
      * @brief Map of GLSL builtin type names to their alignment attributes
      */
     const static std::unordered_map<std::string_view, TypeAlignmentAttributes> glsl_type_alignment_attributes
     {
        // Basic scalar types
        { glsl_builtin_type_names[0], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Scalar, 0, 0 } }, // float
        { glsl_builtin_type_names[1], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Scalar, 0, 0 } }, // double
        { glsl_builtin_type_names[2], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Scalar, 0, 0 } }, // int
        { glsl_builtin_type_names[3], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // uint
        { glsl_builtin_type_names[4], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // bool (treated as uint32)
        
        // Basic vector types
        { glsl_builtin_type_names[5], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector2, 0, 0 } }, // vec2
        { glsl_builtin_type_names[6], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector3, 0, 0 } }, // vec3
        { glsl_builtin_type_names[7], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector4, 0, 0 } }, // vec4
        { glsl_builtin_type_names[8], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector2, 0, 0 } }, // dvec2
        { glsl_builtin_type_names[9], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector3, 0, 0 } }, // dvec3
        { glsl_builtin_type_names[10], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector4, 0, 0 } }, // dvec4
        { glsl_builtin_type_names[11], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector2, 0, 0 } }, // ivec2
        { glsl_builtin_type_names[12], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector3, 0, 0 } }, // ivec3
        { glsl_builtin_type_names[13], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector4, 0, 0 } }, // ivec4
        { glsl_builtin_type_names[14], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // uvec2
        { glsl_builtin_type_names[15], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // uvec3
        { glsl_builtin_type_names[16], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // uvec4
        { glsl_builtin_type_names[17], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // bvec2 (treated as uint32)
        { glsl_builtin_type_names[18], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // bvec3 (treated as uint32)
        { glsl_builtin_type_names[19], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // bvec4 (treated as uint32)
        
        // Base float matrix types
        { glsl_builtin_type_names[20], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // mat2: 2 columns of vec2
        { glsl_builtin_type_names[21], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // mat2x3: 2 columns of vec3 (padded to vec4)
        { glsl_builtin_type_names[22], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // mat2x4: 2 columns of vec4
        { glsl_builtin_type_names[23], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3: 3 columns of vec3 (padded to vec4)
        { glsl_builtin_type_names[24], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 2 } }, // mat3x2: 3 columns of vec2
        { glsl_builtin_type_names[25], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3x3: 3 columns of vec3 (padded to vec4)
        { glsl_builtin_type_names[26], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // mat3x4: 3 columns of vec4
        { glsl_builtin_type_names[27], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // mat4: 4 columns of vec4
        { glsl_builtin_type_names[28], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 2 } }, // mat4x2: 4 columns of vec2
        { glsl_builtin_type_names[29], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // mat4x3: 4 columns of vec3 (padded to vec4)
        
        // Double matrix types
        { glsl_builtin_type_names[30], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // dmat2: 2 columns of dvec2
        { glsl_builtin_type_names[31], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // dmat2x3: 2 columns of dvec3 (padded to dvec4)
        { glsl_builtin_type_names[32], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // dmat2x4: 2 columns of dvec4
        { glsl_builtin_type_names[33], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // dmat3: 3 columns of dvec3 (padded to dvec4)
        { glsl_builtin_type_names[34], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // dmat3x4: 3 columns of dvec4
        { glsl_builtin_type_names[35], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4: 4 columns of dvec4
        { glsl_builtin_type_names[36], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 2 } }, // dmat4x2: 4 columns of dvec2
        { glsl_builtin_type_names[37], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4x3: 4 columns of dvec3 (padded to dvec4)
        { glsl_builtin_type_names[38], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // dmat4x4: 4 columns of dvec4
        
        // float16_t types
        { glsl_builtin_type_names[39], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Scalar, 0, 0 } }, // float16_t
        { glsl_builtin_type_names[40], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Vector2, 0, 0 } }, // f16vec2
        { glsl_builtin_type_names[41], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Vector3, 0, 0 } }, // f16vec3
        { glsl_builtin_type_names[42], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Vector4, 0, 0 } }, // f16vec4
        { glsl_builtin_type_names[43], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 2, 2 } }, // f16mat2: 2 columns of f16vec2
        { glsl_builtin_type_names[44], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3: 3 columns of f16vec3 (padded to f16vec4)
        { glsl_builtin_type_names[45], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4: 4 columns of f16vec4
        { glsl_builtin_type_names[46], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 2, 2 } }, // f16mat2x2: 2 columns of f16vec2
        { glsl_builtin_type_names[47], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 2, 4 } }, // f16mat2x3: 2 columns of f16vec3 (padded to f16vec4)
        { glsl_builtin_type_names[48], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 2, 4 } }, // f16mat2x4: 2 columns of f16vec4
        { glsl_builtin_type_names[49], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 3, 2 } }, // f16mat3x2: 3 columns of f16vec2
        { glsl_builtin_type_names[50], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3x3: 3 columns of f16vec3 (padded to f16vec4)
        { glsl_builtin_type_names[51], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 3, 4 } }, // f16mat3x4: 3 columns of f16vec4
        { glsl_builtin_type_names[52], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 4, 2 } }, // f16mat4x2: 4 columns of f16vec2
        { glsl_builtin_type_names[53], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4x3: 4 columns of f16vec3 (padded to f16vec4)
        { glsl_builtin_type_names[54], { ComponentType::f16, component_type_sizes[static_cast<size_t>(ComponentType::f16)], TypeDimensionality::Matrix, 4, 4 } }, // f16mat4x4: 4 columns of f16vec4
        
        // float32_t types
        { glsl_builtin_type_names[55], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Scalar, 0, 0 } }, // float32_t
        { glsl_builtin_type_names[56], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector2, 0, 0 } }, // f32vec2
        { glsl_builtin_type_names[57], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector3, 0, 0 } }, // f32vec3
        { glsl_builtin_type_names[58], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Vector4, 0, 0 } }, // f32vec4
        { glsl_builtin_type_names[59], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // f32mat2: 2 columns of f32vec2
        { glsl_builtin_type_names[60], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3: 3 columns of f32vec3 (padded to f32vec4)
        { glsl_builtin_type_names[61], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4: 4 columns of f32vec4
        { glsl_builtin_type_names[62], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 2 } }, // f32mat2x2: 2 columns of f32vec2
        { glsl_builtin_type_names[63], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // f32mat2x3: 2 columns of f32vec3 (padded to f32vec4)
        { glsl_builtin_type_names[64], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 2, 4 } }, // f32mat2x4: 2 columns of f32vec4
        { glsl_builtin_type_names[65], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 2 } }, // f32mat3x2: 3 columns of f32vec2
        { glsl_builtin_type_names[66], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3x3: 3 columns of f32vec3 (padded to f32vec4)
        { glsl_builtin_type_names[67], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 3, 4 } }, // f32mat3x4: 3 columns of f32vec4
        { glsl_builtin_type_names[68], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 2 } }, // f32mat4x2: 4 columns of f32vec2
        { glsl_builtin_type_names[69], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4x3: 4 columns of f32vec3 (padded to f32vec4)
        { glsl_builtin_type_names[70], { ComponentType::f32, component_type_sizes[static_cast<size_t>(ComponentType::f32)], TypeDimensionality::Matrix, 4, 4 } }, // f32mat4x4: 4 columns of f32vec4
        
        // float64_t types
        { glsl_builtin_type_names[71], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Scalar, 0, 0 } }, // float64_t
        { glsl_builtin_type_names[72], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector2, 0, 0 } }, // f64vec2
        { glsl_builtin_type_names[73], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector3, 0, 0 } }, // f64vec3
        { glsl_builtin_type_names[74], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Vector4, 0, 0 } }, // f64vec4
        { glsl_builtin_type_names[75], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // f64mat2: 2 columns of f64vec2
        { glsl_builtin_type_names[76], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3: 3 columns of f64vec3 (padded to f64vec4)
        { glsl_builtin_type_names[77], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4: 4 columns of f64vec4
        { glsl_builtin_type_names[78], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 2 } }, // f64mat2x2: 2 columns of f64vec2
        { glsl_builtin_type_names[79], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // f64mat2x3: 2 columns of f64vec3 (padded to f64vec4)
        { glsl_builtin_type_names[80], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 2, 4 } }, // f64mat2x4: 2 columns of f64vec4
        { glsl_builtin_type_names[81], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 2 } }, // f64mat3x2: 3 columns of f64vec2
        { glsl_builtin_type_names[82], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3x3: 3 columns of f64vec3 (padded to f64vec4)
        { glsl_builtin_type_names[83], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 3, 4 } }, // f64mat3x4: 3 columns of f64vec4
        { glsl_builtin_type_names[84], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 2 } }, // f64mat4x2: 4 columns of f64vec2
        { glsl_builtin_type_names[85], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4x3: 4 columns of f64vec3 (padded to f64vec4)
        { glsl_builtin_type_names[86], { ComponentType::f64, component_type_sizes[static_cast<size_t>(ComponentType::f64)], TypeDimensionality::Matrix, 4, 4 } }, // f64mat4x4: 4 columns of f64vec4
        
        // int64_t types
        { glsl_builtin_type_names[87], { ComponentType::i64, component_type_sizes[static_cast<size_t>(ComponentType::i64)], TypeDimensionality::Scalar, 0, 0 } }, // int64_t
        { glsl_builtin_type_names[88], { ComponentType::i64, component_type_sizes[static_cast<size_t>(ComponentType::i64)], TypeDimensionality::Vector2, 0, 0 } }, // i64vec2
        { glsl_builtin_type_names[89], { ComponentType::i64, component_type_sizes[static_cast<size_t>(ComponentType::i64)], TypeDimensionality::Vector3, 0, 0 } }, // i64vec3
        { glsl_builtin_type_names[90], { ComponentType::i64, component_type_sizes[static_cast<size_t>(ComponentType::i64)], TypeDimensionality::Vector4, 0, 0 } }, // i64vec4
        { glsl_builtin_type_names[91], { ComponentType::ui64, component_type_sizes[static_cast<size_t>(ComponentType::ui64)], TypeDimensionality::Scalar, 0, 0 } }, // uint64_t
        { glsl_builtin_type_names[92], { ComponentType::ui64, component_type_sizes[static_cast<size_t>(ComponentType::ui64)], TypeDimensionality::Vector2, 0, 0 } }, // u64vec2
        { glsl_builtin_type_names[93], { ComponentType::ui64, component_type_sizes[static_cast<size_t>(ComponentType::ui64)], TypeDimensionality::Vector3, 0, 0 } }, // u64vec3
        { glsl_builtin_type_names[94], { ComponentType::ui64, component_type_sizes[static_cast<size_t>(ComponentType::ui64)], TypeDimensionality::Vector4, 0, 0 } }, // u64vec4
        
        // int32_t types
        { glsl_builtin_type_names[95], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Scalar, 0, 0 } }, // int32_t
        { glsl_builtin_type_names[96], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector2, 0, 0 } }, // i32vec2
        { glsl_builtin_type_names[97], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector3, 0, 0 } }, // i32vec3
        { glsl_builtin_type_names[98], { ComponentType::i32, component_type_sizes[static_cast<size_t>(ComponentType::i32)], TypeDimensionality::Vector4, 0, 0 } }, // i32vec4
        { glsl_builtin_type_names[99], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Scalar, 0, 0 } }, // uint32_t
        { glsl_builtin_type_names[100], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector2, 0, 0 } }, // u32vec2
        { glsl_builtin_type_names[101], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector3, 0, 0 } }, // u32vec3
        { glsl_builtin_type_names[102], { ComponentType::ui32, component_type_sizes[static_cast<size_t>(ComponentType::ui32)], TypeDimensionality::Vector4, 0, 0 } }, // u32vec4
        
        // int16_t types
        { glsl_builtin_type_names[103], { ComponentType::i16, component_type_sizes[static_cast<size_t>(ComponentType::i16)], TypeDimensionality::Scalar, 0, 0 } }, // int16_t
        { glsl_builtin_type_names[104], { ComponentType::i16, component_type_sizes[static_cast<size_t>(ComponentType::i16)], TypeDimensionality::Vector2, 0, 0 } }, // i16vec2
        { glsl_builtin_type_names[105], { ComponentType::i16, component_type_sizes[static_cast<size_t>(ComponentType::i16)], TypeDimensionality::Vector3, 0, 0 } }, // i16vec3
        { glsl_builtin_type_names[106], { ComponentType::i16, component_type_sizes[static_cast<size_t>(ComponentType::i16)], TypeDimensionality::Vector4, 0, 0 } }, // i16vec4
        { glsl_builtin_type_names[107], { ComponentType::ui16, component_type_sizes[static_cast<size_t>(ComponentType::ui16)], TypeDimensionality::Scalar, 0, 0 } }, // uint16_t
        { glsl_builtin_type_names[108], { ComponentType::ui16, component_type_sizes[static_cast<size_t>(ComponentType::ui16)], TypeDimensionality::Vector2, 0, 0 } }, // u16vec2
        { glsl_builtin_type_names[109], { ComponentType::ui16, component_type_sizes[static_cast<size_t>(ComponentType::ui16)], TypeDimensionality::Vector3, 0, 0 } }, // u16vec3
        { glsl_builtin_type_names[110], { ComponentType::ui16, component_type_sizes[static_cast<size_t>(ComponentType::ui16)], TypeDimensionality::Vector4, 0, 0 } }, // u16vec4
        
        // int8_t types
        { glsl_builtin_type_names[111], { ComponentType::i8, component_type_sizes[static_cast<size_t>(ComponentType::i8)], TypeDimensionality::Scalar, 0, 0 } }, // int8_t
        { glsl_builtin_type_names[112], { ComponentType::i8, component_type_sizes[static_cast<size_t>(ComponentType::i8)], TypeDimensionality::Vector2, 0, 0 } }, // i8vec2
        { glsl_builtin_type_names[113], { ComponentType::i8, component_type_sizes[static_cast<size_t>(ComponentType::i8)], TypeDimensionality::Vector3, 0, 0 } }, // i8vec3
        { glsl_builtin_type_names[114], { ComponentType::i8, component_type_sizes[static_cast<size_t>(ComponentType::i8)], TypeDimensionality::Vector4, 0, 0 } }, // i8vec4
        { glsl_builtin_type_names[115], { ComponentType::ui8, component_type_sizes[static_cast<size_t>(ComponentType::ui8)], TypeDimensionality::Scalar, 0, 0 } }, // uint8_t
        { glsl_builtin_type_names[116], { ComponentType::ui8, component_type_sizes[static_cast<size_t>(ComponentType::ui8)], TypeDimensionality::Vector2, 0, 0 } }, // u8vec2
        { glsl_builtin_type_names[117], { ComponentType::ui8, component_type_sizes[static_cast<size_t>(ComponentType::ui8)], TypeDimensionality::Vector3, 0, 0 } }, // u8vec3
        { glsl_builtin_type_names[118], { ComponentType::ui8, component_type_sizes[static_cast<size_t>(ComponentType::ui8)], TypeDimensionality::Vector4, 0, 0 } } // u8vec4
     };

     TypeAlignmentAttributes ExtractTypeAlignmentAttributes(std::string_view type_string)
     {
        if (type_string.empty())
        {
           return TypeAlignmentAttributes{};
        }
        

        auto iter = glsl_type_alignment_attributes.find(type_string);
        if (iter != glsl_type_alignment_attributes.end())
        {
           return iter->second;
        }
        else
        {
            // Now we need to handle the case where the type is not found in the map. 
            // First, check to see if it's an array type
            if (type_string.back() == ']')
            {
                // Extract the base type and array size
                size_t array_start = type_string.find('[');
                if (array_start != std::string_view::npos)
                {
                    std::string_view base_type = type_string.substr(0, array_start);
                    std::string_view array_size_str = type_string.substr(array_start + 1, type_string.size() - array_start - 2);
                    size_t array_size = std::stoul(std::string(array_size_str));

                    // now we try to find the TypeAlignmentAttributes for the base type
                    auto base_iter = glsl_type_alignment_attributes.find(base_type);
                    if (base_iter != glsl_type_alignment_attributes.end())
                    {
                        TypeAlignmentAttributes base_attributes = base_iter->second;
                        base_attributes.Dimensionality = TypeDimensionality::Array;
                        base_attributes.ArraySize = array_size;
                        return base_attributes;
                    }
                    else
                    {
                        // The type is probably a custom struct type, so we need to find the attributes for that struct
                        // I'm not doing that yet because I really don't want to do that right now lol
                        return TypeAlignmentAttributes{};
                    }            
                }
            }
            else
            {
                // Type is probably a custom struct type, and we're still not handling that yet
                return TypeAlignmentAttributes{};
            }
        }
     }

     struct TypeAlignmentResult
     {
        size_t Size{ 0u};
        size_t Alignment{ 0u };
        bool IsArrayType{ false };
        size_t ArraySize{ 0u };
     };

     void SetAlignmentForArrayStd430(const TypeAlignmentAttributes& attributes, TypeAlignmentResult& result)
     {
        
     }

     void SetAlignmentForStd430(const TypeAlignmentAttributes& attributes, TypeAlignmentResult& result)
     {
        switch (attributes.Dimensionality)
        {
            case TypeDimensionality::Scalar:
                result.Size = attributes.ComponentSize;
                // Scalar alignment in std430 is the size of the component type
                result.Alignment = attributes.ComponentSize;
                break;
            case TypeDimensionality::Vector2:
                result.Size = attributes.ComponentSize * 2;
                // std430 vector2 alignment is 2 * component size
                result.Alignment = result.Size;
                break;

            case TypeDimensionality::Vector3:
                [[fallthrough]]; // Vector3 is treated as Vector4 in std430
            case TypeDimensionality::Vector4: // also matches for vector3
                // std430 alignment for vector3 and vector4 is 4 * component size
                result.Size = attributes.ComponentSize * 4;
                result.Alignment = result.Size;
                break;
            case TypeDimensionality::Array:
                SetAlignmentForArrayStd430(attributes, result);
                break;
            case TypeDimensionality::Matrix:
                // Matrix alignment in std430 is the alignment of their column vectors (same as arrays, alignment of element type)
                result.Size = attributes.ComponentSize * attributes.ArraySize * attributes.MatrixColumnLength;
                // so as mentioned above, alignment is just equivalent to what we'd find for a vec2/vec3/vec4 
                result.Alignment = attributes.ComponentSize * attributes.MatrixColumnLength;
                break;
            default:
                // Handle other dimensionalities if necessary
                break;
        }
     }

     void SetAlignmentForStd140(const TypeAlignmentAttributes& attributes, TypeAlignmentResult& result)
     {
        // This alignment is more challenging, especially in struct case. 
        switch (attributes.Dimensionality)
        {
            case TypeDimensionality::Scalar:

                break;

            case TypeDimensionality::Vector2:

                break;

            case TypeDimensionality::Vector3:

                break;

            case TypeDimensionality::Vector4:

                break;

            case TypeDimensionality::Matrix:

                break;

            default:
                // Handle other dimensionalities if necessary
                break;
        }
     }

     void SetAlignmentForScalar(const TypeAlignmentAttributes& attributes, TypeAlignmentResult& result)
     {
        
     }

     TypeAlignmentResult GetFinalAlignmentResult(const TypeAlignmentAttributes& attributes, st::MemoryLayout layout_rules)
     {
        TypeAlignmentResult result;

        // Calculate the final size and alignment based on the layout rules
        switch (layout_rules)
        {
            case st::MemoryLayout::Std430:
                SetAlignmentForStd430(attributes, result);
                break;

            case st::MemoryLayout::Std140:
                SetAlignmentForStd140(attributes, result);
                break;
            case st::MemoryLayout::Scalar:
                SetAlignmentForScalar(attributes, result);
                break;
            default:
                throw std::runtime_error("Unsupported memory layout");
        }

        result.IsArrayType = attributes.Dimensionality == TypeDimensionality::Array;
        result.ArraySize = attributes.ArraySize;

        return result;
     }

}

namespace st
{
    bool IsArrayType(std::string_view type) noexcept
    {
        size_t pos = type.find('[');
        return pos != std::string_view::npos && type.find(']', pos) != std::string_view::npos;
    }

    std::optional<GLSLTypeAttributes> GetGLSLTypeAttributes(std::string_view type, MemoryLayout memory_layout) noexcept
    {
        if (!IsArrayType(type))
        {
            auto it = glsl_builtin_type_attributes_std430.find(type);
            if (it != glsl_builtin_type_attributes_std430.end())
            {
                return it->second;
            }
        }
        else
        {
            // Extract underlying type by getting everything before the first '['
            size_t pos = type.find('[');
            std::string_view base_type = type.substr(0, pos);
            auto it = glsl_builtin_type_attributes_std430.find(base_type);
            if (it != glsl_builtin_type_attributes_std430.end())
            {
                // okay, now we've found the base type: we need to extract the array size from between the brackets
                size_t end_pos = type.find(']', pos);
                if (end_pos != std::string_view::npos)
                {
                    std::string_view array_size_str = type.substr(pos + 1, end_pos - pos - 1);
                    // use charconv to convert the string to an integer real quick like
                    size_t array_size = 0;
                    if (auto [ptr, ec] = std::from_chars(array_size_str.data(), array_size_str.data() + array_size_str.size(), array_size); ec == std::errc())
                    {
                        GLSLTypeAttributes attrs = it->second;
                        attrs.ElementTypename = base_type;
                        attrs.ElementSize = it->second.Size; // store the element size
                        attrs.Size *= array_size; // multiply the size by the array size
                        attrs.IsArrayType = true;
                        attrs.ArraySize = array_size; // store the array size
                        return attrs;
                    }
                    else
                    {
                        // if we can't convert the array size, we return an empty optional
                        return std::nullopt;
                    }
                }
            }
        }

        return std::nullopt;
    }
}