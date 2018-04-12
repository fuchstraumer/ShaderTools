#pragma once
#ifndef ST_LUA_ENVIRONMENT_HPP
#define ST_LUA_ENVIRONMENT_HPP
#include "common/CommonInclude.hpp"

namespace st {

    class LuaEnvironmentImpl;

    class LuaEnvironment {
    public:

        LuaEnvironment();
        ~LuaEnvironment();

    private:
        std::unique_ptr<LuaEnvironmentImpl> impl;
    };

}

#endif //!ST_LUA_ENVIRONMENT_HPP