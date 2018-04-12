#include "lua/LuaEnvironment.hpp"
#include "sol/sol.hpp"
namespace st {

    class LuaEnvironmentImpl {
    public:
        LuaEnvironmentImpl();
        sol::state luaState;
    };
    
    LuaEnvironmentImpl::LuaEnvironmentImpl() : luaState() {
        luaState.open_libraries(sol::lib::base | sol::lib::math | sol::lib::string | sol::lib::jit);
    }

    LuaEnvironment::LuaEnvironment() {
    }

    LuaEnvironment::~LuaEnvironment()
    {
    }

}