#include "SchemaParser.hpp"
#include <string>
#include <vector>

namespace st
{

    void SchemaParser::ParseStructureSchema(const std::string& name, const std::vector<std::string>& members)
    {
        ParsedStructureSchema schema;
        schema.Name = name;
        schema.BufferMembers = members;

        // Validate and finalize the schema
        ValidateAndFinalizeSchema(schema);

        // Store the finalized schema
        structureSchemas[name] = schema;
    }

    std::optional<ParsedStructureSchema> SchemaParser::GetStructureSchema(const std::string& name) const noexcept
    {
        auto it = structureSchemas.find(name);
        if (it != structureSchemas.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void SchemaParser::ValidateAndFinalizeSchema(ParsedStructureSchema& schema) const noexcept
    {
        

        // Placeholder for validation logic, e.g., checking member types, alignment, etc.
        // For now, we just set size and alignment to 0.
        schema.Size = 0; // Calculate actual size based on members
        schema.Alignment = 0; // Calculate actual alignment based on members
    }

}
