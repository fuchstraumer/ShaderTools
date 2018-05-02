#include "core/ShaderGroup.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "parser/BindingGenerator.hpp"
#include "core/ShaderResource.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "../lua/ResourceFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "util/Delegate.hpp"
#include <unordered_set>
#include <experimental/filesystem>
#include "../parser/BindingGeneratorImpl.hpp"
#include "easyloggingpp/src/easylogging++.h"

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

        std::vector<const char*> includePaths;
        std::unordered_set<st::Shader> stHandles{};
        std::unique_ptr<ShaderGenerator> generator{ nullptr };
        std::unique_ptr<ShaderCompiler> compiler{ nullptr };
        std::unique_ptr<BindingGenerator> bindingGenerator{ nullptr };
        ResourceFile* rsrcFile{ nullptr };
        std::string groupName;
        std::experimental::filesystem::path resourceScriptPath;
    };

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, size_t num_includes, const char* const* include_paths) : groupName(group_name), compiler(std::make_unique<ShaderCompiler>()), 
        bindingGenerator(std::make_unique<BindingGenerator>()) {
        for (size_t i = 0; i < num_includes; ++i) {
            includePaths.emplace_back(include_paths[i]);
        }
    }

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl && other) noexcept : includePaths(std::move(other.includePaths)), stHandles(std::move(other.stHandles)), generator(std::move(other.generator)), 
        compiler(std::move(other.compiler)), bindingGenerator(std::move(other.bindingGenerator)), rsrcFile(std::move(other.rsrcFile)), groupName(std::move(other.groupName)), resourceScriptPath(std::move(other.resourceScriptPath)) {}

    ShaderGroupImpl & ShaderGroupImpl::operator=(ShaderGroupImpl && other) noexcept {
        includePaths = std::move(other.includePaths);
        stHandles = std::move(other.stHandles);
        generator = std::move(other.generator);
        compiler = std::move(other.compiler);
        bindingGenerator = std::move(other.bindingGenerator);
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

        bindingGenerator->ParseBinary(handle);

        // Need to reset generator to neutral state each time.
        generator.reset();
    }

    size_t ShaderGroup::GetNumSetsRequired() const {
        return static_cast<size_t>(impl->bindingGenerator->GetNumSets());
    }

    BindingGeneratorImpl* ShaderGroup::GetBindingGeneratorImpl() {
        return impl->bindingGenerator->GetImpl();
    }

    const BindingGeneratorImpl* ShaderGroup::GetBindingGeneratorImpl() const {
        return impl->bindingGenerator->GetImpl();
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

    void ShaderGroup::GetVertexAttributes(size_t * num_attributes, VkVertexInputAttributeDescription * attributes) const {
        const BindingGeneratorImpl* b_impl = GetBindingGeneratorImpl();
        if (!b_impl->inputAttributes.at(VK_SHADER_STAGE_VERTEX_BIT).empty()) {
            *num_attributes = b_impl->inputAttributes.at(VK_SHADER_STAGE_VERTEX_BIT).size();
            if (attributes != nullptr) {
                std::vector<VkVertexInputAttributeDescription> descriptions;
                for (const auto& attr : b_impl->inputAttributes.at(VK_SHADER_STAGE_VERTEX_BIT)) {
                    descriptions.emplace_back(attr.second);
                }
                std::copy(descriptions.begin(), descriptions.end(), attributes);
            }
        }
        else {
            *num_attributes = 0;
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
        const BindingGeneratorImpl* b_impl = GetBindingGeneratorImpl();
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

    dll_retrieved_strings_t ShaderGroup::GetSetResourceNames(const uint32_t set_idx) const {
        const auto& b_impl = GetBindingGeneratorImpl();
        auto iter = b_impl->sortedSets.find(set_idx);
        
        if (iter != b_impl->sortedSets.cend()) {
            dll_retrieved_strings_t results;
            results.SetNumStrings(iter->second.Members.size());
            size_t i = 0;
            for (auto& member : iter->second.Members) {
                results.Strings[i] = strdup(member.second.BackingResource()->GetName());
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