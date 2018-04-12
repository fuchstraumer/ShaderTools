#pragma once
#ifndef ST_STRUCTURE_FILE_HPP
#define ST_STRUCTURE_FILE_HPP
#include "common/CommonInclude.hpp"
#include <unordered_map>
#include <unordered_set>
namespace st {

    class StructureFile {
    public:

        StructureFile(const char* fname);
        ~StructureFile();

        size_t GetStructureSize(const std::string& str) const noexcept;
        std::string GetStructureGLSL(const std::string& str) const;
        std::string GetFullGLSL() const;

        struct struct_member_t {
            std::string Name;
            std::string Type;
            bool operator==(const struct_member_t& other) const noexcept;
        };

    private: 
        
        void parse(std::string file_contents);
        void calculateSizes();
        
        std::unordered_map<std::string, size_t> structureSizes;
        std::unordered_map<std::string, std::unordered_set<struct_member_t>> structuresMap;
    };

}

namespace std {
    template<>
    struct hash<st::StructureFile::struct_member_t> {
        size_t operator()(const st::StructureFile::struct_member_t& obj) {
            return std::hash<std::string>()(obj.Name) ^ std::hash<std::string>()(obj.Type);
        }
    };
}

#endif //!ST_STRUCTURE_FILE_HPP