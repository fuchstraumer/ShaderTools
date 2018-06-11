#include "core/ShaderGroup.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "reflection/ShaderReflector.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceUsage.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "../lua/ResourceFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../reflection/ShaderReflectorImpl.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include <unordered_set>
#include <experimental/filesystem>

namespace st {

    class ShaderGroupImpl {
        ShaderGroupImpl(const ShaderGroupImpl& other) = delete;
        ShaderGroupImpl& operator=(const ShaderGroupImpl& other) = delete;
    public:

        ShaderGroupImpl(const std::string& group_name, size_t num_includes, const char* const* include_paths);
        ~ShaderGroupImpl();
        ShaderGroupImpl(ShaderGroupImpl&& other) noexcept;
        ShaderGroupImpl& operator=(ShaderGroupImpl&& other) noexcept;

        void addShader(const Shader& handle, std::string src_str_path);

        std::string groupName;
        size_t idx;
        std::vector<const char*> includePaths;
        std::unordered_set<st::Shader> stHandles{};
        std::unique_ptr<ShaderGenerator> generator{ nullptr };
        std::unique_ptr<ShaderCompiler> compiler{ nullptr };
        std::unique_ptr<ShaderReflector> reflector{ nullptr };
        ResourceFile* rsrcFile{ nullptr };
        std::vector<std::string> tags;
        std::experimental::filesystem::path resourceScriptPath;
    };

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, size_t num_includes, const char* const* include_paths) : groupName(group_name), compiler(std::make_unique<ShaderCompiler>()), 
        reflector(std::make_unique<ShaderReflector>()) {
        for (size_t i = 0; i < num_includes; ++i) {
            includePaths.emplace_back(include_paths[i]);
        }
    }

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl && other) noexcept : includePaths(std::move(other.includePaths)), stHandles(std::move(other.stHandles)), generator(std::move(other.generator)), 
        compiler(std::move(other.compiler)), reflector(std::move(other.reflector)), rsrcFile(std::move(other.rsrcFile)), groupName(std::move(other.groupName)), resourceScriptPath(std::move(other.resourceScriptPath)) {}

    ShaderGroupImpl & ShaderGroupImpl::operator=(ShaderGroupImpl && other) noexcept {
        includePaths = std::move(other.includePaths);
        stHandles = std::move(other.stHandles);
        generator = std::move(other.generator);
        compiler = std::move(other.compiler);
        reflector = std::move(other.reflector);
        rsrcFile = std::move(other.rsrcFile);
        groupName = std::move(other.groupName);
        resourceScriptPath = std::move(other.resourceScriptPath);
        return *this;
    }

    void ShaderGroupImpl::addShader(const Shader& handle, std::string src_str_path) {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        generator = std::make_unique<ShaderGenerator>(handle.GetStage());
       
        generator->SetResourceFile(rsrcFile);
        generator->Generate(handle, src_str_path.c_str(), includePaths.size(), includePaths.data());
        size_t completed_src_size = 0;
        generator->GetFullSource(&completed_src_size, nullptr);
        std::string completed_src_str; completed_src_str.resize(completed_src_size);
        generator->GetFullSource(&completed_src_size, completed_src_str.data());

        const std::string& name = FileTracker.ShaderNames.at(handle);
        compiler->Compile(handle, name.c_str(), completed_src_str.c_str(), completed_src_size);
        std::string recompiled_src_str; size_t size = 0;
        compiler->RecompileBinaryToGLSL(handle, &size, nullptr);
        recompiled_src_str.resize(size);
        compiler->RecompileBinaryToGLSL(handle, &size, recompiled_src_str.data());

        reflector->ParseBinary(handle);

        // Need to reset generator to neutral state each time.
        generator.reset();
    }

    size_t ShaderGroup::GetNumSetsRequired() const {
        return static_cast<size_t>(impl->reflector->GetNumSets());
    }

    size_t ShaderGroup::GetIndex() const noexcept {
        return impl->idx;
    }

    void ShaderGroup::SetIndex(size_t _idx) {
        impl->idx = std::move(_idx);
    }

    void ShaderGroup::SetTags(const size_t num_tags, const char ** tag_strings) {
        for (size_t i = 0; i < num_tags; ++i) {
            impl->tags.emplace_back(tag_strings[i]);
        }
    }

    ShaderReflectorImpl* ShaderGroup::GetBindingGeneratorImpl() {
        return impl->reflector->GetImpl();
    }

    const ShaderReflectorImpl* ShaderGroup::GetBindingGeneratorImpl() const {
        return impl->reflector->GetImpl();
    }

    ShaderGroup::ShaderGroup(const char * group_name, const char * resource_file_path, const size_t num_includes, const char* const* paths) : impl(std::make_unique<ShaderGroupImpl>(group_name, num_includes, paths)){
        const std::string file_path{ resource_file_path };
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        if (!FileTracker.FindResourceScript(file_path, impl->rsrcFile)) {
            LOG(ERROR) << "Failed to execute or find resource script.";
            throw std::runtime_error("Failed to execute resource script: check error log.");
        }
        else {
            namespace fs = std::experimental::filesystem;
            impl->rsrcFile = FileTracker.ResourceScripts.at(fs::path(fs::absolute(fs::path(file_path))).string()).get();
            impl->resourceScriptPath = fs::absolute(fs::path(file_path));
        }

    }

    ShaderGroup::~ShaderGroup() {}

    ShaderGroup::ShaderGroup(ShaderGroup && other) noexcept : impl(std::move(other.impl)) {}

    ShaderGroup & ShaderGroup::operator=(ShaderGroup && other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    Shader ShaderGroup::AddShader(const char * shader_name, const char * body_src_file_path, const VkShaderStageFlagBits & flags) {
        Shader handle(shader_name, flags);
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        FileTracker.ShaderNames.emplace(handle, shader_name);
        FileTracker.ShaderUsedResourceScript.emplace(handle, impl->resourceScriptPath.string());
        auto iter = impl->stHandles.emplace(handle);
        if (!iter.second) {
            LOG(ERROR) << "Could not add/emplace Shader to ShaderGroup - handle or shader may already exist!";
            throw std::runtime_error("Could not add shader to ShaderGroup, failed to emplace into handles set: might already exist!");
        }
        impl->addShader(handle, body_src_file_path);
        return handle;
    }

    void ShaderGroup::GetInputAttributes(const VkShaderStageFlags stage, size_t * num_attrs, VertexAttributeInfo * attributes) const {
        impl->reflector->GetInputAttributes(stage, num_attrs, attributes);
    }

    void ShaderGroup::GetOutputAttributes(const VkShaderStageFlags stage, size_t * num_attrs, VertexAttributeInfo * attributes) const {
        impl->reflector->GetOutputAttributes(stage, num_attrs, attributes);
    }

    PushConstantInfo ShaderGroup::GetPushConstantInfo(const VkShaderStageFlags stage) const {
        return impl->reflector->GetStagePushConstantInfo(stage);
    }

    void ShaderGroup::GetShaderBinary(const Shader & handle, size_t * binary_size, uint32_t * dest_binary_ptr) const {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        auto iter = impl->stHandles.find(handle);
        std::vector<uint32_t> binary_vec;
        if (iter == impl->stHandles.cend()) {
            LOG(WARNING) << "Could not find requested shader binary in ShaderGroup.";
            *binary_size = 0;
        }
        else if (FileTracker.FindShaderBinary(handle, binary_vec)) {
            *binary_size = binary_vec.size();
            if (dest_binary_ptr != nullptr) {
                std::copy(binary_vec.begin(), binary_vec.end(), dest_binary_ptr);
            }
        }
        else {
            *binary_size = 0;
        }
    }

    void ShaderGroup::GetSetLayoutBindings(const size_t & set_idx, size_t * num_bindings, VkDescriptorSetLayoutBinding * bindings) const {
        const auto& b_impl = GetBindingGeneratorImpl();
        
        auto iter = b_impl->sortedSets.find(static_cast<uint32_t>(set_idx));
        if (iter != b_impl->sortedSets.cend()) {
            *num_bindings = iter->second.Members.size();
            if (bindings != nullptr) {
                std::vector<VkDescriptorSetLayoutBinding> bindings_vec;
                for (auto& member : iter->second.Members) {
                    bindings_vec.emplace_back((VkDescriptorSetLayoutBinding)member.second);
                }
                std::copy(bindings_vec.begin(), bindings_vec.end(), bindings);
            }
        }
        else {
            *num_bindings = 0;
        }
    }

    void ShaderGroup::GetSpecializationConstants(size_t * num_constants, SpecializationConstant * constants) const {
        const ShaderReflectorImpl* b_impl = GetBindingGeneratorImpl();
        if (!b_impl->specializationConstants.empty()) {
            *num_constants = b_impl->specializationConstants.size();
            if (constants != nullptr) {
                std::vector<SpecializationConstant> constant_vec;
                for (const auto& entry : b_impl->specializationConstants) {
                    constant_vec.emplace_back(entry.second);
                }
                std::copy(constant_vec.begin(), constant_vec.end(), constants);
            }
        }
        else {
            *num_constants = 0;
        }
    }

    void ShaderGroup::GetResourceUsages(const size_t & _set_idx, size_t * num_resources, ResourceUsage * resources) const {
        const ShaderReflectorImpl* b_impl = GetBindingGeneratorImpl();
        const uint32_t set_idx = static_cast<uint32_t>(_set_idx);
        if (b_impl->sortedSets.count(set_idx) != 0) {
            if (b_impl->sortedSets.at(set_idx).Members.empty()) {
                *num_resources = 0;
                return;
            }
            *num_resources = b_impl->sortedSets.at(set_idx).Members.size();
            if (resources != nullptr) {
                std::vector<ResourceUsage> resources_vec;
                for (const auto& rsrc : b_impl->sortedSets.at(set_idx).Members) {
                    resources_vec.emplace_back(rsrc.second);
                }
                std::copy(resources_vec.begin(), resources_vec.end(), resources);
            }   
        }
    }

    VkShaderStageFlags ShaderGroup::Stages() const noexcept {
        VkShaderStageFlags result = 0;
        for (auto& shader : impl->stHandles) {
            result |= shader.GetStage();
        }
        return result;
    }

    dll_retrieved_strings_t ShaderGroup::GetTags() const {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->tags.size());
        for (size_t i = 0; i < impl->tags.size(); ++i) {
            results.Strings[i] = strdup(impl->tags[i].c_str());
        }
        return results;
    }

    dll_retrieved_strings_t ShaderGroup::GetSetResourceNames(const uint32_t set_idx) const {
        const auto& b_impl = GetBindingGeneratorImpl();
        auto iter = b_impl->sortedSets.find(set_idx);
        
        if (iter != b_impl->sortedSets.cend()) {
            dll_retrieved_strings_t results;
            results.SetNumStrings(iter->second.Members.size());
            size_t i = 0;
            for (auto& member : iter->second.Members) {
                results.Strings[i] = strdup(member.second.BackingResource()->Name());
                ++i;
            }
            return results;
        }
        else {
            return dll_retrieved_strings_t();
        }
        
    }

    dll_retrieved_strings_t ShaderGroup::GetUsedResourceBlocks() const {
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        size_t num_strings = 0;
        for (auto& handle : impl->stHandles) {
            num_strings += ftracker.ShaderUsedResourceBlocks.count(handle);
        }
        dll_retrieved_strings_t results;
        results.SetNumStrings(num_strings);

        size_t curr_idx = 0;
        for (auto& handle : impl->stHandles) {
            auto iter_pair = ftracker.ShaderUsedResourceBlocks.equal_range(handle);
            for (auto iter = iter_pair.first; iter != iter_pair.second; ++iter) {
                results.Strings[curr_idx] = strdup(iter->second.c_str());
                ++curr_idx;
            }
        }

        return results;
    }

}
