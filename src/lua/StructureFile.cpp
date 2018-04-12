#include "lua/StructureFile.hpp"
#include <fstream>
#include <iostream>

namespace st {

    typedef unsigned int uint;

    static const std::unordered_map<std::string, size_t> glsl_object_sizes {
        { "vec4", sizeof(float) * 4 },
        { "vec3", sizeof(float) * 3 },
        { "vec2", sizeof(float) * 2 },
        { "float", sizeof(float) * 1 },
        { "dvec4", sizeof(double) * 4 },
        { "dvec3", sizeof(double) * 3 },
        { "dvec2", sizeof(double) * 2 },
        { "double", sizeof(double) * 1 },
        { "bvec4", sizeof(int) * 4 },
        { "bvec3", sizeof(int) * 3 },
        { "bvec2", sizeof(int) * 2 },
        { "bool", sizeof(int) * 1 },
        { "ivec4", sizeof(int) * 4 },
        { "ivec3", sizeof(int) * 3 },
        { "ivec2", sizeof(int) * 2 },
        { "int", sizeof(int) * 1 },
        { "uvec4", sizeof(uint) * 4 },
        { "uvec3", sizeof(uint) * 3 },
        { "uvec2", sizeof(uint) * 2 },
        { "uint", sizeof(uint) * 1},
        { "mat2", sizeof(float) * 4 },
        { "mat3", sizeof(float) * 9 },
        { "mat4", sizeof(float) * 16 }
    };

    void StructureFile::calculateSizes() {
        for (const auto& structure : structuresMap) {
            size_t total_size = 0;
            for (const auto& member : structure.second) {
                auto iter = glsl_object_sizes.find(member.Type);
                if (iter == glsl_object_sizes.cend()) {
                    auto struct_iter = structuresMap.find(member.Type);
                    if (struct_iter == structuresMap.cend()) {

                    }
                    else {
                        
                    }
                }
                else {
                    total_size += iter->second;
                }
            }
            structureSizes.emplace(structure.first, total_size);
        }
    }

}