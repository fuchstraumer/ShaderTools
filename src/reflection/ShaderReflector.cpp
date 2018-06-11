#include "reflection/ShaderReflector.hpp"
#include "ShaderReflectorImpl.hpp"
#include "core/ResourceUsage.hpp"
namespace st {

    ShaderReflector::ShaderReflector() : impl(std::make_unique<ShaderReflectorImpl>()) {}

    ShaderReflector::~ShaderReflector() {}

    ShaderReflector::ShaderReflector(ShaderReflector&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderReflector& ShaderReflector::operator=(ShaderReflector&& other) noexcept {
        impl = std::move(other.impl);
        other.impl.reset();
        return *this;
    }

    uint32_t ShaderReflector::GetNumSets() const noexcept {
        return static_cast<uint32_t>(impl->getNumSets());
    }    
    
    void ShaderReflector::Clear() {
        impl.reset();
        impl = std::make_unique<ShaderReflectorImpl>();
    }

    void ShaderReflector::GetShaderResources(const size_t set_idx, size_t * num_resources, ResourceUsage * resources) {
        auto iter = impl->sortedSets.find(static_cast<unsigned int>(set_idx));
        if (iter != impl->sortedSets.cend()) {
            const auto& set = iter->second;
            *num_resources = set.Members.size();
            std::vector<ResourceUsage> resources_copy;
            for (const auto& member : set.Members) {
                resources_copy.emplace_back(member.second);
            }   

            if (resources != nullptr) {
                std::copy(resources_copy.begin(), resources_copy.end(), resources);
            }
        }
        else {
            *num_resources = 0;
        }
    }

    ShaderReflectorImpl * ShaderReflector::GetImpl() {
        return impl.get();
    }

    void ShaderReflector::ParseBinary(const Shader & shader) {
        impl->parseBinary(shader);
    }
}
