#include "common/UtilityStructs.hpp"
#include <string>
#include <unordered_map>
namespace st {


    dll_retrieved_strings_t::dll_retrieved_strings_t() {}

    dll_retrieved_strings_t::~dll_retrieved_strings_t() {
        for (uint32_t i = 0; i < NumStrings; ++i) {
            free(Strings[i]);
        }
        delete[] Strings;
    }

    dll_retrieved_strings_t::dll_retrieved_strings_t(dll_retrieved_strings_t && other) noexcept : NumStrings(std::move(other.NumStrings)), Strings(std::move(other.Strings)) {
        other.NumStrings = 0;
        other.Strings = nullptr;
    }

    dll_retrieved_strings_t& dll_retrieved_strings_t::operator=(dll_retrieved_strings_t && other) noexcept {
        NumStrings = std::move(other.NumStrings);
        other.NumStrings = 0;
        Strings = std::move(other.Strings);
        other.Strings = nullptr;
        return *this;
    }

    void dll_retrieved_strings_t::SetNumStrings(const size_t & num_names) {
        NumStrings = num_names;
        Strings = new char*[num_names];
    }

    ShaderResourceSubObject::ShaderResourceSubObject(const ShaderResourceSubObject & other) noexcept : Type(strdup(other.Type)), Name(strdup(other.Name)), NumElements(other.NumElements),
        isComplex(other.isComplex) {}

    ShaderResourceSubObject::ShaderResourceSubObject(ShaderResourceSubObject && other) noexcept : Type(std::move(other.Type)), Name(std::move(other.Name)), NumElements(std::move(other.NumElements)),
        isComplex(std::move(other.isComplex)) {
        other.Type = nullptr;
        other.Name = nullptr;
    }

    ShaderResourceSubObject & ShaderResourceSubObject::operator=(const ShaderResourceSubObject & other) noexcept {
        Type = strdup(other.Type);
        Name = strdup(other.Name);
        NumElements = other.NumElements;
        isComplex = other.isComplex;
        return *this;
    }

    ShaderResourceSubObject & ShaderResourceSubObject::operator=(ShaderResourceSubObject && other) noexcept {
        Type = std::move(other.Type);
        Name = std::move(other.Name);
        NumElements = std::move(other.NumElements);
        isComplex = std::move(other.isComplex);
        other.Type = nullptr;
        other.Name = nullptr;
        return *this;
    }

    ShaderResourceSubObject::~ShaderResourceSubObject() {
        if (Type != nullptr) {
            free(Type);
        }
        if (Name != nullptr) {
            free(Name);
        }
    }

}
