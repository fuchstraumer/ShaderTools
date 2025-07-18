#pragma once
#ifndef ST_SCHEMA_PARSER_HPP
#define ST_SCHEMA_PARSER_HPP
#include "common/CommonInclude.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace st
{

    /**
     * @ingroup Parser
     * @brief Parsed representation of a structure schema, which is used to define structured types in the shader.
     * This is the finalized representation of a parsed structure schema, which can be used by the shader generator to create the final shader source code. This
     * also includes some extra data that can be queried by frontends (eventually) if we want to display the structure in debug tooling.
     */
    struct ParsedStructureSchema
    {
        std::string Name; /** Name of the structure from the schema: not the name it is referred to by, that comes from the resource usage */
        /** Display string that we can give to the frontend for display in debug tooling: direct copy of parsed string with no reorg or changes for alignment. */
        std::string PrettyDisplayString;
        /** 
         * Finalized aligned and organized list of members, but not wrapped in any additional structure since we do that at generation time. 
         * Includes padding members at tail if needed. Layout of members is reorganized to optimize for alignment, e.g. a vec3 member will likely
         * have a 4 byte padding member either added or moved from other spots in the structure to ensure it's aligned to 16 bytes.
        */
        std::vector<std::string> BufferMembers;
        /** Total size of structure in bytes, including padding. */
        size_t Size{ 0u };
        /** Alignment of the structure in bytes, used to align the buffer reference in the shader. Not as important in bound case, which always aligns the same. */
        size_t Alignment{ 0u };
    };

    /**
     * @ingroup Parser
     * @brief Parses schema definitions from the YAML file, validating the contained types, the alignment, and total size. 
     * 
     * In order to enable bindless functionality, we want to move most types that would be stored in individual descriptors (like storage buffers or uniforms)
     * into buffer references, which the SPIR-V compiler can then use to interpret the data when we store everything as one big huge bucket of bytes that we access. 
     * In the cases of non-bindless setups, this will still work and does provide some use, but is much less useful since we'll just generate bindings and descriptors
     * for most of the individual resources anyway. In either case, this moves definition of schemas into a separate group in the YAML, leaving the resource groups
     * themselves to define just the resource *usages* in the shader rather than full definitions.
     */
    class ST_API SchemaParser
    {
    public:

        SchemaParser() = default;
        ~SchemaParser() = default;
        SchemaParser(const SchemaParser&) = delete;
        SchemaParser& operator=(const SchemaParser&) = delete;

        /** Parses a structure schema using the given name and list of members. */
        void ParseStructureSchema(const std::string& name, const std::vector<std::string>& members);

        /** Retrieves a structure schema with the given name, if it exists. */
        std::optional<ParsedStructureSchema> GetStructureSchema(const std::string& name) const noexcept;
        
    private:

        /** Map of structure schemas, keyed by name. */
        std::unordered_map<std::string, ParsedStructureSchema> structureSchemas;

        /** Helper function to validate the structure schema members and calculate size/alignment. */
        void ValidateAndFinalizeSchema(ParsedStructureSchema& schema) const noexcept;
    };

}

#endif // !ST_SCHEMA_PARSER_HPP