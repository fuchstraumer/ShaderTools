#pragma once
#ifndef ST_SHADER_PACK_HPP
#define ST_SHADER_PACK_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "common/stSession.hpp"

namespace st
{

    class ShaderPackImpl;
    class Shader;
    class ShaderResource;
    class ResourceGroup;

    class ST_API ShaderPack
    {
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;
    public:

        ShaderPack(const char* shader_pack_yaml_path, Session& session);
        ~ShaderPack();

        const Shader* GetShaderGroup(const char* name) const;
        dll_retrieved_strings_t GetShaderGroupNames() const;
        dll_retrieved_strings_t GetResourceGroupNames() const;
        const descriptor_type_counts_t& GetTotalDescriptorTypeCounts() const;

        const ResourceGroup* GetResourceGroup(const char* name) const;
        // Avoid using this, has to do a fair bit of searching.
        const ShaderResource* GetResource(const char* rsrc_name) const;

    private:
        friend struct ShaderFileTracker;
        friend struct ShaderPackBinary;
        std::unique_ptr<ShaderPackImpl> impl;
    };

}

#endif //ST_SHADER_PACK_HPP
