#include "generation/ShaderGenerator.hpp"
#include <string>
#include <regex>
#include <fstream>
#include <experimental/filesystem>
#include <vector>
#include <set>
#include <unordered_map>
#include "../lua/ResourceFile.hpp"
#include "../util/FilesystemUtils.hpp"
#include "../util/ResourceFormats.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "common/UtilityStructs.hpp"
#include "easyloggingpp/src/easylogging++.h"
namespace fs = std::experimental::filesystem;

namespace st {

    extern std::unordered_map<Shader, std::string> shaderFiles;
    extern std::unordered_multimap<Shader, fs::path> shaderPaths;

    std::string BasePath = "../fragments/";
    std::string LibPath = "../fragments/include";

    static const std::regex vertex_main("#pragma VERT_MAIN_BEGIN\n");
    static const std::regex end_vertex_main("#pragma VERT_MAIN_END\n");
    static const std::regex fragment_main("#pragma FRAG_MAIN_BEGIN\n");
    static const std::regex end_fragment_main("#pragma FRAG_MAIN_END\n");

    static const std::regex vertex_interface("(#pragma VERT_INTERFACE_BEGIN\n)");
    static const std::regex end_vertex_interface("#pragma VERT_INTERFACE_END\n");
    static const std::regex fragment_interface("#pragma FRAG_INTERFACE_BEGIN\n");
    static const std::regex end_fragment_interface("#pragma FRAG_INTERFACE_END\n");

    static const std::regex interface_var_in("in\\s+\\S+\\s+\\S+;\n");
    static const std::regex interface_var_out("out\\s+\\S+\\s+\\S+;\n");

    static const std::regex lighting_model("#pragma LIGHTING_MODEL_BEGIN\n");
    static const std::regex end_lighting_model("#pragma LIGHTING_MODEL_END\n");
    static const std::regex feature_resources("#pragma FEATURE_RESOURCES_BEGIN\n");
    static const std::regex end_feature_resources("#pragma FEATURE_RESOURCES_END\n");
    
    // $TEXTURE flag indicates texture follows. First match group is type. Second match group is name.
    static const std::regex sampler2d_resource("SAMPLER_2D\\s+(\\S+;\n)");
    static const std::regex texture2d_resource("TEXTURE_2D\\s+(\\S+;\n)");
    static const std::regex uniform_resource("UNIFORM_BUFFER\\s+(\\S+)");
    static const std::regex end_uniform_resource("\\}(\\s+\\S+;\n)");
    static const std::regex storage_buffer_resource("STORAGE_BUFFER\\s+(\\S+)\\s+(\\S+)");
    static const std::regex image_buffer_resource("IMAGE_BUFFER\\s+(\\S+)\\s+(\\S+;\n)");
    static const std::regex i_image_buffer_resource("I_IMAGE_BUFFER\\s+(\\S+)\\s+(\\S+;\n)");
    static const std::regex u_image_buffer_resource("U_IMAGE_BUFFER\\s+(\\S+)\\s+(\\S+;\n)");
    static const std::regex sampler_buffer_resource("TEXEL_BUFFER\\s+(\\S+;\n)");
    static const std::regex begin_set_resources("#pragma\\s+BEGIN_RESOURCES\\s+(\\S+)\n");
    static const std::regex end_set_resources("#pragma\\s+END_RESOURCES\\s+(\\S+)\n");
    static const std::regex use_set_resources("#pragma\\s+USE_RESOURCES\\s+(\\S+)\n");
    static const std::regex include_library("#include <(\\S+)>");
    static const std::regex include_local("#include \"(\\S+)\"");
    static const std::regex specialization_constant("\\$SPC\\s+(const\\s+\\S+\\s+\\S+\\s+=\\s+\\S+;\n)");

    enum class fragment_type : uint8_t {
        Preamble = 0,
        InterfaceBlock,
        InputAttribute,
        OutputAttribute,
        glPerVertex,
        SpecConstant,
        IncludedFragment,
        ResourceBlock,
        PushConstantItem,
        Main,
        Invalid
    };

    struct shaderFragment {
        fragment_type Type = fragment_type::Invalid;
        std::string Data;
        bool operator==(const shaderFragment& other) const noexcept {
            return Type == other.Type;
        }
        bool operator<(const shaderFragment& other) const noexcept {
            return Type < other.Type;
        }
    };

    struct shader_resources_t {
        size_t LastConstantIndex = 0;
        size_t PushConstantOffset = 0;
        size_t LastInputIndex = 0;
        size_t LastOutputIndex = 0;
        size_t LastInputAttachmentIndex = 0;
        size_t NumAttributes = 0;
        size_t NumInstanceAttributes = 0;
        std::multimap<size_t, size_t> DescriptorBindings;
    };

    class ShaderGeneratorImpl {
    public:

        ShaderGeneratorImpl(const VkShaderStageFlagBits& stage);
        ~ShaderGeneratorImpl();

        ShaderGeneratorImpl(ShaderGeneratorImpl&& other) noexcept;
        ShaderGeneratorImpl& operator=(ShaderGeneratorImpl&& other) noexcept;

        const std::string& addFragment(const fs::path& path_to_source);
        std::string getFullSource() const;
        void addPerVertex();
        void addIncludePath(const char* include_path);
        void addPreamble(const fs::path& str);
        void parseInterfaceBlock(const std::string& str);
        void parseConstantBlock(const std::string& str);
        void parseInclude(const std::string& str, bool local);

        size_t getBinding(size_t & active_set) const;
        std::string getResourcePrefix(size_t active_set, const std::string & image_format) const;
        std::string getBufferMembersString(const ShaderResource & resource) const;
        std::string getUniformBufferString(const size_t& active_set, const ShaderResource & buffer, const std::string & name) const;
        std::string getStorageBufferString(const size_t & active_set, const ShaderResource & buffer, const std::string & name) const;
        std::string getStorageTexelBufferString(const size_t & active_set, const ShaderResource & buffer, const std::string & name) const;
        std::string getUniformTexelBufferString(const size_t & active_set, const ShaderResource & texel_buffer, const std::string & name) const;
        std::string getImageTypeSuffix(const VkImageCreateInfo & info) const;
        std::string getStorageImageString(const size_t & active_set, const ShaderResource & storage_image, const std::string & name) const;
        std::string getSamplerString(const size_t & active_set, const ShaderResource & sampler, const std::string & name) const;
        std::string getSampledImageString(const size_t & active_set, const ShaderResource & sampled_image, const std::string & name) const;
        std::string getCombinedImageSamplerString(const size_t & active_set, const ShaderResource & combined_image_sampler, const std::string & name) const;
        std::string getInputAttachmentString(const size_t & active_set, const ShaderResource & input_attachment, const std::string & name) const;
        void useResourceBlock(const std::string& block_name);

        std::string fetchBodyStr(const Shader & handle, const std::string & path_to_source);
        void processBodyStrSpecializationConstants(std::string & body_src_str);
        void processBodyStrIncludes(std::string & body_src_str);
        void processBodyStrResourceBlocks(const Shader& handle, std::string & body_str);
        void generate(const Shader& handle, const std::string& path_to_src);

        VkShaderStageFlagBits Stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        std::multiset<shaderFragment> fragments;
        static std::map<fs::path, std::string> fileContents;
        std::map<std::string, std::string> resourceBlocks;
        mutable shader_resources_t ShaderResources;
        ResourceFile* luaResources;
        std::vector<fs::path> includes;
    };

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
            std::string local_str{str.cbegin() + match.length() + 1, str.cend() };
            while (!local_str.empty()) {
                if (!std::regex_search(local_str, match, specialization_constant)) {
                    break;
                }
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.insert(shaderFragment{fragment_type::SpecConstant, std::string(prefix + match[1].str())});
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

    size_t ShaderGeneratorImpl::getBinding(size_t& active_set) const {
        if (ShaderResources.DescriptorBindings.count(active_set) == 0) {
            ShaderResources.DescriptorBindings.insert({ active_set, 0 });
            return 0;
        }
        else {
            const auto& iter = ShaderResources.DescriptorBindings.equal_range(active_set);
            auto minmax = std::minmax_element(iter.first, iter.second);
            ShaderResources.DescriptorBindings.insert({ active_set, (*minmax.second).second + 1 });
            return (*minmax.second).second + 1;
        }
    }

    std::string ShaderGeneratorImpl::getResourcePrefix(size_t active_set, const std::string& format_specifier) const {
        std::string prefix{
            std::string{ "layout (set = " } + std::to_string(active_set) + std::string{ ", binding = " } + std::to_string(getBinding(active_set))
        };

        if (!format_specifier.empty()) {
            prefix += std::string(", " + format_specifier + ") ");
        }
        else {
            prefix += std::string(") ");
        }

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
        const std::string prefix = getResourcePrefix(active_set, "");
        const std::string alt_name = std::string{ "_" } + name + std::string{ "_" };
        std::string result = prefix + std::string{ "uniform " } + alt_name + std::string{ " {\n" };
        result += getBufferMembersString(buffer);
        result += std::string{ "} " } + name + std::string{ ";\n\n" };
        return result;
    }

    std::string ShaderGeneratorImpl::getStorageBufferString(const size_t& active_set, const ShaderResource& buffer, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "std430");
        const std::string alt_name = std::string{ "buffer " } + std::string{ "_" } + name + std::string{ "_" };
        std::string result = prefix + alt_name + std::string{ " {\n" };
        result += getBufferMembersString(buffer);
        result += std::string{ "} " } + name + std::string{ ";\n\n" };
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

        const VkFormat& fmt = image.GetFormat();
        const std::string format_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, format_string);
        const std::string buffer_type = get_storage_texel_buffer_subtype(format_string);
        return prefix + std::string{ "uniform " } + buffer_type + std::string{ " " } +name + std::string{ ";\n" };
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
        
        const VkFormat fmt = texel_buffer.GetFormat();
        const std::string format_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, format_string);
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

        const VkFormat fmt = storage_image.GetFormat();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, fmt_string);
        const std::string resource_type = get_image_subtype(fmt_string) + getImageTypeSuffix(storage_image.ImageInfo());
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSamplerString(const size_t& active_set, const ShaderResource& sampler, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "");
        return prefix + std::string("uniform sampler ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getSampledImageString(const size_t& active_set, const ShaderResource& sampled_image, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "");
        const std::string resource_type = std::string("texture") + getImageTypeSuffix(sampled_image.ImageInfo());
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getCombinedImageSamplerString(const size_t& active_set, const ShaderResource& combined_image_sampler, const std::string& name) const {
        const std::string prefix = getResourcePrefix(active_set, "");
        const std::string resource_type = std::string("sampler") + getImageTypeSuffix(combined_image_sampler.ImageInfo());
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

    std::string ShaderGeneratorImpl::getInputAttachmentString(const size_t& active_set, const ShaderResource& input_attachment, const std::string& name) const {
        auto get_input_attachment_specifier = [&]()->std::string {
            static const std::string base_str("input_attachment_index=");
            return base_str + std::to_string(input_attachment.GetInputAttachmentIndex()) + std::string(" ");
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

        const VkFormat fmt = input_attachment.GetFormat();
        const std::string fmt_string = VkFormatEnumToString(fmt);
        const std::string prefix = getResourcePrefix(active_set, get_input_attachment_specifier());
        const std::string resource_type = get_input_attachment_subtype(fmt_string);
        return prefix + std::string("uniform ") + resource_type + std::string(" ") + name + std::string(";\n");
    }

#ifndef NDEBUG
    constexpr bool SAVE_BLOCKS_TO_FILE = false;
#else
    constexpr bool SAVE_BLOCKS_TO_FILE = false;
#endif

    void ShaderGeneratorImpl::useResourceBlock(const std::string & block_name) {
        size_t active_set = 0;
        if (!ShaderResources.DescriptorBindings.empty()) {
            active_set = ShaderResources.DescriptorBindings.crbegin()->first + 1;
        }

        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, std::string("// Resource block: ") + block_name + std::string("\n") });

        auto& resource_block = luaResources->GetResources(block_name);
        std::string resource_block_string{ "" };

        for (auto& resource : resource_block) {
            const std::string resource_name = resource.GetName();
            auto& resource_item = resource;
            switch (resource_item.GetType()) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                resource_block_string += getSamplerString(active_set, resource_item, resource_name);
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

    }

    std::string ShaderGeneratorImpl::fetchBodyStr(const Shader& handle, const std::string& path_to_source) {
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

    void ShaderGeneratorImpl::processBodyStrSpecializationConstants(std::string& body_src_str) {
        bool spc_found = true;
        while (spc_found) {
            std::smatch match;
            if (std::regex_search(body_src_str, match, specialization_constant)) {
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.emplace(shaderFragment{ fragment_type::SpecConstant, std::string(prefix + match[1].str()) });
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

    void ShaderGeneratorImpl::processBodyStrResourceBlocks(const Shader& handle, std::string& body_str) {

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

    void ShaderGeneratorImpl::generate(const Shader& handle, const std::string& path_to_source) {
        std::string body_str{ fetchBodyStr(handle, path_to_source) };
        processBodyStrSpecializationConstants(body_str);
        processBodyStrIncludes(body_str);
        processBodyStrResourceBlocks(handle, body_str);
        fragments.emplace(shaderFragment{ fragment_type::Main, body_str });
    }

    ShaderGenerator::ShaderGenerator(const VkShaderStageFlagBits& stage) : impl(std::make_unique<ShaderGeneratorImpl>(stage)) {}

    ShaderGenerator::~ShaderGenerator() {}

    ShaderGenerator::ShaderGenerator(ShaderGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderGenerator& ShaderGenerator::operator=(ShaderGenerator&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    void ShaderGenerator::SetResourceFile(ResourceFile * rsrc_file) {
        impl->luaResources = rsrc_file;
    }

    void ShaderGenerator::Generate(const Shader& handle, const char* path, const size_t num_includes, const char* const* paths) {
        for (size_t i = 0; i < num_includes; ++i) {
            assert(paths);
            impl->addIncludePath(paths[i]);
        }
        impl->generate(handle, path);
    }

    void ShaderGenerator::AddIncludePath(const char * path_to_include) {
        impl->addIncludePath(path_to_include);
    }

    void ShaderGenerator::GetFullSource(size_t * len, char * dest) const {
        
        const std::string source = impl->getFullSource();
        *len = source.size();

        if (dest != nullptr) {
            std::copy(source.cbegin(), source.cend(), dest);
        }
        
    }

    Shader ShaderGenerator::SaveCurrentToFile(const char* fname) const {
        
        const std::string source_str = impl->getFullSource();
        std::ofstream output_file(fname);
        if (!output_file.is_open()) {
            LOG(ERROR) << "Could not open file to output completed shader to.";
            throw std::runtime_error("Could not open file to write completed plaintext shader to!");
        }

        fs::path file_path(fs::absolute(fname));
        const std::string path_str = file_path.string();
        
       
        return WriteAndAddShaderSource(fname, source_str, impl->Stage);
    }

    VkShaderStageFlagBits ShaderGenerator::GetStage() const {
        return impl->Stage;
    }

    void ShaderGenerator::SetBasePath(const char * new_base_path) {
        BasePath = std::string(new_base_path);
    }

    const char * ShaderGenerator::GetBasePath() {
        return BasePath.c_str();
    }


}