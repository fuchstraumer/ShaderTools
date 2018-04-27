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
        return impl->getNumSets();
    }    
    
    void BindingGenerator::Clear() {
        impl.reset();
        impl = std::make_unique<BindingGeneratorImpl>();
    }

    void BindingGenerator::GetShaderResources(const size_t set_idx, size_t * num_resources, ResourceUsage * resources) {

    }

    BindingGeneratorImpl * BindingGenerator::GetImpl() {
        return impl.get();
    }

    void BindingGenerator::ParseBinary(const Shader & shader) {
        impl->parseBinary(shader);
    }
}