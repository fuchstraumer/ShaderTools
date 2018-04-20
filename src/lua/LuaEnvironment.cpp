#include "lua/LuaEnvironment.hpp"
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

    LuaEnvironment & LuaEnvironment::GetCurrentLuaEnvironment() {
        static LuaEnvironment environment;
        return environment;
    }

    bool LuaEnvironment::HasVariable(const std::string & var_name) {
        LuaRef ref = getGlobal(state, var_name.c_str());
        return !ref.isNil();
    }

    std::unordered_map<std::string, luabridge::LuaRef> LuaEnvironment::GetTableMap(const luabridge::LuaRef & table) {
        std::unordered_map<std::string, luabridge::LuaRef> results{};

        if (table.isNil()) {
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