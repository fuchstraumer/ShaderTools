#include "generation/ShaderGenerator.hpp"

#include <string>
#include <algorithm>
#include <regex>
#include <fstream>
#include <string_view>
#include <experimental/filesystem>
#include <map>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cassert>
#include <sstream>
#include <unordered_map>

#include "lua/ResourceFile.hpp"
#include "../util/FilesystemUtils.hpp"

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
        LightingModel,
        FreeFunction,
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

        void addBody(const fs::path& path_to_body);
        const std::string& addFragment(const fs::path& path_to_source);
        void addPreamble(const fs::path& str);
        void parseInterfaceBlock(const std::string& str);
        void parseConstantBlock(const std::string& str);
        void parseInclude(const std::string& str, bool local);
        void useResourceBlock(const std::string& block_name);
        void addPerVertex();
        std::string getFullSource() const;


        void addIncludePath(const char* include_path);

        bool collated = false;
        VkShaderStageFlagBits Stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        std::multiset<shaderFragment> fragments;
        std::map<fs::path, std::string> fileContents;
        std::map<std::string, std::string> resourceBlocks;
        shader_resources_t ShaderResources;
        ResourceFile* luaResources;
        std::vector<fs::path> includes;
    };

    ShaderGeneratorImpl::ShaderGeneratorImpl(const VkShaderStageFlagBits& stage) : Stage(stage) {
        fs::path preamble(std::string(BasePath) + "builtins/preamble450.glsl");
        addPreamble(preamble);
        if (Stage == VK_SHADER_STAGE_VERTEX_BIT) {
            fs::path vertex_interface_base(std::string(BasePath) + "builtins/vertexInterface.glsl");
            const auto& interface_str = addFragment(vertex_interface_base);
            parseInterfaceBlock(interface_str);
            addPerVertex();
        }
        else if (Stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            fs::path fragment_interface_base(std::string(BasePath) + "builtins/fragmentInterface.glsl");
            const auto& interface_str = addFragment(fragment_interface_base);
            parseInterfaceBlock(interface_str);
        }

    }

    ShaderGeneratorImpl::~ShaderGeneratorImpl() {
        std::ofstream out("compute_test.glsl");
        for (auto& frag : fragments) {
            out << frag.Data;
        }
        out.close();
    }

    ShaderGeneratorImpl::ShaderGeneratorImpl(ShaderGeneratorImpl&& other) noexcept : Stage(std::move(other.Stage)), fragments(std::move(other.fragments)),
        fileContents(std::move(other.fileContents)), resourceBlocks(std::move(other.resourceBlocks)),
        ShaderResources(std::move(other.ShaderResources)), includes(std::move(other.includes)) {}

    ShaderGeneratorImpl& ShaderGeneratorImpl::operator=(ShaderGeneratorImpl&& other) noexcept {
        Stage = std::move(other.Stage);
        fragments = std::move(other.fragments);
        fileContents = std::move(other.fileContents);
        resourceBlocks = std::move(other.resourceBlocks);
        ShaderResources = std::move(other.ShaderResources);
        includes = std::move(other.includes);
        return *this;
    }

    void ShaderGeneratorImpl::addPreamble(const fs::path& path) {
        std::ifstream file_stream(path);
        std::string preamble{ std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>() };
        fileContents.emplace(path, preamble);
        fragments.insert(shaderFragment{ fragment_type::Preamble, preamble });
    }

    const std::string& ShaderGeneratorImpl::addFragment(const fs::path& src_path) {
        fs::path source_file(src_path);

        if (!fs::exists(source_file)) {
            throw std::runtime_error("Could not find given shader fragment source file.");
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
                throw std::runtime_error("Failed to find desired include in local include paths.");
            }
        }

        if (fileContents.count(include_path)) {
            fragments.emplace(shaderFragment{ fragment_type::IncludedFragment, fileContents.at(include_path) });
            return;
        }

        std::ifstream include_file(include_path);

        if (!include_file.is_open()) {
            throw std::runtime_error("Failed to open include file, despite having a valid path!");
        }

        std::string file_content{ std::istreambuf_iterator<char>(include_file), std::istreambuf_iterator<char>() };
        fileContents.emplace(include_path, file_content);
        fragments.emplace(shaderFragment{ fragment_type::IncludedFragment, file_content });

    }

    void ShaderGeneratorImpl::useResourceBlock(const std::string & block_name) {
        size_t active_set = 0;
        if (!ShaderResources.DescriptorBindings.empty()) {
            active_set = ShaderResources.DescriptorBindings.crbegin()->first + 1;
        }
        std::string block_str = resourceBlocks.at(block_name);

        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, std::string("// Resource block:") + block_name + std::string("\n") });

        auto get_binding = [&]()->size_t {
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
        };

        auto get_prefix = [&](const std::string& image_type)->std::string {
            std::string prefix("layout (set = " + std::to_string(active_set) + ", binding = " + std::to_string(get_binding()));
            if (!image_type.empty()) {
                prefix += std::string(", " + image_type + ") ");
            }
            else {
                prefix += std::string(") ");
            }
            return prefix;
        };

        auto uniform_buffer_string = [&](const UniformBuffer& buffer, const std::string& name)->std::string {
            const std::string prefix = get_prefix("");
            const std::string alt_name = std::string{ "_" } +name + std::string{ "_" };
            std::string result = prefix + std::string{ " uniform " } +name + std::string{ " {\n" };
            for (const auto& member : buffer.MemberTypes) {
                result += std::string{ "    " };
                result += member.second;
                result += std::string{ " " } + member.first;
                result += std::string{ ";\n" };
            }
            result += std::string{ "} " } +name + std::string{ ";\n" };
            return result;
        };

        auto storage_buffer_string = [&](const StorageBuffer& buffer, const std::string& name)->std::string {
            const std::string prefix = get_prefix("std430");
            const std::string alt_name = std::string{" buffer "} + std::string{ "_" } +name + std::string{ "_" };
            std::string result = prefix + alt_name + std::string{ " {\n" };
            result += std::string{ "    " } + buffer.ElementType + std::string{ " Data[];\n" };
            result += std::string{ "} " } +name + std::string{ ";\n" };
            return result;
        };

        auto get_storage_buffer_subtype = [&](const std::string& image_format)->std::string {
            if (image_format.back() == 'f') {
                return std::string("imageBuffer");
            }
            else {
                std::string last_two = image_format.substr(image_format.size() - 2, 2);
                if (last_two == "ui") {
                    return std::string("uimageBuffer");
                }
                else {
                    return std::string("iimageBuffer");
                }
            }
        };

        auto storage_image_string = [&](const StorageImage& image, const std::string& name)->std::string {
            const std::string prefix = get_prefix(image.Format);
            const std::string buffer_type = get_storage_buffer_subtype(image.Format);
            return prefix + std::string{ " " } + buffer_type + std::string{ " " } + name + std::string{ ";\n" };
        };

        auto& resource_block = luaResources->GetResources(block_name);
        std::string resource_block_string{ "\n" };

        for (auto& resource : resource_block) {
            const auto& resource_name = resource.first;
            const auto& resource_item = resource.second;
            
            if (std::holds_alternative<UniformBuffer>(resource_item)) {
                resource_block_string += uniform_buffer_string(std::get<UniformBuffer>(resource_item), resource_name);
            }
            else if (std::holds_alternative<StorageBuffer>(resource_item)) {
                resource_block_string += storage_buffer_string(std::get<StorageBuffer>(resource_item), resource_name);
            }
            else if (std::holds_alternative<StorageImage>(resource_item)) {
                resource_block_string += storage_image_string(std::get<StorageImage>(resource_item), resource_name);
            }
            else {
                throw std::domain_error("Encountered currently unsupported resource type.");
            }

        }

        std::string block_file_name = block_name + std::string{ ".glsl" };
        std::ofstream block_stream(block_name);
        if (!block_stream.is_open()) {
            throw std::runtime_error("failed to open stream!");
        }
        block_stream << resource_block_string;
        block_stream.flush(); block_stream.close();
         
        fragments.emplace(shaderFragment{ fragment_type::ResourceBlock, resource_block_string });

    }

    void ShaderGeneratorImpl::addBody(const fs::path& path_to_body) {
        std::ifstream body_file(path_to_body);
        if (!body_file.is_open()) {
            throw std::runtime_error("Failed to open given file that was being used as body of the shader!");
        }
        std::string body_str{ std::istreambuf_iterator<char>(body_file), std::istreambuf_iterator<char>() };

        bool spc_found = true;
        while (spc_found) {
            std::smatch match;
            if (std::regex_search(body_str, match, specialization_constant)) {
                const std::string prefix("layout (constant_id = " + std::to_string(ShaderResources.LastConstantIndex++) + ") ");
                fragments.emplace(shaderFragment{ fragment_type::SpecConstant, std::string(prefix + match[1].str()) });
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else {
                spc_found = false;
            }
        }

        bool include_found = true;
        while (include_found) {
            std::smatch match;
            if (std::regex_search(body_str, match, include_local)) {
                parseInclude(match[1].str(), true);
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else if (std::regex_search(body_str, match, include_library)) {
                parseInclude(match[1].str(), false);
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else {
                include_found = false;
            }
        }

        bool block_found = true;
        while (block_found) {
            std::smatch match;
            if (std::regex_search(body_str, match, use_set_resources)) {
                useResourceBlock(match[1].str());
                body_str.erase(body_str.begin() + match.position(), body_str.begin() + match.position() + match.length());
            }
            else {
                block_found = false;
            }
        }

        fragments.emplace(shaderFragment{ fragment_type::Main, body_str });
    }

    ShaderGenerator::ShaderGenerator(const VkShaderStageFlagBits& stage) : impl(std::make_unique<ShaderGeneratorImpl>(stage)) {}

    ShaderGenerator::~ShaderGenerator() {}

    ShaderGenerator::ShaderGenerator(ShaderGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderGenerator& ShaderGenerator::operator=(ShaderGenerator&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    void ShaderGenerator::AddBody(const char* path, const size_t num_includes, const char* const* paths) {
        for (size_t i = 0; i < num_includes; ++i) {
            assert(paths);
            impl->addIncludePath(paths[i]);
        }
        impl->addBody(path);
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


}