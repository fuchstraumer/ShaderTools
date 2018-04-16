#pragma once
#ifndef ST_LUA_ENVIRONMENT_HPP
#define ST_LUA_ENVIRONMENT_HPP
#include "common/CommonInclude.hpp"
#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include <unordered_map>

namespace st {

    class LuaEnvironment {
    public:

        LuaEnvironment();
        ~LuaEnvironment();

        void Execute(const char* fname);

        bool HasVariable(const std::string& var_name);
        std::unordered_map<std::string, luabridge::LuaRef> GetTableMap(const luabridge::LuaRef& table);

        lua_State* GetState();

    private:
        lua_State * state{ nullptr };
    };

    LuaEnvironment* GetCurrentLuaEnvironment();

}

#endif //!ST_LUA_ENVIRONMENT_HPP