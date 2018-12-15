#include "core/Shader.hpp"
#include "impl/ShaderImpl.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceUsage.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "../lua/ResourceFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "reflection/ShaderReflector.hpp"
#include "../reflection/impl/ShaderReflectorImpl.hpp"
#include <unordered_set>
#include <experimental/filesystem>
#include "easyloggingpp/src/easylogging++.h"

namespace st {

    size_t Shader::GetNumSetsRequired() const {
        return static_cast<size_t>(impl->reflector->GetNumSets());
    }

    size_t Shader::GetIndex() const noexcept {
        return impl->idx;
    }

    void Shader::SetIndex(size_t _idx) {
        impl->idx = std::move(_idx);
    }

    void Shader::SetTags(const size_t num_tags, const char ** tag_strings) {
        for (size_t i = 0; i < num_tags; ++i) {
            impl->tags.emplace_back(tag_strings[i]);
        }
    }

    ShaderReflectorImpl* Shader::GetBindingGeneratorImpl() {
        return impl->reflector->GetImpl();
    }

    const ShaderReflectorImpl* Shader::GetBindingGeneratorImpl() const {
        return impl->reflector->GetImpl();
    }

    Shader::Shader(const char* group_name, const char* resource_file_path, const size_t num_extensions, const char* const* extensions, const size_t num_includes, const char* const* paths) 
        : impl(std::make_unique<ShaderGroupImpl>(group_name, num_extensions, extensions, num_includes, paths)){
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

    Shader::~Shader() {}

    Shader::Shader(Shader && other) noexcept : impl(std::move(other.impl)) {}

    Shader & Shader::operator=(Shader && other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    ShaderStage Shader::AddShaderStage(const char * shader_name, const char * body_src_file_path, const VkShaderStageFlagBits & flags) {
        ShaderStage handle(shader_name, flags);
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        FileTracker.ShaderNames.emplace(handle, shader_name);
        FileTracker.ShaderUsedResourceScript.emplace(handle, impl->resourceScriptPath.string());
        auto iter = impl->stHandles.emplace(handle);
        if (!iter.second) {
            LOG(ERROR) << "Could not add/emplace Shader to Shader - handle or shader may already exist!";
            throw std::runtime_error("Could not add shader to Shader, failed to emplace into handles set: might already exist!");
        }
        impl->addShaderStage(handle, body_src_file_path);
        return handle;
    }

    void Shader::GetInputAttributes(const VkShaderStageFlags stage, size_t * num_attrs, VertexAttributeInfo * attributes) const {
        impl->reflector->GetInputAttributes(stage, num_attrs, attributes);
    }

    void Shader::GetOutputAttributes(const VkShaderStageFlags stage, size_t * num_attrs, VertexAttributeInfo * attributes) const {
        impl->reflector->GetOutputAttributes(stage, num_attrs, attributes);
    }

    PushConstantInfo Shader::GetPushConstantInfo(const VkShaderStageFlags stage) const {
        return impl->reflector->GetStagePushConstantInfo(stage);
    }

    void Shader::GetShaderStages(size_t * num_stages, ShaderStage * stages) const {
        *num_stages = impl->stHandles.size();
        if (stages != nullptr) {
            std::vector<ShaderStage> stages_buffer;
            for (const auto& stage : impl->stHandles) {
                stages_buffer.emplace_back(stage);
            }
            std::copy(std::begin(stages_buffer), std::end(stages_buffer), stages);
        }
    }

    void Shader::GetShaderBinary(const ShaderStage & handle, size_t * binary_size, uint32_t * dest_binary_ptr) const {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        auto iter = impl->stHandles.find(handle);
        std::vector<uint32_t> binary_vec;
        if (iter == impl->stHandles.cend()) {
            LOG(WARNING) << "Could not find requested shader binary in Shader.";
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

    void Shader::GetSetLayoutBindings(const size_t & set_idx, size_t * num_bindings, VkDescriptorSetLayoutBinding * bindings) const {
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

    void Shader::GetSpecializationConstants(size_t * num_constants, SpecializationConstant * constants) const {
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

    void Shader::GetResourceUsages(const size_t & _set_idx, size_t * num_resources, ResourceUsage * resources) const {
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

    VkShaderStageFlags Shader::Stages() const noexcept {
        VkShaderStageFlags result = 0;
        for (auto& shader : impl->stHandles) {
            result |= shader.GetStage();
        }
        return result;
    }

    bool Shader::OptimizationEnabled(const ShaderStage & handle) const noexcept {
        if (auto iter = impl->optimizationEnabled.find(handle); iter != std::end(impl->optimizationEnabled)) {
            return iter->second;
        }
        else {
            return false;
        }
    }

    size_t Shader::ResourceGroupSetIdx(const char * name) const {
        auto iter = impl->resourceGroupBindingIndices.find(name);
        if (iter != std::cend(impl->resourceGroupBindingIndices)) {
            return iter->second;
        }
        else {
            return std::numeric_limits<size_t>::max();
        }
    }

    void Shader::SetResourceGroupIdx(const char * name, size_t idx) {
        impl->resourceGroupBindingIndices[name] = std::move(idx);
    }

    dll_retrieved_strings_t Shader::GetTags() const {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->tags.size());
        for (size_t i = 0; i < impl->tags.size(); ++i) {
            results.Strings[i] = strdup(impl->tags[i].c_str());
        }
        return results;
    }

    dll_retrieved_strings_t Shader::GetSetResourceNames(const uint32_t set_idx) const {
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

    dll_retrieved_strings_t Shader::GetUsedResourceBlocks() const {
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
