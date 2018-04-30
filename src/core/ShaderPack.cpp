#include "core/ShaderPack.hpp"
#include "core/ShaderGroup.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "../lua/ResourceFile.hpp"
#include "core/ShaderResource.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../parser/BindingGeneratorImpl.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include <unordered_map>
#include <experimental/filesystem>
#include <set>
#include <future>
#include <array>

namespace st {

    struct shader_pack_file_t {

        shader_pack_file_t(const char* fname);
        ~shader_pack_file_t() {};

        std::string PackName;
        std::string ResourceFileName;
        std::unordered_map<std::string, std::map<VkShaderStageFlagBits, std::string>> ShaderGroups;
        std::unique_ptr<LuaEnvironment> environment;

        void parseScript();
    };

    shader_pack_file_t::shader_pack_file_t(const char * fname) : environment(std::make_unique<LuaEnvironment>()) {
        lua_State* state = environment->GetState();
        if (luaL_dofile(state, fname)) {
            const std::string err = std::string("Failed to execute Lua script, error log is:\n") + lua_tostring(state, -1) + std::string("\n");
            LOG(ERROR) << err;
            throw std::logic_error(err.c_str());
        }
        else {
            parseScript();
        }
        
    }

    void shader_pack_file_t::parseScript() {
        using namespace luabridge;
        lua_State* state = environment->GetState();
        {
            LuaRef pack_name_ref = getGlobal(state, "PackName");
            PackName = pack_name_ref.cast<std::string>();
            LuaRef resource_file_ref = getGlobal(state, "ResourceFileName");
            ResourceFileName = resource_file_ref.cast<std::string>();

            LuaRef shader_groups_table = getGlobal(state, "ShaderGroups");
            auto shader_groups = environment->GetTableMap(shader_groups_table);

            for (auto& group : shader_groups) {
                auto group_entries = environment->GetTableMap(group.second);
                for (auto& entry : group_entries) {
                    const std::string& shader_stage = entry.first;
                    const std::string shader_name = entry.second;
                    if (shader_stage == "Vertex") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_VERTEX_BIT, shader_name);
                    }
                    else if (shader_stage == "Fragment") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_FRAGMENT_BIT, shader_name);
                    }
                    else if (shader_stage == "Geometry") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_GEOMETRY_BIT, shader_name);
                    }
                    else if (shader_stage == "TessEval") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shader_name);
                    }
                    else if (shader_stage == "TessControl") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shader_name);
                    }
                    else if (shader_stage == "Compute") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_COMPUTE_BIT, shader_name);
                    }
                    else {
                        LOG(ERROR) << "Found invalid shader stage type in ShaderPack Lua script.";
                        throw std::domain_error("Invalid shader stage in parsed Lua script.");
                    }
                }
            }
        }
        environment.reset();
    }

    class ShaderPackImpl {
    public:
        ShaderPackImpl(const ShaderPackImpl&) = delete;
        ShaderPackImpl& operator=(const ShaderPackImpl&) = delete;

        ShaderPackImpl(const char* shader_pack_file_path);
        void createGroups();
        void createSingleGroup(const std::string& name, const std::map<VkShaderStageFlagBits, std::string>& shader_map);
        void createResourceGroupData();
        void createBackingResourceData();

        std::vector<std::set<ShaderResource>> resources;
        std::unordered_map<std::string, std::vector<size_t>> groupSetIndices;
        std::unordered_map<std::string, std::unique_ptr<ShaderGroup>> groups;
        std::unique_ptr<shader_pack_file_t> filePack;
        std::experimental::filesystem::path workingDir;
        std::mutex guardMutex;
    };


    ShaderPackImpl::ShaderPackImpl(const char * shader_pack_file_path) : filePack(std::make_unique<shader_pack_file_t>(shader_pack_file_path)), workingDir(shader_pack_file_path) {
        namespace fs = std::experimental::filesystem;
        workingDir = fs::absolute(workingDir);
        workingDir = workingDir.remove_filename();
        createGroups();
        createBackingResourceData();
    }

    void ShaderPackImpl::createGroups() {
        namespace fs = std::experimental::filesystem;

        fs::path resource_path = workingDir / fs::path(filePack->ResourceFileName);
        if (!fs::exists(resource_path)) {
            LOG(ERROR) << "Resource Lua script could not be found using specified path.";
            throw std::runtime_error("Couldn't find resource file using given path.");
        }
        const std::string resource_path_str = resource_path.string();

        const std::string working_dir_str = workingDir.string();
        static const std::array<const char*, 1> base_includes{ working_dir_str.c_str() };

        for (const auto& group : filePack->ShaderGroups) {
            groups.emplace(group.first, std::make_unique<ShaderGroup>(group.first.c_str(), resource_path_str.c_str(), base_includes.size(), base_includes.data()));
            createSingleGroup(group.first, group.second);
        }
    }

    void ShaderPackImpl::createSingleGroup(const std::string & name, const std::map<VkShaderStageFlagBits, std::string>& shader_map) {
        namespace fs = std::experimental::filesystem;

        for (const auto& shader : shader_map) {
            std::string shader_name_str = shader.second;
            fs::path shader_path = workingDir / fs::path(shader_name_str);
            if (!fs::exists(shader_path)) {
                LOG(ERROR) << "Shader path given could not be found.";
                throw std::runtime_error("Failed to find shader using given path.");
            }
            const std::string shader_path_str = shader_path.string();

            size_t name_prefix_idx = shader_name_str.find_last_of('/');
            if (name_prefix_idx != std::string::npos) {
                shader_name_str.erase(shader_name_str.begin(), shader_name_str.begin() + name_prefix_idx + 1);
            }

            size_t name_suffix_idx = shader_name_str.find_first_of('.');
            if (name_suffix_idx != std::string::npos) {
                shader_name_str.erase(shader_name_str.begin() + name_suffix_idx, shader_name_str.end());
            }


            groups.at(name)->AddShader(shader_name_str.c_str(), shader_path_str.c_str(), shader.first);
        }
    }


    void ShaderPackImpl::createBackingResourceData() {
        namespace fs = std::experimental::filesystem;
        fs::path resource_path = workingDir / fs::path(filePack->ResourceFileName);
        if (!fs::exists(resource_path)) {
            LOG(ERROR) << "Couldn't find resource Lua script using given path.";
            throw std::runtime_error("Couldn't find resource file using given path.");
        }
        const std::string resource_path_str = resource_path.string();

        auto& ftracker = ShaderFileTracker::GetFileTracker();
        ResourceFile* file_ptr = ftracker.ResourceScripts.at(resource_path_str).get();
        const auto& file_resources = file_ptr->GetAllResources();


        

    }

    ShaderPack::ShaderPack(const char* fpath) : impl(std::make_unique<ShaderPackImpl>(fpath)) {}

    ShaderPack::~ShaderPack() {}

    ShaderGroup * ShaderPack::GetShaderGroup(const char * name) const {
        if (impl->groups.count(name) != 0) {
            return impl->groups.at(name).get();
        }
        else {
            return nullptr;
        }
    }

}