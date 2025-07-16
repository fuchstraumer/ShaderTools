#pragma once
#ifndef SHADERTOOLS_FRAGMENT_GENERATOR_BASE_HPP
#define SHADERTOOLS_FRAGMENT_GENERATOR_BASE_HPP
#include "common/ShaderStage.hpp"
#include "common/ShaderToolsErrors.hpp"
#include "../../../common/impl/SessionImpl.hpp"
#include <string>
#include <string_view>
#include <filesystem>
#include <set>

namespace st
{

    /**
     * @brief Enum that describes the "type" of fragment being generated. 
     * This is mostly used to let us write multiple fragments of the same "type" to the eventual output source, and potentially do things
     * slight out of "order", but handle final ordering during final source generation. We do this by just iterating through the stored
     * map of fragments and writing them out in order (order defined by the enum).
     */
    enum class FragmentType : uint8_t
    {
        /** Required preamble for generating valid GLSL code - mostly just a version declaration */
        Preamble = 0,
        /** Extension enabling strings, e.g. #extension GL_EXT_shader_3d : require */
        Extension = 1,
        /** Include paths for external shader source files */
        IncludePath,
        /** Declaration of built-in gl_PerVertex interface block for vertex shaders */
        glPerVertex,
        /** Interface block for user-defined per-stage input/output attributes. Replaces Input/OutputAttribute, thus it's higher precedence here. */
        InterfaceBlock,
        /** Explicitly defined and individually listed input vertex attributes. @note Deprecated. */
        InputAttribute,
        /** Explicitly defined and individually listed output vertex attributes. @note Deprecated. */
        OutputAttribute,
        /** Vulkan specialization constants */
        SpecConstant,
        /** Push constants */
        PushConstantItem,
        /** Generated resource blocks and their accessors */
        ResourceBlock,
        /** User-defined ShaderMain function that we call from the final generated main() function. */
        ShaderMain,
        /** Required and finalized main() entrypoint, generated during final assembly. */
        Main,
        Invalid
    };

    /**
     * @brief A distinct block of code in the final shader source, with the fragment type attached so we can introspect on it and order them correctly.
     */
    struct ShaderFragment
    {
        ShaderFragment(FragmentType type, std::string data) noexcept : Type(type), Data(std::move(data)) {}
        ShaderFragment() noexcept = default;
        ~ShaderFragment() noexcept = default;
        ShaderFragment(const ShaderFragment&) noexcept = default;
        ShaderFragment& operator=(const ShaderFragment&) noexcept = default;
        ShaderFragment(ShaderFragment&&) noexcept = default;
        ShaderFragment& operator=(ShaderFragment&&) noexcept = default;

        FragmentType Type = FragmentType::Invalid;
        std::string Data;

        bool operator==(const ShaderFragment& other) const noexcept
        {
            return Type == other.Type;
        }

        bool operator<(const ShaderFragment& other) const noexcept
        {
            return Type < other.Type;
        }
    };

    /**
     * @brief Running state of the shader resources used so far, such as offsets and constant indices.
     */
    struct ShaderResourcesInfo
    {
        uint32_t LastConstantIndex = 0;
        uint32_t PushConstantOffset = 0;
        uint32_t LastInputIndex = 0;
        uint32_t LastOutputIndex = 0;
        uint32_t LastInputAttachmentIndex = 0;
        uint32_t NumAttributes = 0;
        uint32_t NumInstanceAttributes = 0;
        uint32_t LastSetIdx = 0;
    };

    struct ShaderGenerationContext
    {
        /**
         * @brief We take a copy of the input string as we will be removing parts of it as we process it. This helps make
         * the continued processing more efficient to use our regexes on, and generally makes things easier.
         * @todo Maybe consider using a string_view, if we can get remove_prefix to work with how we process the string?
         */
        std::string Input;
        /**
         * @brief We store the output fragments discretely, so that we can later process them and place them together as appropriate.
         * This also allows us to perform final validation and optimization on the fragments before we write them out to the final shader source.
         */
        std::multiset<ShaderFragment> Output;
        /** @brief We use `StageHandle` to query more information about the current shader we're processing as needed. */
        ShaderStage StageHandle;
        /** @brief We need to track constant indices and offset info when generating, this struct is how we do it. */
        ShaderResourcesInfo ResourcesInfo;
    };

    /** 
     * @brief Base interface class for generating shader fragments.
     * "Fragments" in the parlance of this library are just distinct blocks of shader source code, such as interface blocks and resource declarations,
     * include files, functions, extensions, etc. Each stage in the shader generation process can implement this interface to generate source code,
     * and then the shader generator class will call of these in order to generate the final shader source from the collected fragments.
     */
    class FragmentGeneratorBase
    {
    public:
        FragmentGeneratorBase(FragmentType generated_type, SessionImpl& session) noexcept : GeneratedType(generated_type), errorSession(session) {}
        FragmentGeneratorBase(const FragmentGeneratorBase&) = delete;
        FragmentGeneratorBase& operator=(const FragmentGeneratorBase&) = delete;

        virtual ~FragmentGeneratorBase() = default;

        virtual ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) = 0;

        static void SetBasePath(const std::filesystem::path& path)
        {
            BasePath = path;
        }

        [[nodiscard]] const std::filesystem::path& GetBasePath() const noexcept
        {
            return BasePath;
        }

        /** Not only do we sort the stored fragments, we also sort the stored generators so we run those in order when we can */
        bool operator<(const FragmentGeneratorBase& other) const noexcept
        {
            return GeneratedType < other.GeneratedType;
        }

    protected:

        static std::filesystem::path BasePath;

        FragmentType GeneratedType{ FragmentType::Invalid };
        SessionImpl& errorSession;
    };

}

#endif // !SHADERTOOLS_FRAGMENT_GENERATOR_BASE_HPP
