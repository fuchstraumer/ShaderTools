#include "lua/ResourceFile.hpp"
#include "parser/ShaderResource.hpp"
#include <unordered_map>
namespace st {

    ResourceFile::ResourceFile(const char* fname, sol::state& lstate) : lState(lstate) {
        luaEnvironment = sol::environment(lstate, sol::Create, lstate.globals());
    }

    void ResourceFile::registerResourceEnumTypeTable() {
        
    }
}