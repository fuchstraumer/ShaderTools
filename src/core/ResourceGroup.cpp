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

    ResourceGroupImpl::ResourceGroupImpl(ResourceFile* resource_file, const char* group_name) :
        name(group_name), resourcesPtr(&resource_file->setResources[group_name]), tagsPtr(&resource_file->groupTags[group_name]) {}

    
}
