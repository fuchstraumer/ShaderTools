#include "core/ResourceGroup.hpp"
#include "../lua/ResourceFile.hpp"
#include <unordered_map>
#include <string>
#include <set>
namespace st {

    class ResourceGroupImpl {
    public:
        ResourceGroupImpl(ResourceFile* resource_file, const char* group_name);
        
        std::string name;
        std::vector<ShaderResource>* resourcesPtr;
        std::vector<std::string>* tagsPtr;
        std::set<std::string> usedByGroups;
    };


    
}
