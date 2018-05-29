#include "LuaEnvironment.hpp"
#include "easyloggingpp/src/easylogging++.h"
using namespace luabridge;

namespace st {

    LuaEnvironment::LuaEnvironment() : state(luaL_newstate()) {
        luaL_openlibs(state);
    }

    LuaEnvironment::~LuaEnvironment() {
        if (state != nullptr) {
            lua_close(state);
        }
    }

    void LuaEnvironment::Execute(const char * fname) {
        if (luaL_dofile(state, fname)) {
            const std::string err = std::string("Failed to execute Lua script, error log is:\n") + lua_tostring(state, -1) + std::string("\n");
            LOG(ERROR) << err;
            throw std::logic_error(err.c_str());
        }
    }

    bool LuaEnvironment::HasVariable(const std::string & var_name) {
        LuaRef ref = getGlobal(state, var_name.c_str());
        return !ref.isNil();
    }

    std::unordered_map<std::string, luabridge::LuaRef> LuaEnvironment::GetTableMap(const luabridge::LuaRef & table) {
        std::unordered_map<std::string, luabridge::LuaRef> results{};

        if (table.isNil()) {
            LOG(WARNING) << "Passed LuaRef to GetTableMap method was nil!";
            return results;
        }

        auto table_state = table.state();
        push(table_state, table);

        lua_pushnil(table_state);
        while (lua_next(table_state, -2) != 0) {
            if (lua_isstring(table_state, -2)) {
                results.emplace(lua_tostring(table_state, -2), LuaRef::fromStack(table_state, -1));
            }
            lua_pop(table_state, 1);
        }

        return results;
    }

    lua_State * LuaEnvironment::GetState() {
        return state;
    }

}
