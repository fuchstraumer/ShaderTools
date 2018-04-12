#include "StructureFile.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

namespace st {

    StructureFile::StructureFile(const char* fname) {
        std::ifstream input_stream(fname);
        if (!input_stream.is_open()) {
            std::cerr << "Failed to open input structures JSON file.\n";
            throw std::runtime_error("Failed to open input Structure file!");
        }
        std::string source_file{ std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>() };
        parse(std::move(source_file));
    }

    size_t StructureFile::GetStructureSize(const std::string& str) const {
        auto iter = structureSizes.find(str);
        if (iter == structureSizes.cend()) {
            std::cerr << "Failed to find size of requested structure.";
            return std::numeric_limits<size_t>::max();
        }
        else {
            return iter->second;
        }
    }

    void StructureFile::parse(std::string src) {
        using namespace nlohmann;
        json j = src;

        for (auto iter = j.cbegin(); iter != j.cend(); ++iter) {
            const std::string struct_name = iter.key();
            structuresMap.emplace(struct_name, std::unordered_set<struct_member_t>());
            for (auto members_iter = iter->cbegin(); iter != iter->cend(); ++iter) {
                const std::string member_name = members_iter.key();
                structuresMap.at(struct_name).emplace(members_iter.key(), *members_iter);
            }
        }

        calculateSizes();
    }

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