#include "parser/BindingGenerator.hpp"
#include "BindingGeneratorImpl.hpp"

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
        return static_cast<uint32_t>(impl->sortedSets.size());
    }    
    
    void BindingGenerator::Clear() {
        impl.reset();
        impl = std::make_unique<BindingGeneratorImpl>();
    }

    BindingGeneratorImpl * BindingGenerator::GetImpl() {
        return impl.get();
    }

    void BindingGenerator::ParseBinary(const Shader & shader) {
        impl->parseBinary(shader);
    }
}