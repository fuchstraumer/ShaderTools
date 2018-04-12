#pragma once
#ifndef ST_LUA_ENVIRONMENT_HPP
#define ST_LUA_ENVIRONMENT_HPP
#include "sol.hpp"
#include "mango/math/geometry.hpp"
namespace st {

    class LuaEnvironment {
    public:

        LuaEnvironment();
        ~LuaEnvironment();

        sol::state& GetState();
        sol::environment& GetEnvironment();

    private:

        sol::state luaState;
        sol::environment environment;

    };

}

#endif //!ST_LUA_ENVIRONMENT_HPP