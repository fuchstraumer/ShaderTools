#pragma once
#ifndef ST_BUILT_IN_FRAGMENTS_HPP
#define ST_BUILT_IN_FRAGMENTS_HPP
#include <string>
namespace st {

    const char* VertexInterfaceStr = R"(
    #pragma VERT_INTERFACE_BEGIN
    in vec3 position;
    in vec3 normal;
    in vec3 tangent;
    in vec2 uv;
    out vec3 vPosition;
    out vec3 vNormal;
    out vec3 vTangent;
    out vec2 vUV;
    #pragma VERT_INTERFACE_END
    )";

    const char* FragmentInterfaceStr = R"(
    #pragma FRAG_INTERFACE_BEGIN
    in vec3 vPosition;
    in vec3 vNormal;
    in vec3 vTangent;
    in vec2 vUV;
    out vec4 backbuffer;
    #pragma FRAG_INTERFACE_END
    )";

    const char* preamble450 = R"(
    #version 450
    #extension GL_ARB_separate_shader_objects : enable
    #extension GL_ARB_shading_language_420pack : enable
    )";

    const char* PerVertexTriangles = R"(
    out gl_PerVertex {
        vec4 gl_Position;
    };
    )";

    const char* PerVertexPoints = R"(
    out gl_PerVertex {
        vec4 gl_Position;
        float gl_PointSize;
    };
    )";

    const char* SubgroupExtensionsBase = R"(
    #extension GL_KHR_shader_subgroup_ballot : enable
    #extension GL_KHR_shader_subgroup_arithmetic: enable
    )";

    const char* SubgroupExtensionsShuffle = R"(
    #extension GL_KHR_shader_subgroup_shuffle: enable
    #extension GL_KHR_shader_subgroup_shuffle_relative: enable
    )";

    const char* SubgroupExtensionsAdvanced = R"(
    #extension GL_KHR_shader_subgroup_clustered: enable
    #extension GL_KHR_shader_subgroup_quad: enable
    )";

    const char* SparseResidencyExtensions = R"(
    #extension GL_ARB_sparse_texture2 : enable
    #extension GL_ARB_sparse_texture_clamp : enable
    )";

}

#endif //!ST_BUILT_IN_FRAGMENTS_HPP
