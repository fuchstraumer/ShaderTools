#include "common/ShaderGroup.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "parser/BindingGenerator.hpp"
#include "common/ShaderResource.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "../lua/ResourceFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "util/Delegate.hpp"
#include <unordered_set>
#include <experimental/filesystem>
namespace st {

    engine_environment_callbacks_t ShaderGroup::RetrievalCallbacks = engine_environment_callbacks_t{};

    class ShaderGroupImpl {
        ShaderGroupImpl(const ShaderGroupImpl& other) = delete;
        ShaderGroupImpl& operator=(const ShaderGroupImpl& other) = delete;
    public:

        ShaderGroupImpl(const std::string& group_name, size_t num_includes, const char* const* include_paths);
        ~ShaderGroupImpl();

        void addShader(const Shader& handle, std::string src_str_path);


        std::vector<const char*> includePaths;
        std::unordered_set<st::Shader> stHandles{};
        std::unique_ptr<ShaderGenerator> generator{ nullptr };
        std::unique_ptr<ShaderCompiler> compiler{ nullptr };
        std::unique_ptr<BindingGenerator> bindingGenerator{ nullptr };
        ResourceFile* rsrcFile{ nullptr };
        const std::string groupName;
    };

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, size_t num_includes, const char* const* include_paths) : groupName(group_name), compiler(std::make_unique<ShaderCompiler>()), 
        bindingGenerator(std::make_unique<BindingGenerator>()) {
        for (size_t i = 0; i < num_includes; ++i) {
            includePaths.emplace_back(include_paths[i]);
        }
    }

    ShaderGroupImpl::~ShaderGroupImpl() { }

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

    ShaderGroup::ShaderGroup(const char * group_name, const char * resource_file_path, const size_t num_includes, const char* const* paths) : impl(std::make_unique<ShaderGroupImpl>(group_name, num_includes, paths)){
        const std::string file_path{ resource_file_path };
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        if (!FileTracker.FindResourceScript(file_path, impl->rsrcFile)) {
            throw std::runtime_error("Failed to execute resource script: check error log.");
        }
        else {
            namespace fs = std::experimental::filesystem;
            impl->rsrcFile = FileTracker.ResourceScripts.at(fs::path(fs::absolute(fs::path(file_path))).string()).get();
        }
    }

    ShaderGroup::~ShaderGroup() {}

    Shader ShaderGroup::AddShader(const char * shader_name, const char * body_src_file_path, const VkShaderStageFlagBits & flags) {
        Shader handle(shader_name, flags);
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        FileTracker.ShaderNames.emplace(handle, shader_name);
        auto iter = impl->stHandles.emplace(handle);
        if (!iter.second) {
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

}