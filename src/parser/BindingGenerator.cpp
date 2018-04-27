#include "parser/BindingGenerator.hpp"
#include "BindingGeneratorImpl.hpp"
#include "core/ResourceUsage.hpp"
namespace st {

    BindingGenerator::BindingGenerator() : impl(std::make_unique<BindingGeneratorImpl>()) {}

    BindingGenerator::~BindingGenerator() {}

    BindingGenerator::BindingGenerator(BindingGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    BindingGenerator& BindingGenerator::operator=(BindingGenerator&& other) noexcept {
        impl = std::move(other.impl);
        other.impl.reset();
        return *this;
    }

    uint32_t BindingGenerator::GetNumSets() const noexcept {
        return static_cast<uint32_t>(impl->getNumSets());
    }    
    
    void BindingGenerator::Clear() {
        impl.reset();
        impl = std::make_unique<BindingGeneratorImpl>();
    }

    void BindingGenerator::GetShaderResources(const size_t set_idx, size_t * num_resources, ResourceUsage * resources) {
        auto iter = impl->sortedSets.find(static_cast<unsigned int>(set_idx));
        if (iter != impl->sortedSets.cend()) {
            const auto& set = iter->second;
            *num_resources = set.Members.size();
            if (resources != nullptr) {
                std::copy(set.Members.cbegin(), set.Members.cend(), resources);
            }
        }
        else {
            *num_resources = 0;
        }
    }

    BindingGeneratorImpl * BindingGenerator::GetImpl() {
        return impl.get();
    }

    void BindingGenerator::ParseBinary(const Shader & shader) {
        impl->parseBinary(shader);
    }
}