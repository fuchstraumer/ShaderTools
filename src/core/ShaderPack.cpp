#include "core/ShaderPack.hpp"
#include "impl/ShaderPackImpl.hpp"
#include "common/UtilityStructs.hpp"
#include "core/ResourceGroup.hpp"

namespace st {

    static int screen_x() {
        return 1920;
    }

    static int screen_y() {
        return 1080;
    }

    static double z_near() {
        return 0.1;
    }

    static double z_far() {
        return 3000.0;
    }

    static double fov_y() {
        return 75.0;
    }

    void SetLoggingRepository(void* repo) {
        el::base::type::StoragePointer* storage_ptr = reinterpret_cast<el::base::type::StoragePointer*>(repo);
        el::Helpers::setStorage(*storage_ptr);
    }

    ShaderPack::ShaderPack(const char* fpath) : impl(std::make_unique<ShaderPackImpl>(fpath)) {}

    ShaderPack::ShaderPack(ShaderPackBinary * shader_pack_bin) : impl(std::make_unique<ShaderPackImpl>(shader_pack_bin)) {}

    ShaderPack::~ShaderPack() {}

    const Shader* ShaderPack::GetShaderGroup(const char * name) const {
        if (impl->groups.count(name) != 0) {
            return impl->groups.at(name).get();
        }
        else {
            return nullptr;
        }
    }

    dll_retrieved_strings_t ShaderPack::GetShaderGroupNames() const {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->groups.size());
        size_t i = 0;
        for (auto& group : impl->groups) {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    dll_retrieved_strings_t ShaderPack::GetResourceGroupNames() const {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->resourceGroups.size());
        size_t i = 0;
        for (const auto& group : impl->resourceGroups) {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    const descriptor_type_counts_t& ShaderPack::GetTotalDescriptorTypeCounts() const {
        return impl->typeCounts;
    }

    const ResourceGroup * ShaderPack::GetResourceGroup(const char * name) const {
        auto iter = impl->resourceGroups.find(name);
        if (iter != std::cend(impl->resourceGroups)) {
            return iter->second.get();
        }
        else {
            return nullptr;
        }
    }

    const ShaderResource* ShaderPack::GetResource(const char* rsrc_name) const {
        
        for (auto& group : impl->resourceGroups) {
            auto find_in_group = [rsrc_name, this](const std::string& group_name)->const ShaderResource* {
                return (*impl->resourceGroups.at(group_name))[rsrc_name];
            };
            const ShaderResource* result = find_in_group(group.first);

            if (result != nullptr) {
                return result;
            }
        }
        
        return nullptr;
    }

    engine_environment_callbacks_t & ShaderPack::RetrievalCallbacks() {
        static engine_environment_callbacks_t callbacks = engine_environment_callbacks_t{
            &screen_x,
            &screen_y,
            &z_near,
            &z_far,
            &fov_y
        };
        return callbacks;
    }

}
