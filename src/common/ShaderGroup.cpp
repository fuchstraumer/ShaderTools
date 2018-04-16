#include "common/ShaderGroup.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "parser/BindingGenerator.hpp"
#include "parser/ShaderResource.hpp"
#include "lua/LuaEnvironment.hpp"
#include "lua/ResourceFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "util/Delegate.hpp"
#include <unordered_set>

namespace st {

    static ShaderFileTracker FileTracker;
    engine_environment_callbacks_t ShaderGroup::RetrievalCallbacks = engine_environment_callbacks_t{};

    class ShaderGroupImpl {
        ShaderGroupImpl(const ShaderGroupImpl& other) = delete;
        ShaderGroupImpl& operator=(const ShaderGroupImpl& other) = delete;
    public:

        ShaderGroupImpl(const std::string& group_name);
        ~ShaderGroupImpl();

        void addShaderBody(const Shader& handle, std::string src_str);
        void buildShader(const Shader& handle);

        void resourceScriptValid();

        std::unordered_set<st::Shader> stHandles{};
        std::unique_ptr<ShaderGenerator> generator{ nullptr };
        std::unique_ptr<ShaderCompiler> compiler{ nullptr };
        std::unique_ptr<BindingGenerator> bindingGenerator{ nullptr };
        const ResourceFile* rsrcFile{ nullptr };
        const std::string groupName;
    };

    void ShaderGroupImpl::addShaderBody(const Shader& handle, std::string src_str_path) {

    }

    void ShaderGroupImpl::buildShader(const Shader & handle)
    {
    }

    void ShaderGroupImpl::resourceScriptValid() {
        for (const auto& shader : stHandles) {
            buildShader(shader);
        }
    }

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
        /*auto iter = impl->layoutBindings.find(set_idx);
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

        return result;*/
        return shader_resource_names_t{};
    }

    ShaderGroup::ShaderGroup(const char * group_name, const char * resource_file_path) : impl(std::make_unique<ShaderGroupImpl>(group_name)){
        const std::string file_path{ resource_file_path };
        if (!FileTracker.FindResourceScript(file_path, impl->rsrcFile)) {
            // Failed to compile, register this class a watcher.
            auto watcher_delegate = delegate_t<void()>::create<ShaderGroupImpl, &ShaderGroupImpl::resourceScriptValid>(this->impl.get());
            //FileTracker.ScriptValidCallbacks[file_path].emplace_back(watcher_delegate);
        }
    }

    ShaderGroup::~ShaderGroup()
    {
    }

    Shader ShaderGroup::RegisterShader(const char * shader_name, const VkShaderStageFlagBits & flags) {
        auto iter = impl->stHandles.emplace(Shader(shader_name, flags));
        if (!iter.second) {
            throw std::runtime_error("Couldn't add Shader handle to ShaderGroup's map!");
        }
        return *iter.first;
    }

    void ShaderGroup::RegisterShader(const Shader & handle) {
        auto iter = impl->stHandles.emplace(handle);
        if (!iter.second) {
            throw std::runtime_error("Couldn't add Shader handle to ShaderGroup's map!");
        }
    }

    void ShaderGroup::AddShaderBody(const Shader & handle, const char * shader_body_src_file) {
        impl->addShaderBody(handle, shader_body_src_file);
    }

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name) : groupName(group_name), compiler(std::make_unique<ShaderCompiler>()), bindingGenerator(std::make_unique<BindingGenerator>()) { }

    ShaderGroupImpl::~ShaderGroupImpl()
    {
    }

}