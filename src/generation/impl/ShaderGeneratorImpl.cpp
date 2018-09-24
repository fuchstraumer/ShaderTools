#include "ShaderGeneratorImpl.hpp"
#include "../../lua/ResourceFile.hpp"
#include "../../util/FilesystemUtils.hpp"
#include "../../util/ResourceFormats.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include <regex>
#include "easyloggingpp/src/easylogging++.h"

namespace st {

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
    static const std::regex include_library("#include <(\\S+)>");
    static const std::regex include_local("#include \"(\\S+)\"");
    static const std::regex specialization_constant("SPC\\s+(const.*$)");

    std::map<fs::path, std::string> ShaderGeneratorImpl::fileContents = std::map<fs::path, std::string>{};

    ShaderGeneratorImpl::ShaderGeneratorImpl(const VkShaderStageFlagBits& stage) : Stage(stage) {
        fs::path preamble(std::string(BasePath) + "/builtins/preamble450.glsl");
        addPreamble(preamble);
        if (Stage == VK_SHADER_STAGE_VERTEX_BIT) {
            fs::path vertex_interface_base(std::string(BasePath) + "/builtins/vertexInterface.glsl");
            const auto& interface_str = addFragment(vertex_interface_base);
            parseInterfaceBlock(interface_str);
            addPerVertex();
        }
        else if (Stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            fs::path fragment_interface_base(std::string(BasePath) + "/builtins/fragmentInterface.glsl");
            const auto& interface_str = addFragment(fragment_interface_base);
            parseInterfaceBlock(interface_str);
        }

    }

    ShaderGeneratorImpl::~ShaderGeneratorImpl() { }

    ShaderGeneratorImpl::ShaderGeneratorImpl(ShaderGeneratorImpl&& other) noexcept : Stage(std::move(other.Stage)), fragments(std::move(other.fragments)),
        resourceBlocks(std::move(other.resourceBlocks)), ShaderResources(std::move(other.ShaderResources)), includes(std::move(other.includes)) {}

    ShaderGeneratorImpl& ShaderGeneratorImpl::operator=(ShaderGeneratorImpl&& other) noexcept {
        Stage = std::move(other.Stage);
        fragments = std::move(other.fragments);
        resourceBlocks = std::move(other.resourceBlocks);
        ShaderResources = std::move(other.ShaderResources);
        includes = std::move(other.includes);
        return *this;
    }

    void ShaderGeneratorImpl::addPreamble(const fs::path& path) {
        if (fileContents.count(fs::absolute(path)) == 0) {
            std::ifstream file_stream(path);
            if (!file_stream.is_open()) {
                LOG(ERROR) << "Couldn't find shader preamble file. Workding directory is probably set incorrectly!";
                throw std::runtime_error("Failed to open preamble file at given path.");
            }
            std::string preamble{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };

            auto fc_iter = fileContents.emplace(fs::absolute(path), preamble);
            if (!fc_iter.second) {
                LOG(WARNING) << "Failed to emplace loaded source string into fileContents map.";
            }

            auto frag_iter = fragments.emplace(shaderFragment{ fragment_type::Preamble, preamble });
            if (frag_iter == fragments.end()) {
                LOG(ERROR) << "Couldn't add premamble to generator's fragments container! Generation cannot proceed without this component.";
                throw std::runtime_error("Failed to add preamble to generator's fragments multimap!");
            }

        }
        else {
            auto frag_iter = fragments.emplace(shaderFragment{ fragment_type::Preamble, fileContents.at(fs::absolute(path)) });
            if (frag_iter == fragments.end()) {
                throw std::runtime_error("Failed to add preamble to generator's fragments multimap!");
            }
        }
    }

    const std::string& ShaderGeneratorImpl::addFragment(const fs::path& src_path) {
        fs::path source_file(fs::absolute(src_path));

        if (!fs::exists(source_file)) {
            const std::string exception_str = std::string("Given shader fragment path \"") + source_file.string() + std::string("\" does not exist!");
            LOG(ERROR) << exception_str;
            throw std::runtime_error(exception_str);
        }

        std::ifstream file_stream(source_file);

        std::string file_content{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };
        fileContents.emplace(source_file, std::move(file_content));
        return fileContents.at(source_file);
    }

    void ShaderGeneratorImpl::parseInterfaceBlock(const std::string& str) {
        std::smatch match;
        if (Stage == VK_SHADER_STAGE_VERTEX_BIT) {
            std::regex_search(str, match, vertex_interface);
        }
        else if (Stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            std::regex_search(str, match, fragment_interface);
        }

        if (match.size() == 0) {
            LOG(ERROR) << "Could not find match for interface block.";
            throw std::runtime_error("Failed to find any matches.");
        }

        std::string local_str{ str.cbegin() + match.length(), str.cend() };
        while (!local_str.empty()) {
            if (std::regex_search(local_str, match, interface_var_in)) {
                const std::string prefix("layout (location = " + std::to_string(ShaderResources.LastInputIndex++) + ") ");
                fragments.insert(shaderFragment{ fragment_type::InputAttribute, std::string(prefix + match[0].str()) });
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
            else if (std::regex_search(local_str, match, interface_var_out)) {
                const std::string prefix("layout (location = " + std::to_string(ShaderResources.LastOutputIndex++) + ") ");
                fragments.insert(shaderFragment{ fragment_type::OutputAttribute, std::string(prefix + match[0].str()) });
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
            else {
                break;
            }
        }
    }

    void ShaderGeneratorImpl::addPerVertex() {
        static const std::string per_vertex{
            "out gl_PerVertex {\n    vec4 gl_Position;\n};\n\n"
        };
        fragments.insert(shaderFragment{ fragment_type::glPerVertex, per_vertex });
    }

    std::string ShaderGeneratorImpl::getFullSource() const {
        std::stringstream result_stream;
        for (auto& fragment : fragments) {
            result_stream << fragment.Data;
        }
        return result_stream.str();
    }

    void ShaderGeneratorImpl::addIncludePath(const char * include_path) {
        includes.push_back(fs::path{ include_path });
    }


    void ShaderGeneratorImpl::parseConstantBlock(const std::string& str) {
        std::smatch match;
        if (std::regex_search(str, match, specialization_constant)) {
            std::string local_str{ str.cbegin() + match.length() + 1, str.cend() };
            while (!local_str.empty()) {
                if (!std::regex_search(local_str, match, specialization_constant)) {
                    break;
                }
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.insert(shaderFragment{ fragment_type::SpecConstant, std::string(prefix + match[1].str()) });
                local_str.erase(local_str.cbegin(), local_str.cbegin() + match[0].length());
            }
        }
    }

    void ShaderGeneratorImpl::parseInclude(const std::string & str, bool local) {

        fs::path include_path;
        if (!local) {
            // Include from our "library"
            include_path = fs::path(std::string(LibPath + str));

            if (!fs::exists(include_path)) {
                LOG(ERROR) << "Couldn't find specified include " << include_path.string() << " in library includes.";
                throw std::runtime_error("Failed to find desired include in library includes.");
            }
        }
        else {
            bool found = false;
            for (auto& path : includes) {
                include_path = path / fs::path(str);
                if (fs::exists(include_path)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                LOG(ERROR) << "Could not find desired include " << include_path.string() << " in shader-local include paths.";
                throw std::runtime_error("Failed to find desired include in local include paths.");
            }
        }

        if (fileContents.count(include_path)) {
            fragments.emplace(shaderFragment{ fragment_type::IncludedFragment, fileContents.at(include_path) });
            return;
        }

        std::ifstream include_file(include_path);

        if (!include_file.is_open()) {
            LOG(ERROR) << "Include file path was valid, but failed to open input file stream!";
            throw std::runtime_error("Failed to open include file, despite having a valid path!");
        }

        std::string file_content{ std::istreambuf_iterator<char>(include_file), std::istreambuf_iterator<char>() };
        fileContents.emplace(include_path, file_content);
        fragments.emplace(shaderFragment{ fragment_type::IncludedFragment, file_content });

    }

    std::string ShaderGeneratorImpl::getResourceQualifiers(const ShaderResource& rsrc) const {
        size_t num_qualifiers = 0;
        rsrc.GetQualifiers(&num_qualifiers, nullptr);
        std::vector<glsl_qualifier> qualifiers(num_qualifiers);
        rsrc.GetQualifiers(&num_qualifiers, qualifiers.data());

        size_t offset = num_qualifiers;
        num_qualifiers = 0;
        rsrc.GetPerUsageQualifiers(currentShaderName.c_str(), &num_qualifiers, nullptr);
        if (num_qualifiers != 0) {
            qualifiers.resize(qualifiers.size() + num_qualifiers);
            rsrc.GetPerUsageQualifiers(currentShaderName.c_str(), &num_qualifiers, qualifiers.data() + offset);
        }

        std::string result;
        for (const auto& qual : qualifiers) {
            if (qual == glsl_qualifier::Coherent) {
                result += " coherent";
            }
            else if (qual == glsl_qualifier::ReadOnly) {
                result += " readonly";
            }
            else if (qual == glsl_qualifier::WriteOnly) {
                result += " writeonly";
            }
            else if (qual == glsl_qualifier::Volatile) {
                result += " volatile";
            }
            else if (qual == glsl_qualifier::Restrict) {
                result += " restrict";
            }
            else if (qual == glsl_qualifier::InvalidQualifier) {
                LOG(WARNING) << "ShaderResource had an invalid qualifier value!";
            }
        }

        return result;
    }

    std::string ShaderGeneratorImpl::getResourcePrefix(size_t active_set, const std::string& format_specifier, const ShaderResource& rsrc) const {

        std::string prefix{
            std::string{ "layout (set = " } +std::to_string(active_set) + std::string{ ", binding = " } +std::to_string(rsrc.BindingIndex())
        };

        if (!format_specifier.empty()) {
            prefix += std::string(", " + format_specifier + ")");
        }
        else {
            prefix += std::string(")");
        }

        if (rsrc.HasQualifiers()) {
            prefix += getResourceQualifiers(rsrc);
        }

        prefix += " ";

        return prefix;
    }

    std::string ShaderGeneratorImpl::getBufferMembersString(const ShaderResource& resource) const {
        std::string result;

        size_t num_members = 0;
        resource.GetMembers(&num_members, nullptr);
        std::vector<ShaderResourceSubObject> subobjects(num_members);
        resource.GetMembers(&num_members, subobjects.data());

        for (const auto& member : subobjects) {
            result += std::string{ "    " };
            result += member.Type;
            result += std::string(" ") + member.Name;
            result += std::string{ ";\n" };
        }

        return result;
    }

    std::string ShaderGeneratorImpl::getUniformBufferString(const size_t& active_set, const ShaderResource& buffer, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "", buffer);
        const std::string alt_name = std::string{ "_" } +name + std::string{ "_" };
        std::string result = prefix + std::string{ "uniform " } +alt_name + std::string{ " {\n" };
        result += getBufferMembersString(buffer);
        result += std::string{ "} " } +name + std::string{ ";\n\n" };
        return result;
    }

    std::string ShaderGeneratorImpl::getStorageBufferString(const size_t& active_set, const ShaderResource& buffer, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "std430", buffer);
        const std::string alt_name = std::string{ "buffer " } +std::string{ "_" } +name + std::string{ "_" };
        std::string result = prefix + alt_name + std::string{ " {\n" };
        result += getBufferMembersString(buffer);
        result += std::string{ "} " } +name + std::string{ ";\n\n" };
        return result;
    }

    std::string ShaderGeneratorImpl::getStorageTexelBufferString(const size_t& active_set, const ShaderResource& image, const std::string& name) const {

        auto get_storage_texel_buffer_subtype = [&](const std::string& image_format)->std::string {
            if (image_format.back() == 'f') {
                return std::string("imageBuffer");
            }
            else {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui") {
                    return std::string("uimageBuffer");
                }
                else if (image_format.back() == 'i') {
                    return std::string("iimageBuffer");
                }
                else {
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

    std::string ShaderGeneratorImpl::getUniformTexelBufferString(const size_t& active_set, const ShaderResource& texel_buffer, const std::string& name) const {
        auto get_uniform_texel_buffer_subtype = [&](const std::string& image_format)->std::string {
            if (image_format.back() == 'f') {
                return std::string("textureBuffer");
            }
            else {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui") {
                    return std::string("utextureBuffer");
                }
                else if (image_format.back() == 'i') {
                    return std::string("itextureBuffer");
                }
                else {
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

    std::string ShaderGeneratorImpl::getImageTypeSuffix(const VkImageCreateInfo& info) const {
        std::string base_suffix;
        switch (info.imageType) {
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
            LOG(ERROR) << "Encountered invalid image type.";
            throw std::domain_error("Invalid/not set ImageType value");
        }

        if (info.flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
            base_suffix += "Array";
        }
        else if (info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
            base_suffix += "CubeArray";
        }

        return base_suffix;
    }

    std::string ShaderGeneratorImpl::getStorageImageString(const size_t& active_set, const ShaderResource& storage_image, const std::string& name) const {
        auto get_image_subtype = [&](const std::string& image_format)->std::string {
            if (image_format.back() == 'f') {
                return std::string("image");
            }
            else {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui") {
                    return std::string("uimage");
                }
                else if (image_format.back() == 'i') {
                    return std::string("iimage");
                }
                else {
                    return std::string("image");
                }
            }
        };

        const VkFormat fmt = storage_image.Format();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, fmt_string, storage_image);
        const std::string resource_type = get_image_subtype(fmt_string) + getImageTypeSuffix(storage_image.ImageInfo());
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSamplerString(const size_t& active_set, const ShaderResource& sampler, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "", sampler);
        return prefix + std::string("uniform sampler ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSampledImageString(const size_t& active_set, const ShaderResource& sampled_image, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "", sampled_image);
        std::string resource_type = std::string("texture");
        if (!sampled_image.FromFile()) {
            resource_type += getImageTypeSuffix(sampled_image.ImageInfo());
        }
        else {
            resource_type += "2D";
        }
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getCombinedImageSamplerString(const size_t& active_set, const ShaderResource& combined_image_sampler, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "", combined_image_sampler);
        std::string resource_type = std::string("sampler");
        if (!combined_image_sampler.FromFile()) {
            resource_type += getImageTypeSuffix(combined_image_sampler.ImageInfo());
        }
        else {
            resource_type += "2D";
        }
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getInputAttachmentString(const size_t& active_set, const ShaderResource& input_attachment, const std::string& name) const {
        auto get_input_attachment_specifier = [&]()->std::string {
            static const std::string base_str("input_attachment_index=");
            return base_str + std::to_string(input_attachment.InputAttachmentIndex()) + std::string(" ");
        };

        auto get_input_attachment_subtype = [&](const std::string& format_str)->std::string {
            std::string result;
            if (format_str.back() == 'f') {
                result = std::string("subpassInput");
            }
            else {
                std::string last_two = format_str.substr(format_str.size() - 2, 2);
                if (last_two == "ui") {
                    result = std::string("usubpassInput");
                }
                else if (format_str.back() == 'i') {
                    result = std::string("isubpassInput");
                }
                else {
                    result = std::string("subpassInput");
                }
            }

            const VkImageCreateInfo info = input_attachment.ImageInfo();
            if (info.samples != VK_SAMPLE_COUNT_1_BIT) {
                result += std::string("MS"); // add multisampled flag.
            }

            return result;
        };

        const VkFormat fmt = input_attachment.Format();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, get_input_attachment_specifier(), input_attachment);
        const std::string resource_type = get_input_attachment_subtype(fmt_string);
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

#ifndef NDEBUG
    constexpr bool SAVE_BLOCKS_TO_FILE = false;
#else
    constexpr bool SAVE_BLOCKS_TO_FILE = false;
#endif

    void ShaderGeneratorImpl::useResourceBlock(const std::string & block_name) {
        size_t active_set = ShaderResources.LastSetIdx;


        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, std::string("// Resource block: ") + block_name + std::string("\n") });

        auto& resource_block = luaResources->GetResources(block_name);
        std::string resource_block_string{ "" };

        for (auto& resource : resource_block) {
            const std::string resource_name = resource.Name();
            auto& resource_item = resource;
            switch (resource_item.DescriptorType()) {
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
                resource_block_string += getUniformBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                resource_block_string += getStorageBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                resource_block_string += getUniformBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                resource_block_string += getStorageBufferString(active_set, resource_item, resource_name);
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                resource_block_string += getInputAttachmentString(active_set, resource_item, resource_name);
                break;
            default:
                LOG(ERROR) << "Unsupported VkDescriptorType encountered when generating resources for resource block in ShaderGenerator!";
                throw std::domain_error("Unsupported VkDescriptorType encountered in ShaderGenerator!");
            }

        }

        resource_block_string += std::string{ "// End resource block\n\n" };

        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, resource_block_string });
        ++ShaderResources.LastSetIdx;
    }

    std::string ShaderGeneratorImpl::fetchBodyStr(const ShaderStage& handle, const std::string& path_to_source) {
        std::string body_str;
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        if (!FileTracker.FindShaderBody(handle, body_str)) {
            if (!FileTracker.AddShaderBodyPath(handle, path_to_source)) {
                LOG(ERROR) << "Could not find or add+load a shader body source string at path " << path_to_source;
                throw std::runtime_error("Failed to find or add (then load) a shader body source string!");
            }
            else {
                FileTracker.FindShaderBody(handle, body_str);
            }
        }

        assert(!body_str.empty());
        return body_str;
    }

    void ShaderGeneratorImpl::findShaderName(const ShaderStage& handle) {
        auto& tracker = ShaderFileTracker::GetFileTracker();
        auto iter = tracker.ShaderNames.find(handle);
        if (iter == std::end(tracker.ShaderNames)) {
            throw std::runtime_error("Couldn't find name of current shader being generated!");
        }
        currentShaderName = iter->second;
    }

    void ShaderGeneratorImpl::checkInterfaceOverrides(std::string& body_src_str) {
        std::smatch match;
        if (std::regex_search(body_src_str, match, interface_override)) {
            auto iter = std::find_if(fragments.begin(), fragments.end(), [](const shaderFragment& fragment) { return fragment.Type == fragment_type::InterfaceBlock; });
            if (iter != fragments.cend()) {
                // Remove pre-existing interface block, use one defined inline in source string.
                fragments.erase(iter);
            }
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            if (!std::regex_search(body_src_str, match, end_interface_override)) {
                throw std::runtime_error("Found opening of an override interface block, but couldn't find closing #pragma.");
            }
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
        }

        if (std::regex_search(body_src_str, match, fragment_shader_no_output) && (Stage == VK_SHADER_STAGE_FRAGMENT_BIT)) {
            // shader doesn't write to color output, potentially only the backbuffer
            auto iter = std::begin(fragments);
            while (iter != std::end(fragments)) {
                if (iter->Type == fragment_type::OutputAttribute) {
                    fragments.erase(iter++);
                }
                else {
                    ++iter;
                }
            }
            body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
        }
    }

    void ShaderGeneratorImpl::addExtension(const std::string& extension_str) {
        const std::string extension_begin("#extension ");
        std::string result = extension_begin + extension_str;
        const std::string extension_close(" : enable \n");
        result += extension_close;
        fragments.emplace(shaderFragment{ fragment_type::Extension, result });
    }

    void ShaderGeneratorImpl::processBodyStrSpecializationConstants(std::string& body_src_str) {
        bool spc_found = true;
        while (spc_found) {
            std::smatch match;
            if (std::regex_search(body_src_str, match, specialization_constant)) {
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.emplace(shaderFragment{ fragment_type::SpecConstant, std::string(prefix + match[1].str() + "\n") });
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else {
                spc_found = false;
            }
        }
    }

    void ShaderGeneratorImpl::processBodyStrIncludes(std::string& body_src_str) {

        bool include_found = true;
        while (include_found) {
            std::smatch match;
            if (std::regex_search(body_src_str, match, include_local)) {
                parseInclude(match[1].str(), true);
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else if (std::regex_search(body_src_str, match, include_library)) {
                parseInclude(match[1].str(), false);
                body_src_str.erase(body_src_str.begin() + match.position(), body_src_str.begin() + match.position() + match.length());
            }
            else {
                include_found = false;
            }
        }

    }

    void ShaderGeneratorImpl::processBodyStrResourceBlocks(const ShaderStage& handle, std::string& body_str) {

        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        bool block_found = true;
        while (block_found) {
            std::smatch match;
            if (std::regex_search(body_str, match, use_set_resources)) {
                useResourceBlock(match[1].str());
                FileTracker.ShaderUsedResourceBlocks.emplace(handle, match[1].str());
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else {
                block_found = false;
            }
        }
    }

    void ShaderGeneratorImpl::processBodyStrInlineResources(const ShaderStage& handle, std::string& body_str) {
        std::smatch inline_rsrc_match;
        if (std::regex_search(body_str, inline_rsrc_match, inline_resources_begin)) {

        }
    }

    void ShaderGeneratorImpl::generate(const ShaderStage& handle, const std::string& path_to_source, const size_t num_extensions, const char* const* extensions) {
        std::string body_str{ fetchBodyStr(handle, path_to_source) };
        findShaderName(handle);
        // Includes can be any of the following: resource blocks, overrides, specialization_constants. 
        // Get them imported first so any potentially unique elements included are processed properly.
        processBodyStrIncludes(body_str);
        checkInterfaceOverrides(body_str);
        if ((num_extensions != 0) && extensions) {
            for (size_t i = 0; i < num_extensions; ++i) {
                addExtension(extensions[i]);
            }
        }
        processBodyStrSpecializationConstants(body_str);
        processBodyStrResourceBlocks(handle, body_str);
        processBodyStrInlineResources(handle, body_str);
        fragments.emplace(shaderFragment{ fragment_type::Main, body_str });
    }

}