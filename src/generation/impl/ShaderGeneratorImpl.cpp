#include "ShaderGeneratorImpl.hpp"
#include "../../common/impl/SessionImpl.hpp"
#include "../../parser/yamlFile.hpp"
#include "../../util/FilesystemUtils.hpp"
#include "../../util/ResourceFormats.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>

namespace st
{

    static const std::regex vertex_interface("(#pragma VERT_INTERFACE_BEGIN\n)");
    static const std::regex end_vertex_interface("#pragma VERT_INTERFACE_END\n");
    static const std::regex fragment_interface("#pragma FRAG_INTERFACE_BEGIN\n");
    static const std::regex end_fragment_interface("#pragma FRAG_INTERFACE_END\n");
    static const std::regex interface_var_in("in\\s+\\S+\\s+\\S+;\n");
    static const std::regex interface_var_out("out\\s+\\S+\\s+\\S+;\n");
    static const std::regex interface_override("#pragma INTERFACE_OVERRIDE\n");
    static const std::regex end_interface_override("#pragma END_INTERFACE_OVERRIDE");
    static const std::regex fragment_shader_no_output("#pragma NO_FRAGMENT_OUTPUT");
    static const std::regex inline_resources_begin("#pragma BEGIN_INLINE_RESOURCES");
    static const std::regex inline_resources_end("#pragma END_INLINE_RESOURCES");
    static const std::regex use_set_resources("#pragma\\s+USE_RESOURCES\\s+(\\S+)\n");
    static const std::regex include_library("#include <(\\S+)>\n");
    static const std::regex include_local("#include \"(\\S+)\"\n");
    static const std::regex specialization_constant("SPC\\s+(const.*$)");

    ShaderGeneratorImpl::ShaderGeneratorImpl(ShaderStage _stage, SessionImpl* errorSession) noexcept : Stage(std::move(_stage)), errorSession(errorSession)
    {
        fs::path preamble(std::string(BasePath) + "/builtins/preamble450.glsl");
        ShaderToolsErrorCode errorCode;
        errorCode = addPreamble(preamble);
        if (errorCode != ShaderToolsErrorCode::Success)
        {
            constructionSuccessful = false;
            return;
        }

        if (Stage.stageBits == VK_SHADER_STAGE_VERTEX_BIT)
        {
            fs::path vertex_interface_base(std::string(BasePath) + "/builtins/vertexInterface.glsl");
            errorCode = addStageInterface(Stage.stageBits, vertex_interface_base);
            if (errorCode != ShaderToolsErrorCode::Success)
            {
                constructionSuccessful = false;
                return;
            }
            addPerVertex();
        }
        else if (Stage.stageBits == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            fs::path fragment_interface_base(std::string(BasePath) + "/builtins/fragmentInterface.glsl");
            errorCode = addStageInterface(Stage.stageBits, fragment_interface_base);
            if (errorCode != ShaderToolsErrorCode::Success)
            {
                constructionSuccessful = false;
                return;
            }
        }

        // Compute shaders will fallthrough to here
        constructionSuccessful = true;
    }

    ShaderGeneratorImpl::~ShaderGeneratorImpl() noexcept {}

    ShaderGeneratorImpl::ShaderGeneratorImpl(ShaderGeneratorImpl&& other) noexcept :
        Stage(std::move(other.Stage)),
        fragments(std::move(other.fragments)),
        resourceBlocks(std::move(other.resourceBlocks)),
        ShaderResources(std::move(other.ShaderResources)),
        includes(std::move(other.includes)),
        errorSession(other.errorSession),
        constructionSuccessful(other.constructionSuccessful),
        resourceFile(other.resourceFile)
    {}

    ShaderGeneratorImpl& ShaderGeneratorImpl::operator=(ShaderGeneratorImpl&& other) noexcept
    {
        Stage = std::move(other.Stage);
        fragments = std::move(other.fragments);
        resourceBlocks = std::move(other.resourceBlocks);
        ShaderResources = std::move(other.ShaderResources);
        includes = std::move(other.includes);
        errorSession = std::move(other.errorSession);
        constructionSuccessful = other.constructionSuccessful;
        resourceFile = other.resourceFile;
        return *this;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::addPreamble(const fs::path& path)
    {
        std::ifstream file_stream(path);
        std::source_location error_location = std::source_location::current();
        const std::string pathString = path.string();

        if (!file_stream.is_open())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToFindPreambleFile, pathString.c_str(), error_location);
            return ShaderToolsErrorCode::GeneratorUnableToFindPreambleFile;
        }

        std::string preamble{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };

        auto frag_iter = fragments.emplace(fragment_type::Preamble, preamble);
        error_location = std::source_location::current();
        if (frag_iter == fragments.end())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToAddPreambleToInstanceStorage, nullptr, error_location);
            return ShaderToolsErrorCode::GeneratorUnableToAddPreambleToInstanceStorage;
        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::addStageInterface(uint32_t stageBits, fs::path interfacePath)
    {
        std::string interfaceStr;

        fs::path source_file(fs::canonical(interfacePath));
        const std::string pathString = source_file.string();

        if (!fs::exists(source_file))
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::FilesystemPathDoesNotExist, pathString.c_str());
            return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
        }

        std::ifstream file_stream(source_file);
        if (!file_stream.is_open())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened, pathString.c_str());
            return ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened;
        }

        std::string file_content{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };
        if (!file_content.empty())
        {
            interfaceStr = file_content;
        }
        else
        {
            return ShaderToolsErrorCode::FilesystemFailedToReadValidFileStream;
        }

        ShaderToolsErrorCode interfaceParseError = parseInterfaceBlock(interfaceStr);
        return interfaceParseError;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::parseInterfaceBlock(const std::string& str)
    {
        std::smatch match;
        if (Stage.stageBits == VK_SHADER_STAGE_VERTEX_BIT)
        {
            std::regex_search(str, match, vertex_interface);
        }
        else if (Stage.stageBits == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            std::regex_search(str, match, fragment_interface);
        }

        if (match.size() == 0)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Generator,
                ShaderToolsErrorCode::GeneratorUnableToFindMatchingElementOfVertexInterfaceBlockNeededForCompletion,
                "Couldn't parse interface block, missing matching element needed to complete");

            return ShaderToolsErrorCode::GeneratorUnableToFindMatchingElementOfVertexInterfaceBlockNeededForCompletion;
        }

        std::string local_str{ str.cbegin() + match.length(), str.cend() };
        while (!local_str.empty())
        {
            if (std::regex_search(local_str, match, interface_var_in))
            {
                const std::string prefix("layout (location = " + std::to_string(ShaderResources.LastInputIndex++) + ") ");
                fragments.emplace(fragment_type::InputAttribute, std::string(prefix + match[0].str()));
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
            else if (std::regex_search(local_str, match, interface_var_out))
            {
                const std::string prefix("layout (location = " + std::to_string(ShaderResources.LastOutputIndex++) + ") ");
                fragments.emplace(fragment_type::OutputAttribute, std::string(prefix + match[0].str()));
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
            else
            {
                break;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

    void ShaderGeneratorImpl::addPerVertex()
    {
        static const std::string per_vertex
        {
            "out gl_PerVertex {\n    vec4 gl_Position;\n};\n\n"
        };
        fragments.emplace(fragment_type::glPerVertex, per_vertex);
    }

    std::string ShaderGeneratorImpl::getFullSource() const
    {
        static const std::string emptyStringResult{};

        ReadRequest readRequest{ ReadRequest::Type::FindFullSourceString, Stage };
        ReadRequestResult readResult = MakeFileTrackerReadRequest(readRequest);
        if (readResult.has_value())
        {
            return std::get<std::string>(*readResult);
        }
        else
        {
            std::stringstream sourceStringStream;
            for (auto& fragment : fragments)
            {
                sourceStringStream << fragment.Data;
            }

            std::string sourceString{ sourceStringStream.str() };
            WriteRequest writeRequest{ WriteRequest::Type::AddFullSourceString, Stage,  sourceString};
            ShaderToolsErrorCode writeResult = MakeFileTrackerWriteRequest(std::move(writeRequest));
            std::source_location error_location = std::source_location::current();
            if (writeResult != ShaderToolsErrorCode::Success)
            {
                errorSession->AddError(this, ShaderToolsErrorSource::Filesystem, ShaderToolsErrorCode::FileTrackerWriteRequestFailed, "Failed to emplace generated full source string", error_location);
            }

            return sourceString;
        }
    }

    void ShaderGeneratorImpl::addIncludePath(const char* include_path)
    {
        includes.push_back(fs::path{ include_path });
    }

    void ShaderGeneratorImpl::parseConstantBlock(const std::string& str)
    {
        std::smatch match;
        if (std::regex_search(str, match, specialization_constant))
        {
            std::string local_str{ str.cbegin() + match.length() + 1, str.cend() };
            while (!local_str.empty())
            {
                if (!std::regex_search(local_str, match, specialization_constant))
                {
                    break;
                }
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.emplace(fragment_type::SpecConstant, std::string(prefix + match[1].str()));
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
        }
    }

    // Unlike how we previously just pasted the whole include in, here all we do now is extract the path to the include and make sure
    // it's put as early in the generated file as possible (so it works!)
    ShaderToolsErrorCode ShaderGeneratorImpl::processBodyStrIncludePaths(std::string& body_src_str)
    {
        bool include_found = true;
        while (include_found)
        {
            std::smatch match;
            if (std::regex_search(body_src_str, match, include_local))
            {
                fragments.emplace(fragment_type::IncludePath, match[0].str());
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else if (std::regex_search(body_src_str, match, include_library))
            {
                fragments.emplace(fragment_type::IncludePath, match[0].str());
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else
            {
                include_found = false;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::getResourceQualifiers(const ShaderResource& rsrc, std::string& result) const
    {

        // Qualifiers applied across all usages of this resource
        size_t num_qualifiers = 0;
        rsrc.GetQualifiers(&num_qualifiers, nullptr);
        std::vector<glsl_qualifier> qualifiers(num_qualifiers);
        rsrc.GetQualifiers(&num_qualifiers, qualifiers.data());

        // Now we need to get the qualifiers used for the current stage being generated
        size_t offset = num_qualifiers;
        num_qualifiers = 0;
        rsrc.GetPerUsageQualifiers(Stage, &num_qualifiers, nullptr);
        if (num_qualifiers != 0)
        {
            qualifiers.resize(qualifiers.size() + num_qualifiers);
            rsrc.GetPerUsageQualifiers(Stage, &num_qualifiers, qualifiers.data() + offset);
        }

        for (const auto& qual : qualifiers)
        {
            switch (qual)
            {
            case glsl_qualifier::Coherent:
                result += " coherent";
                break;
            case glsl_qualifier::ReadOnly:
                result += " readonly";
                break;
            case glsl_qualifier::WriteOnly:
                result += " writeonly";
                break;
            case glsl_qualifier::Volatile:
                result += " volatile";
                break;
            case glsl_qualifier::Restrict:
                result += " restrict";
                break;
            default:
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Generator,
                    ShaderToolsErrorCode::GeneratorInvalidResourceQualifier,
                    "All resource qualifiers stripped from resource");
                return ShaderToolsErrorCode::GeneratorInvalidResourceQualifier;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

    std::string ShaderGeneratorImpl::getResourcePrefix(size_t active_set, const std::string& format_specifier, const ShaderResource& rsrc) const
    {

        std::string prefix
        {
            std::string("layout (set = ") +std::to_string(active_set) + std::string(", binding = ") + std::to_string(rsrc.BindingIndex())
        };

        if (!format_specifier.empty())
        {
            prefix += std::string(", " + format_specifier + ")");
        }
        else
        {
            prefix += std::string(")");
        }

        if (rsrc.HasQualifiers())
        {
            std::string qualifiers;
            ShaderToolsErrorCode errorCode = getResourceQualifiers(rsrc, qualifiers);
            if (errorCode == ShaderToolsErrorCode::Success)
            {
                prefix += qualifiers;
            }
        }

        prefix += " ";

        return prefix;
    }

    std::string ShaderGeneratorImpl::getBufferMembersString(const ShaderResource& resource) const
    {
        return resource.GetMembersStr();
    }

    std::string ShaderGeneratorImpl::generateBufferAccessorString(const std::string& type_name, const std::string& buffer_name) const
    {
        std::string result = type_name;
        result += " " + std::string("Get") + buffer_name + std::string("(const in int idx)\n{\n");
        result += std::string("    return ") + buffer_name + std::string(".Data[idx];\n}\n");
        return result;
    }

    std::string ShaderGeneratorImpl::getBufferDescriptorString(const size_t& active_set, const ShaderResource& buffer, const std::string& name, const bool isArray, const bool isStorage) const
    {
        const std::string descrTypeStr = isStorage ? std::string("buffer ") : std::string("uniform ");

        if (isArray)
        {
            // insert structure declaration then make array of them
            const std::string typeName = name + std::string("Type");
            const std::string structName = std::string("struct ") + typeName + std::string("\n{\n");
            const std::string membersStr = getBufferMembersString(buffer);
            const static std::string endOfBlock("\n};\n");

            std::string result{ structName + membersStr + endOfBlock };
            result += getResourcePrefix(active_set, "", buffer);
            result += descrTypeStr;
            result += std::string("_") + name + std::string("Declaration_\n{\n");
            result += std::string("    ") + typeName + std::string(" Data");

            // dimensionality/range of array is either given or unbounded, depending on this value
            std::string arrayDims;

            if (buffer.ArraySize() == std::numeric_limits<uint32_t>::max())
            {
                arrayDims = std::string("[]");
            }
            else
            {
                arrayDims = "[" + std::to_string(buffer.ArraySize()) + "]";
            }

            result += arrayDims + std::string(";\n} ") + name + std::string(";\n");

            result += generateBufferAccessorString(typeName, name);

            return result;
        }
        else
        {
            const std::string prefix = getResourcePrefix(active_set, "", buffer);
            const std::string alt_name = std::string("_") + name + std::string("_");
            std::string result = prefix + descrTypeStr + alt_name + std::string(" {\n");
            result += getBufferMembersString(buffer);
            result += std::string("} ") + name + std::string(";\n\n");
            return result;
        }
    }

    std::string ShaderGeneratorImpl::getStorageTexelBufferString(const size_t& active_set, const ShaderResource& image, const std::string& name) const
    {

        auto get_storage_texel_buffer_subtype = [&](const std::string& image_format)->std::string
        {
            if (image_format.back() == 'f')
            {
                return std::string("imageBuffer");
            }
            else
            {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui")
                {
                    return std::string("uimageBuffer");
                }
                else if (image_format.back() == 'i') {
                    return std::string("iimageBuffer");
                }
                else
                {
                    return std::string("imageBuffer");
                }
            }
        };

        const VkFormat& fmt = image.Format();
        const std::string format_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, format_string, image);
        const std::string buffer_type = get_storage_texel_buffer_subtype(format_string);
        return prefix + std::string{ "uniform " } +buffer_type + std::string{ " " } +name + std::string{ ";\n" };
    }

    std::string ShaderGeneratorImpl::getUniformTexelBufferString(const size_t& active_set, const ShaderResource& texel_buffer, const std::string& name) const
    {

        auto get_uniform_texel_buffer_subtype = [&](const std::string& image_format)->std::string
        {
            if (image_format.back() == 'f')
            {
                return std::string("textureBuffer");
            }
            else
            {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui")
                {
                    return std::string("utextureBuffer");
                }
                else if (image_format.back() == 'i')
                {
                    return std::string("itextureBuffer");
                }
                else
                {
                    return std::string("textureBuffer");
                }
            }
        };

        const VkFormat fmt = texel_buffer.Format();
        const std::string format_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, format_string, texel_buffer);
        const std::string buffer_type = get_uniform_texel_buffer_subtype(format_string);
        return prefix + std::string("uniform ") + buffer_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getImageTypeSuffix(const VkImageCreateInfo& info) const
    {
        std::string base_suffix;
        switch (info.imageType)
        {
        case VK_IMAGE_TYPE_1D:
            base_suffix = "1D";
            break;
        case VK_IMAGE_TYPE_2D:
            base_suffix = "2D";
            break;
        case VK_IMAGE_TYPE_3D:
            base_suffix = "3D";
            break;
        default:
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorInvalidImageType, nullptr);
            return std::string{};
        }

        if (info.flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT)
        {
            base_suffix += "Array";
        }
        else if (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
        {
            base_suffix += "CubeArray";
        }

        return base_suffix;
    }

    std::string ShaderGeneratorImpl::getStorageImageString(const size_t& active_set, const ShaderResource& storage_image, const std::string& name) const
    {

        auto get_image_subtype = [&](const std::string& image_format)->std::string
        {
            if (image_format.back() == 'f')
            {
                return std::string("image");
            }
            else
            {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui")
                {
                    return std::string("uimage");
                }
                else if (image_format.back() == 'i')
                {
                    return std::string("iimage");
                }
                else
                {
                    return std::string("image");
                }
            }
        };

        const VkFormat fmt = storage_image.Format();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, fmt_string, storage_image);
        const std::string resource_type = get_image_subtype(fmt_string) + storage_image.ImageSamplerSubtype();
        const std::string arrayName = storage_image.IsDescriptorArray() ? name + std::string("[]") : name;
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + arrayName + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSamplerString(const size_t& active_set, const ShaderResource& sampler, const std::string& name) const
    {
        const std::string prefix = getResourcePrefix(active_set, "", sampler);
        const std::string arrayName = sampler.IsDescriptorArray() ? name + std::string("[]") : name;
        return prefix + std::string("uniform sampler ") + arrayName + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSampledImageString(const size_t& active_set, const ShaderResource& sampled_image, const std::string& name) const
    {
        const std::string prefix = getResourcePrefix(active_set, "", sampled_image);
        std::string resource_type = std::string("texture");
        resource_type += sampled_image.ImageSamplerSubtype();
        std::string arrayString("[]");
        if (sampled_image.ArraySize() != std::numeric_limits<uint32_t>::max())
        {
            arrayString = "[" + std::to_string(sampled_image.ArraySize()) + "]";
        }
        const std::string arrayName = sampled_image.IsDescriptorArray() ? name + arrayString : name;
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + arrayName + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getCombinedImageSamplerString(const size_t& active_set, const ShaderResource& combined_image_sampler, const std::string& name) const
    {
        const std::string prefix = getResourcePrefix(active_set, "", combined_image_sampler);
        std::string resource_type = std::string("sampler");
        resource_type += combined_image_sampler.ImageSamplerSubtype();
        const std::string arrayName = combined_image_sampler.IsDescriptorArray() ? name + std::string("[]") : name;
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + arrayName + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getInputAttachmentString(const size_t& active_set, const ShaderResource& input_attachment, const std::string& name) const
    {
        auto get_input_attachment_specifier = [&]()->std::string
        {
            static const std::string base_str("input_attachment_index=");
            return base_str + std::to_string(input_attachment.InputAttachmentIndex()) + std::string(" ");
        };

        auto get_input_attachment_subtype = [&](const std::string& format_str)->std::string
        {
            std::string result;
            if (format_str.back() == 'f')
            {
                result = std::string("subpassInput");
            }
            else
            {
                std::string last_two = format_str.substr(format_str.size() - 2, 2);
                if (last_two == "ui")
                {
                    result = std::string("usubpassInput");
                }
                else if (format_str.back() == 'i')
                {
                    result = std::string("isubpassInput");
                }
                else
                {
                    result = std::string("subpassInput");
                }
            }

            return result;
        };

        const VkFormat fmt = input_attachment.Format();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, get_input_attachment_specifier(), input_attachment);
        const std::string resource_type = get_input_attachment_subtype(fmt_string);
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::useResourceBlock(const std::string & block_name)
    {

        fragments.emplace(fragment_type::ResourceBlock, std::string("// Resource block: ") + block_name + std::string("\n"));

        auto& resource_block = resourceFile->resourceGroups.at(block_name);
        std::string resource_block_string{ "" };

        const size_t active_set = ShaderResources.LastSetIdx;
        for (auto& resource : resource_block)
        {
            const std::string resource_name = resource.Name();
            auto& resource_item = resource;
            switch (resource_item.DescriptorType())
            {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                resource_block_string += getSamplerString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                resource_block_string += getSampledImageString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                resource_block_string += getCombinedImageSamplerString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                resource_block_string += getStorageImageString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                resource_block_string += getUniformTexelBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                resource_block_string += getStorageTexelBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                resource_block_string += getBufferDescriptorString(active_set, resource_item, resource_name, resource.IsDescriptorArray(), false);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                resource_block_string += getBufferDescriptorString(active_set, resource_item, resource_name, resource.IsDescriptorArray(), true);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                resource_block_string += getBufferDescriptorString(active_set, resource_item, resource_name, false, false);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                resource_block_string += getBufferDescriptorString(active_set, resource_item, resource_name, false, true);
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                resource_block_string += getInputAttachmentString(active_set, resource_item, resource_name);
                break;
            default:
                {
                    const std::string errorMessage = "Invalid descriptor type in resource block " + block_name + " for resource " + resource_name;
                    errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorInvalidDescriptorTypeInResourceBlock, errorMessage.c_str());
                }
                return ShaderToolsErrorCode::GeneratorInvalidDescriptorTypeInResourceBlock;
            }

        }

        resource_block_string += std::string{ "// End resource block\n\n" };

        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, resource_block_string });
        ++ShaderResources.LastSetIdx;

        return ShaderToolsErrorCode::Success;
    }

    std::string ShaderGeneratorImpl::fetchBodyStr(const ShaderStage& handle, const std::string& path_to_source)
    {
        ReadRequest readRequest{ ReadRequest::Type::FindShaderBody, handle };
        ReadRequestResult readResult = MakeFileTrackerReadRequest(readRequest);

        std::string body_str;

        if (!readResult.has_value())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorNoBodyStringInFileTrackerStorage, nullptr);

            WriteRequest writeRequest{ WriteRequest::Type::AddShaderBodyPath, handle, fs::path(path_to_source) };
            ShaderToolsErrorCode errorCode = MakeFileTrackerWriteRequest(std::move(writeRequest));

            if (!WasWriteRequestSuccessful(errorCode))
            {
                errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToAddShaderBodyPath, nullptr);
                return std::string{};
            }
            else
            {
                readResult = MakeFileTrackerReadRequest(readRequest);
                if (readResult.has_value())
                {
                    body_str = std::get<std::string>(*readResult);
                }
                else
                {
                    errorSession->AddError(this, ShaderToolsErrorSource::Generator, readResult.error(), "fetchBodyStr failed following second attempt after adding/updating shader body path");
                    return std::string{};
                }
            }
        }
        else
        {
            body_str = std::get<std::string>(*readResult);
        }

        if (body_str.empty())
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorFoundEmptyBodyString, path_to_source.c_str());
            return std::string{};
        }
        else
        {
            return body_str;
        }

    }

    void ShaderGeneratorImpl::checkInterfaceOverrides(std::string& body_src_str)
    {
        std::smatch match;
        if (std::regex_search(body_src_str, match, interface_override))
        {
            fragments.erase(shaderFragment{ fragment_type::InputAttribute, std::string() });
            fragments.erase(shaderFragment{ fragment_type::OutputAttribute, std::string() });
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            if (!std::regex_search(body_src_str, match, end_interface_override))
            {
                errorSession->AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToFindEndingOfInterfaceOverride, nullptr);
            }
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
        }

        if (std::regex_search(body_src_str, match, fragment_shader_no_output) && (Stage.stageBits == VK_SHADER_STAGE_FRAGMENT_BIT))
        {
            // shader doesn't write to color output, potentially only the backbuffer
            auto iter = std::begin(fragments);
            while (iter != std::end(fragments))
            {
                if (iter->Type == fragment_type::OutputAttribute)
                {
                    fragments.erase(iter++);
                }
                else
                {
                    ++iter;
                }
            }
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
        }
    }

    void ShaderGeneratorImpl::addExtension(const std::string& extension_str)
    {
        const std::string extension_begin("#extension ");
        std::string result = extension_begin + extension_str;
        const std::string extension_close(" : enable \n");
        result += extension_close;
        fragments.emplace(fragment_type::Extension, result);
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::processBodyStrSpecializationConstants(std::string& body_src_str)
    {
        bool spc_found = true;
        while (spc_found)
        {
            std::smatch match;
            if (std::regex_search(body_src_str, match, specialization_constant))
            {
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.emplace(fragment_type::SpecConstant, std::string(prefix + match[1].str() + "\n"));
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else
            {
                spc_found = false;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::processBodyStrResourceBlocks(const ShaderStage& handle, std::string& body_str)
    {
        bool block_found = true;

        std::vector<std::string> foundResourceBlocks;

        while (block_found)
        {
            std::smatch match;
            if (std::regex_search(body_str, match, use_set_resources))
            {
                ShaderToolsErrorCode rsrcErrorCode = useResourceBlock(match[1].str());
                if (rsrcErrorCode == ShaderToolsErrorCode::Success)
                {
                    foundResourceBlocks.emplace_back(match[1].str());
                }
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else
            {
                block_found = false;
            }
        }

        WriteRequest writeRequest{ WriteRequest::Type::AddUsedResourceBlocks, handle, std::move(foundResourceBlocks) };
        ShaderToolsErrorCode errorCode = MakeFileTrackerWriteRequest(std::move(writeRequest));

        return errorCode;
    }

    ShaderToolsErrorCode ShaderGeneratorImpl::generate(const ShaderStage& handle, const std::string& path_to_source, const size_t num_extensions, const char* const* extensions)
    {
        ShaderToolsErrorCode ec{ ShaderToolsErrorCode::Success };
        std::string body_str = fetchBodyStr(handle, path_to_source);
        if (body_str.empty())
        {
            return ShaderToolsErrorCode::GeneratorShaderBodyStringNotFound;
        }

        checkInterfaceOverrides(body_str);
        if ((num_extensions != 0) && extensions)
        {
            for (size_t i = 0; i < num_extensions; ++i)
            {
                addExtension(extensions[i]);
            }
        }

        ec = processBodyStrIncludePaths(body_str);
        if (ec != ShaderToolsErrorCode::Success)
        {
            return ec;
        }

        ec = processBodyStrSpecializationConstants(body_str);
        if (ec != ShaderToolsErrorCode::Success)
        {
            return ec;
        }

        ec = processBodyStrResourceBlocks(handle, body_str);
        if (ec != ShaderToolsErrorCode::Success)
        {
            return ec;
        }

        fragments.emplace(fragment_type::Main, body_str);

        return ec;
    }

}
