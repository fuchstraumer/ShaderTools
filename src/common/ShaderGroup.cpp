#include "common/ShaderGroup.hpp"
#include "generation/Compiler.hpp"
#include "parser/BindingGenerator.hpp"
namespace st {

    class ShaderGroupImpl {
        ShaderGroupImpl(const ShaderGroupImpl& other) = delete;
        ShaderGroupImpl& operator=(const ShaderGroupImpl& other) = delete;
    public:

        ShaderGroupImpl();
        ~ShaderGroupImpl();

        void addShader(const char* fname, VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
        void addShader(const char* shader_name, const char* src_str, const uint32_t src_str_len, const VkShaderStageFlagBits stage);

        std::map<VkShaderStageFlagBits, st::Shader> stHandles;

        std::vector<VkVertexInputAttributeDescription> inputAttrs;
        std::map<uint32_t, std::map<std::string, VkDescriptorSetLayoutBinding>> layoutBindings;

        std::unique_ptr<ShaderCompiler> compiler;
        std::unique_ptr<BindingGenerator> bindingGenerator;
    };

    ShaderGroup::shader_resource_names_t::shader_resource_names_t() {}

    ShaderGroup::shader_resource_names_t::~shader_resource_names_t() {
        for (uint32_t i = 0; i < NumNames; ++i) {
            free(Names[i]);
        }
    }

    ShaderGroup::shader_resource_names_t::shader_resource_names_t(shader_resource_names_t && other) noexcept : NumNames(std::move(other.NumNames)), Names(std::move(other.Names)) {
        other.NumNames = 0;
        other.Names = nullptr;
    }

    ShaderGroup::shader_resource_names_t& ShaderGroup::shader_resource_names_t::operator=(shader_resource_names_t && other) noexcept {
        NumNames = std::move(other.NumNames);
        other.NumNames = 0;
        Names = std::move(other.Names);
        other.Names = nullptr;
        return *this;
    }

    ShaderGroup::shader_resource_names_t ShaderGroup::GetSetResourceNames(const uint32_t set_idx) const {
        auto iter = impl->layoutBindings.find(set_idx);
        if (iter == impl->layoutBindings.end()) { 
            return shader_resource_names_t{};
        }

        const auto& set = iter->second;
        shader_resource_names_t result;
        result.NumNames = static_cast<uint32_t>(set.size());
        size_t i = 0;

        for (auto& member : set) {
            result.Names[i] = strdup(member.first.c_str());
            ++i;
        }

        return result;
    }   
}