#include "GLSLTypeAttributes.hpp"
#include <unordered_map>
#include <array>
#include <string>
#include <charconv>

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
     * "Scalar" alignment qualifier is the first ruleset, just using the scalar alignment of the type.
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

     enum class TypeDimensionality : uint8_t
     {
        Scalar,
        Vector,
        Matrix,
        Array,
        Struct,
        Invalid
     };

     struct MatrixDims
     {
        uint8_t Rows{ 0u };
        uint8_t Columns{ 0u };
     };

     struct TypeAlignmentAttributes
     {
        ComponentType ComponentType{ ComponentType::Invalid };
        size_t ComponentSize{ 0u };
        TypeDimensionality Dimensionality{ TypeDimensionality::Invalid };
        MatrixDims MatrixDimensions; // Only valid if Dimensionality is Matrix
        size_t ArraySize{ 0u };
     };

     TypeAlignmentAttributes ExtractTypeAlignmentAttributes(std::string_view type_string)
     {
        if (type_string.empty())
        {
           return TypeAlignmentAttributes{};
        }
        


        return TypeAlignmentAttributes{};
     }

     struct TypeAlignmentResult
     {
        size_t Size{ 0u};
        size_t Alignment{ 0u };
        bool IsArrayType{ false };
        size_t ArraySize{ 0u };
     };

     TypeAlignmentResult GetFinalAlignmentResult(const TypeAlignmentAttributes& attributes)
     {
        TypeAlignmentResult result;

        return result;
     }

}

const static std::unordered_map<std::string_view, st::GLSLTypeAttributes> glsl_builtin_type_attributes_std430
{
    // Basic scalar types
    { glsl_builtin_type_names[0], { sizeof(float), sizeof(float), false, "", 0 } }, // float
    { glsl_builtin_type_names[1], { sizeof(double), sizeof(double), false, "", 0 } }, // double
    { glsl_builtin_type_names[2], { sizeof(int), sizeof(int), false, "", 0 } }, // int
    { glsl_builtin_type_names[3], { sizeof(unsigned int), sizeof(unsigned int), false, "", 0 } }, // uint
    { glsl_builtin_type_names[4], { sizeof(bool), sizeof(bool), false, "", 0 } }, // bool
    
    // Basic vector types
    { glsl_builtin_type_names[5], { sizeof(float) * 2, sizeof(float) * 2, false, "", 0 } }, // vec2
    { glsl_builtin_type_names[6], { sizeof(float) * 3, sizeof(float) * 4, false, "", 0 } }, // vec3 (alignment 4*N for 3-element)
    { glsl_builtin_type_names[7], { sizeof(float) * 4, sizeof(float) * 4, false, "", 0 } }, // vec4
    { glsl_builtin_type_names[8], { sizeof(double) * 2, sizeof(double) * 2, false, "", 0 } }, // dvec2
    { glsl_builtin_type_names[9], { sizeof(double) * 3, sizeof(double) * 4, false, "", 0 } }, // dvec3 (alignment 4*N for 3-element)
    { glsl_builtin_type_names[10], { sizeof(double) * 4, sizeof(double) * 4, false, "", 0 } }, // dvec4
    { glsl_builtin_type_names[11], { sizeof(int) * 2, sizeof(int) * 2, false, "", 0 } }, // ivec2
    { glsl_builtin_type_names[12], { sizeof(int) * 3, sizeof(int) * 4, false, "", 0 } }, // ivec3 (alignment 4*N for 3-element)
    { glsl_builtin_type_names[13], { sizeof(int) * 4, sizeof(int) * 4, false, "", 0 } }, // ivec4
    { glsl_builtin_type_names[14], { sizeof(unsigned int) * 2, sizeof(unsigned int) * 2, false, "", 0 } }, // uvec2
    { glsl_builtin_type_names[15], { sizeof(unsigned int) * 3, sizeof(unsigned int) * 4, false, "", 0 } }, // uvec3 (alignment 4*N for 3-element)
    { glsl_builtin_type_names[16], { sizeof(unsigned int) * 4, sizeof(unsigned int) * 4, false, "", 0 } }, // uvec4
    { glsl_builtin_type_names[17], { sizeof(bool) * 2, sizeof(bool) * 2, false, "", 0 } }, // bvec2
    { glsl_builtin_type_names[18], { sizeof(bool) * 3, sizeof(bool) * 4, false, "", 0 } }, // bvec3 (alignment 4*N for 3-element)
    { glsl_builtin_type_names[19], { sizeof(bool) * 4, sizeof(bool) * 4, false, "", 0 } }, // bvec4
    
    // Base float matrix types
    { glsl_builtin_type_names[20], { sizeof(float) * 2 * 2, sizeof(float) * 2, false, "", 0 } }, // mat2: 2 columns of vec2, align=8, size=16
    { glsl_builtin_type_names[21], { sizeof(float) * 2 * 4, sizeof(float) * 2, false, "", 0 } }, // mat2x3: 2 columns of vec4, align=8, size=32
    { glsl_builtin_type_names[22], { sizeof(float) * 2 * 4, sizeof(float) * 2, false, "", 0 } }, // mat2x4: 2 columns of vec4, align=8, size=32
    { glsl_builtin_type_names[23], { sizeof(float) * 3 * 4, sizeof(float) * 4, false, "", 0 } }, // mat3: 3 columns of vec4, align=16, size=48
    { glsl_builtin_type_names[24], { sizeof(float) * 3 * 2, sizeof(float) * 4, false, "", 0 } }, // mat3x2: 3 columns of vec2, align=16, size=24
    { glsl_builtin_type_names[25], { sizeof(float) * 3 * 4, sizeof(float) * 4, false, "", 0 } }, // mat3x3: 3 columns of vec3 (actually vec4s), align=16, size=48
    { glsl_builtin_type_names[26], { sizeof(float) * 3 * 4, sizeof(float) * 4, false, "", 0 } }, // mat3x4: 3 columns of vec4, align=16, size=48
    { glsl_builtin_type_names[27], { sizeof(float) * 4 * 4, sizeof(float) * 4, false, "", 0 } }, // mat4: 4 columns of vec4, align=16, size=64
    { glsl_builtin_type_names[28], { sizeof(float) * 4 * 2, sizeof(float) * 4, false, "", 0 } }, // mat4x2: 4 columns of vec2, align=16, size=32
    { glsl_builtin_type_names[29], { sizeof(float) * 4 * 4, sizeof(float) * 4, false, "", 0 } }, // mat4x3: 4 columns of vec3(4), align=16, size=64
    
    // Double matrix types (highp)
    { glsl_builtin_type_names[30], { sizeof(double) * 2 * 2, sizeof(double) * 2, false, "", 0 } }, // dmat2: 2 columns of dvec2, align=16, size=32
    { glsl_builtin_type_names[31], { sizeof(double) * 2 * 4, sizeof(double) * 2, false, "", 0 } }, // dmat2x3: 2 columns of dvec4, align=16, size=64
    { glsl_builtin_type_names[32], { sizeof(double) * 2 * 4, sizeof(double) * 2, false, "", 0 } }, // dmat2x4: 2 columns of dvec4, align=16, size=64
    { glsl_builtin_type_names[33], { sizeof(double) * 3 * 4, sizeof(double) * 4, false, "", 0 } }, // dmat3: 3 columns of dvec3(4), align=32, size=96
    { glsl_builtin_type_names[34], { sizeof(double) * 3 * 4, sizeof(double) * 4, false, "", 0 } }, // dmat3x4: 3 columns of dvec4, align=32, size=96
    { glsl_builtin_type_names[35], { sizeof(double) * 4 * 4, sizeof(double) * 4, false, "", 0 } }, // dmat4: 4 columns of dvec4, align=32, size=128
    { glsl_builtin_type_names[36], { sizeof(double) * 4 * 2, sizeof(double) * 4, false, "", 0 } }, // dmat4x2: 4 columns of dvec2, align=32, size=64
    { glsl_builtin_type_names[37], { sizeof(double) * 4 * 4, sizeof(double) * 4, false, "", 0 } }, // dmat4x3: 4 columns of dvec3(4), align=32, size=128
    { glsl_builtin_type_names[38], { sizeof(double) * 4 * 4, sizeof(double) * 4, false, "", 0 } }, // dmat4x4: 4 columns of dvec4, align=32, size=128
    
    // float16_t types (mediump)
    { glsl_builtin_type_names[39], { 2, 2, false, "", 0 } }, // float16_t: align=2, size=2
    { glsl_builtin_type_names[40], { 2 * 2, 2 * 2, false, "", 0 } }, // f16vec2: align=4, size=4
    { glsl_builtin_type_names[41], { 2 * 3, 2 * 4, false, "", 0 } }, // f16vec3: align=8, size=6
    { glsl_builtin_type_names[42], { 2 * 4, 2 * 4, false, "", 0 } }, // f16vec4: align=8, size=8
    { glsl_builtin_type_names[43], { 2 * 2 * 2, 2 * 2, false, "", 0 } }, // f16mat2: 2 columns of f16vec2, align=4, size=8
    { glsl_builtin_type_names[44], { 2 * 3 * 3, 2 * 4, false, "", 0 } }, // f16mat3: 3 columns of f16vec3, align=8, size=18
    { glsl_builtin_type_names[45], { 2 * 4 * 4, 2 * 4, false, "", 0 } }, // f16mat4: 4 columns of f16vec4, align=8, size=32
    { glsl_builtin_type_names[46], { 2 * 2 * 2, 2 * 2, false, "", 0 } }, // f16mat2x2: 2 columns of f16vec2, align=4, size=8
    { glsl_builtin_type_names[47], { 2 * 2 * 4, 2 * 2, false, "", 0 } }, // f16mat2x3: 2 columns of f16vec3(4), align=4, size=16
    { glsl_builtin_type_names[48], { 2 * 2 * 4, 2 * 2, false, "", 0 } }, // f16mat2x4: 2 columns of f16vec4, align=4, size=16
    { glsl_builtin_type_names[49], { 2 * 3 * 2, 2 * 2, false, "", 0 } }, // f16mat3x2: 3 columns of f16vec2, align=4, size=12
    { glsl_builtin_type_names[50], { 2 * 3 * 4, 2 * 4, false, "", 0 } }, // f16mat3x3: 3 columns of f16vec3(4), align=8, size=24
    { glsl_builtin_type_names[51], { 2 * 3 * 4, 2 * 4, false, "", 0 } }, // f16mat3x4: 3 columns of f16vec4, align=8, size=24
    { glsl_builtin_type_names[52], { 2 * 4 * 2, 2 * 4, false, "", 0 } }, // f16mat4x2: 4 columns of f16vec2, align=8, size=16
    { glsl_builtin_type_names[53], { 2 * 4 * 4, 2 * 4, false, "", 0 } }, // f16mat4x3: 4 columns of f16vec3(4), align=8, size=32
    { glsl_builtin_type_names[54], { 2 * 4 * 4, 2 * 4, false, "", 0 } }, // f16mat4x4: 4 columns of f16vec4, align=8, size=32
    
    // float32_t types (same as float, just explicitly typed)
    { glsl_builtin_type_names[55], { 4, 4, false, "", 0 } }, // float32_t: align=4, size=4
    { glsl_builtin_type_names[56], { 4 * 2, 4 * 2, false, "", 0 } }, // f32vec2: align=8, size=8
    { glsl_builtin_type_names[57], { 4 * 3, 4 * 4, false, "", 0 } }, // f32vec3: align=16, size=12
    { glsl_builtin_type_names[58], { 4 * 4, 4 * 4, false, "", 0 } }, // f32vec4: align=16, size=16
    { glsl_builtin_type_names[59], { 4 * 2 * 2, 4 * 2, false, "", 0 } }, // f32mat2: 2 columns of f32vec2, align=8, size=16
    { glsl_builtin_type_names[60], { 4 * 3 * 3, 4 * 4, false, "", 0 } }, // f32mat3: 3 columns of f32vec4, align=16, size=48
    { glsl_builtin_type_names[61], { 4 * 4 * 4, 4 * 4, false, "", 0 } }, // f32mat4: 4 columns of f32vec4, align=16, size=64
    { glsl_builtin_type_names[62], { 4 * 2 * 2, 4 * 2, false, "", 0 } }, // f32mat2x2: 2 columns of f32vec2, align=8, size=16
    { glsl_builtin_type_names[63], { 4 * 2 * 4, 4 * 2, false, "", 0 } }, // f32mat2x3: 2 columns of f32vec3(4), align=8, size=32
    { glsl_builtin_type_names[64], { 4 * 2 * 4, 4 * 2, false, "", 0 } }, // f32mat2x4: 2 columns of f32vec4, align=8, size=32
    { glsl_builtin_type_names[65], { 4 * 3 * 2, 2 * 4, false, "", 0 } }, // f32mat3x2: 3 columns of f32vec2, align=8, size=24
    { glsl_builtin_type_names[66], { 4 * 3 * 3, 4 * 4, false, "", 0 } }, // f32mat3x3: 3 columns of f32vec3(4), align=16, size=48
    { glsl_builtin_type_names[67], { 4 * 3 * 4, 4 * 4, false, "", 0 } }, // f32mat3x4: 3 columns of f32vec4, align=16, size=48
    { glsl_builtin_type_names[68], { 4 * 4 * 2, 4 * 4, false, "", 0 } }, // f32mat4x2: 4 columns of f32vec2, align=16, size=32
    { glsl_builtin_type_names[69], { 4 * 4 * 3, 4 * 4, false, "", 0 } }, // f32mat4x3: 4 columns of f32vec3(4), align=16, size=64
    { glsl_builtin_type_names[70], { 4 * 4 * 4, 4 * 4, false, "", 0 } }, // f32mat4x4: 4 columns of f32vec4, align=16, size=64
    
    // float64_t types (same as double)
    { glsl_builtin_type_names[71], { 8, 8, false, "", 0 } }, // float64_t: align=8, size=8
    { glsl_builtin_type_names[72], { 8 * 2, 8 * 2, false, "", 0 } }, // f64vec2: align=16, size=16
    { glsl_builtin_type_names[73], { 8 * 3, 8 * 4, false, "", 0 } }, // f64vec3: align=32, size=24
    { glsl_builtin_type_names[74], { 8 * 4, 8 * 4, false, "", 0 } }, // f64vec4: align=32, size=32
    { glsl_builtin_type_names[75], { 8 * 2 * 2, 8 * 2, false, "", 0 } }, // f64mat2: 2 columns of f64vec2, align=16, size=32
    { glsl_builtin_type_names[76], { 8 * 3 * 3, 8 * 4, false, "", 0 } }, // f64mat3: 3 columns of f64vec3, align=32, size=72
    { glsl_builtin_type_names[77], { 8 * 4 * 4, 8 * 4, false, "", 0 } }, // f64mat4: 4 columns of f64vec4, align=32, size=128
    { glsl_builtin_type_names[78], { 8 * 2 * 2, 8 * 2, false, "", 0 } }, // f64mat2x2: 2 columns of f64vec2, align=16, size=32
    { glsl_builtin_type_names[79], { 8 * 2 * 3, 8 * 2, false, "", 0 } }, // f64mat2x3: 2 columns of f64vec3, align=16, size=48
    { glsl_builtin_type_names[80], { 8 * 2 * 4, 8 * 2, false, "", 0 } }, // f64mat2x4: 2 columns of f64vec4, align=16, size=64
    { glsl_builtin_type_names[81], { 8 * 3 * 2, 8 * 4, false, "", 0 } }, // f64mat3x2: 3 columns of f64vec2, align=32, size=48
    { glsl_builtin_type_names[82], { 8 * 3 * 3, 8 * 4, false, "", 0 } }, // f64mat3x3: 3 columns of f64vec3, align=32, size=72
    { glsl_builtin_type_names[83], { 8 * 3 * 4, 8 * 4, false, "", 0 } }, // f64mat3x4: 3 columns of f64vec4, align=32, size=96
    { glsl_builtin_type_names[84], { 8 * 4 * 2, 8 * 4, false, "", 0 } }, // f64mat4x2: 4 columns of f64vec2, align=32, size=64
    { glsl_builtin_type_names[85], { 8 * 4 * 3, 8 * 4, false, "", 0 } }, // f64mat4x3: 4 columns of f64vec3, align=32, size=96
    { glsl_builtin_type_names[86], { 8 * 4 * 4, 8 * 4, false, "", 0 } }, // f64mat4x4: 4 columns of f64vec4, align=32, size=128
    
    // int64_t types (8 bytes)
    { glsl_builtin_type_names[87], { 8, 8, false, "", 0 } }, // int64_t
    { glsl_builtin_type_names[88], { 8 * 2, 8 * 2, false, "", 0 } }, // i64vec2
    { glsl_builtin_type_names[89], { 8 * 3, 8 * 4, false, "", 0 } }, // i64vec3
    { glsl_builtin_type_names[90], { 8 * 4, 8 * 4, false, "", 0 } }, // i64vec4
    { glsl_builtin_type_names[91], { 8, 8, false, "", 0 } }, // uint64_t
    { glsl_builtin_type_names[92], { 8 * 2, 8 * 2, false, "", 0 } }, // u64vec2
    { glsl_builtin_type_names[93], { 8 * 3, 8 * 4, false, "", 0 } }, // u64vec3
    { glsl_builtin_type_names[94], { 8 * 4, 8 * 4, false, "", 0 } }, // u64vec4
    
    // int32_t types (4 bytes, same as int)
    { glsl_builtin_type_names[95], { 4, 4, false, "", 0 } }, // int32_t
    { glsl_builtin_type_names[96], { 4 * 2, 4 * 2, false, "", 0 } }, // i32vec2
    { glsl_builtin_type_names[97], { 4 * 3, 4 * 4, false, "", 0 } }, // i32vec3
    { glsl_builtin_type_names[98], { 4 * 4, 4 * 4, false, "", 0 } }, // i32vec4
    { glsl_builtin_type_names[99], { 4, 4, false, "", 0 } }, // uint32_t
    { glsl_builtin_type_names[100], { 4 * 2, 4 * 2, false, "", 0 } }, // u32vec2
    { glsl_builtin_type_names[101], { 4 * 3, 4 * 4, false, "", 0 } }, // u32vec3
    { glsl_builtin_type_names[102], { 4 * 4, 4 * 4, false, "", 0 } }, // u32vec4
    
    // int16_t types (2 bytes)
    { glsl_builtin_type_names[103], { 2, 2, false, "", 0 } }, // int16_t
    { glsl_builtin_type_names[104], { 2 * 2, 2 * 2, false, "", 0 } }, // i16vec2
    { glsl_builtin_type_names[105], { 2 * 3, 2 * 4, false, "", 0 } }, // i16vec3
    { glsl_builtin_type_names[106], { 2 * 4, 2 * 4, false, "", 0 } }, // i16vec4
    { glsl_builtin_type_names[107], { 2, 2, false, "", 0 } }, // uint16_t
    { glsl_builtin_type_names[108], { 2 * 2, 2 * 2, false, "", 0 } }, // u16vec2
    { glsl_builtin_type_names[109], { 2 * 3, 2 * 4, false, "", 0 } }, // u16vec3
    { glsl_builtin_type_names[110], { 2 * 4, 2 * 4, false, "", 0 } }, // u16vec4
    
    // int8_t types (1 byte)
    { glsl_builtin_type_names[111], { 1, 1, false, "", 0 } }, // int8_t
    { glsl_builtin_type_names[112], { 1 * 2, 1 * 2, false, "", 0 } }, // i8vec2
    { glsl_builtin_type_names[113], { 1 * 3, 1 * 4, false, "", 0 } }, // i8vec3
    { glsl_builtin_type_names[114], { 1 * 4, 1 * 4, false, "", 0 } }, // i8vec4
    { glsl_builtin_type_names[115], { 1, 1, false, "", 0 } }, // uint8_t
    { glsl_builtin_type_names[116], { 1 * 2, 1 * 2, false, "", 0 } }, // u8vec2
    { glsl_builtin_type_names[117], { 1 * 3, 1 * 4, false, "", 0 } }, // u8vec3
    { glsl_builtin_type_names[118], { 1 * 4, 1 * 4, false, "", 0 } }, // u8vec4

};

namespace st
{
    bool IsArrayType(std::string_view type) noexcept
    {
        size_t pos = type.find('[');
        return pos != std::string_view::npos && type.find(']', pos) != std::string_view::npos;
    }

    std::optional<GLSLTypeAttributes> GetGLSLTypeAttributes(std::string_view type) noexcept
    {
        if (!IsArrayType(type))
        {
            auto it = glsl_builtin_type_attributes.find(type);
            if (it != glsl_builtin_type_attributes.end())
            {
                return it->second;
            }
        }
        else
        {
            // Extract underlying type by getting everything before the first '['
            size_t pos = type.find('[');
            std::string_view base_type = type.substr(0, pos);
            auto it = glsl_builtin_type_attributes.find(base_type);
            if (it != glsl_builtin_type_attributes.end())
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
                        attrs.BaseTypeName = std::string(base_type);
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