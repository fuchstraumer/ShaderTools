#pragma once
#ifndef ST_SHADER_GENERATOR_IMPL_HPP
#define ST_SHADER_GENERATOR_IMPL_HPP
#include "common/ShaderStage.hpp"
#include "common/stSession.hpp"
#include "resources/ShaderResource.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <string>
#include <filesystem>
#include <vector>
#include <set>
#include <map>

namespace st
{

    namespace fs = std::filesystem;

    struct yamlFile;

    enum class fragment_type : uint8_t
    {
        Preamble = 0,
        Extension = 1,
        glPerVertex = 2,
        InterfaceBlock = 3,
        InputAttribute,
        OutputAttribute,
        SpecConstant,
        PushConstantItem,
        IncludedFragment,
        ResourceBlock,
        Main,
        Invalid
    };

    // A distinct block of code in the final shader source
    struct shaderFragment
    {
        shaderFragment(fragment_type type, std::string data) noexcept : Type(type), Data(std::move(data)) {}
        shaderFragment() = default;
        shaderFragment(const shaderFragment&) = default;
        shaderFragment& operator=(const shaderFragment&) = default;
        fragment_type Type = fragment_type::Invalid;
        std::string Data;
        bool operator==(const shaderFragment& other) const noexcept
        {
            return Type == other.Type;
        }
        bool operator<(const shaderFragment& other) const noexcept
        {
            return Type < other.Type;
        }
    };

    struct shader_resources_t
    {
        size_t LastConstantIndex = 0;
        size_t PushConstantOffset = 0;
        size_t LastInputIndex = 0;
        size_t LastOutputIndex = 0;
        size_t LastInputAttachmentIndex = 0;
        size_t NumAttributes = 0;
        size_t NumInstanceAttributes = 0;
        size_t LastSetIdx = 0;
    };

    class ShaderGeneratorImpl
    {
    public:

        explicit ShaderGeneratorImpl(ShaderStage _stage, Session& errorSession);  
        ~ShaderGeneratorImpl();

        ShaderGeneratorImpl(ShaderGeneratorImpl&& other) noexcept;
        ShaderGeneratorImpl& operator=(ShaderGeneratorImpl&& other) noexcept;

        bool addFragment(const fs::path& path_to_source, std::string& addedFragment);
        const std::string& getFullSource() const;
        void addPerVertex();
        void addIncludePath(const char* include_path);
        void addPreamble(const fs::path& str);
        bool parseInterfaceBlock(const std::string& str);
        void parseConstantBlock(const std::string& str);
        ShaderToolsErrorCode parseInclude(const std::string& str, bool local);
        
        bool getResourceQualifiers(const ShaderResource& rsrc, std::string& result) const;
        std::string getResourcePrefix(size_t active_set, const std::string& image_format, const ShaderResource& rsrc) const;
        std::string getBufferMembersString(const ShaderResource & resource) const;
        std::string generateBufferAccessorString(const std::string& type_name, const std::string& buffer_name) const;
        std::string getBufferDescriptorString(const size_t& active_set, const ShaderResource& buffer, const std::string& name, const bool isArray, const bool isStorage) const;
        std::string getStorageTexelBufferString(const size_t& active_set, const ShaderResource& buffer, const std::string & name) const;
        std::string getUniformTexelBufferString(const size_t& active_set, const ShaderResource& texel_buffer, const std::string & name) const;
        std::string getImageTypeSuffix(const VkImageCreateInfo& info) const;
        std::string getStorageImageString(const size_t& active_set, const ShaderResource& storage_image, const std::string& name) const;
        std::string getSamplerString(const size_t& active_set, const ShaderResource& sampler, const std::string& name) const;
        std::string getSampledImageString(const size_t& active_set, const ShaderResource& sampled_image, const std::string& name) const;
        std::string getCombinedImageSamplerString(const size_t& active_set, const ShaderResource& combined_image_sampler, const std::string& name) const;
        std::string getInputAttachmentString(const size_t& active_set, const ShaderResource& input_attachment, const std::string& name) const;
        ShaderToolsErrorCode useResourceBlock(const std::string& block_name);

        std::string fetchBodyStr(const ShaderStage& handle, const std::string& path_to_source);
        void checkInterfaceOverrides(std::string& body_src_str);
        void addExtension(const std::string& extension_str);
        void processBodyStrIncludes(std::string & body_src_str);
        void processBodyStrSpecializationConstants(std::string& body_src_str);
        void processBodyStrResourceBlocks(const ShaderStage& handle, std::string& body_str);
        ShaderToolsErrorCode generate(const ShaderStage& handle, const std::string& path_to_src, const size_t num_extensions, const char* const* extensions);

        ShaderStage Stage{ 0u, 0u };
        std::multiset<shaderFragment> fragments;
        static std::map<fs::path, std::string> fileContents;
        std::map<std::string, std::string> resourceBlocks;
        mutable shader_resources_t ShaderResources;
        yamlFile* resourceFile;
        std::vector<fs::path> includes;
        Session& errorSession;
        
        bool constructionSuccessful = false;


        inline static std::string BasePath{ "../fragments/" };
        inline static std::string LibPath{ "../fragments/include" };

    };

}

#endif // !ST_SHADER_GENERATOR_IMPL_HPP
