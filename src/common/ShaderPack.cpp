#include "common/ShaderPack.hpp"
#include "common/ShaderGroup.hpp"
#include "lua/LuaEnvironment.hpp"
#include "lua/ResourceFile.hpp"
#include <unordered_map>
namespace st {

    struct shader_pack_file_t {

        shader_pack_file_t(const char* fname);
        ~shader_pack_file_t();

        std::string PackName;
        std::string ResourceFileName;
        std::unordered_multimap<std::string, std::string> ShaderGroups;
        std::unique_ptr<LuaEnvironment> environment;

        void parseScript();
    };

    shader_pack_file_t::shader_pack_file_t(const char * fname) : environment(std::make_unique<LuaEnvironment>()) {
        lua_State* state = environment->GetState();
        if (luaL_dofile(state, fname)) {
            const std::string err = std::string("Failed to execute Lua script, error log is:\n") + lua_tostring(state, -1) + std::string("\n");
            throw std::logic_error(err.c_str());
        }
        else {
            parseScript();
        }
        
    }

    void shader_pack_file_t::parseScript() {
        using namespace luabridge;
        lua_State* state = environment->GetState();
        LuaRef pack_name_ref = getGlobal(state, "PackName");
        PackName = pack_name_ref.cast<std::string>();
        LuaRef resource_file_ref = getGlobal(state, "ResourceFileName");
        ResourceFileName = pack_name_ref.cast<std::string>();

        LuaRef shader_groups_table = getGlobal(state, "ShaderGroups");
        auto shader_groups = environment->GetTableMap(shader_groups_table);

        for (auto& group : shader_groups) {
            auto group_entries = environment->GetTableMap(group.second);
            if (group_entries.size() == 1) {
                std::string shader_name = group_entries.at("Shader");
                ShaderGroups.emplace(group.first, shader_name);
                continue;
            }
            else {
                int num_shaders = group_entries.at("NumShaders");
                auto shader_table = environment->GetTableMap(group.second);
                for (int i = 0; i < num_shaders; ++i) {
                    std::string shader_name = shader_table.at(std::to_string(i));
                    ShaderGroups.emplace(group.first, shader_name);
                }
            }
        }
    }

    class ShaderPackImpl {
    public:
        ShaderPackImpl(const ShaderPackImpl&) = delete;
        ShaderPackImpl& operator=(const ShaderPackImpl&) = delete;

        ShaderPackImpl(const char* shader_pack_file_path);

        std::unordered_map<std::string, std::unique_ptr<ShaderGroup>> groups;
        std::unique_ptr<shader_pack_file_t> filePack;
    };


    ShaderPackImpl::ShaderPackImpl(const char * shader_pack_file_path) : filePack(std::make_unique<shader_pack_file_t>(shader_pack_file_path)) {}

    ShaderPack::ShaderPack(const char* fpath) : impl(std::make_unique<ShaderPackImpl>(fpath)) {}

    ShaderPack::~ShaderPack() {}

}