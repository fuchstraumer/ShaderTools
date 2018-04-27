#include "common/UtilityStructs.hpp"

namespace st {


    dll_retrieved_strings_t::dll_retrieved_strings_t() {}

    dll_retrieved_strings_t::~dll_retrieved_strings_t() {
        for (uint32_t i = 0; i < NumNames; ++i) {
            free(Strings[i]);
        }
    }

    dll_retrieved_strings_t::dll_retrieved_strings_t(dll_retrieved_strings_t && other) noexcept : NumNames(std::move(other.NumNames)), Strings(std::move(other.Strings)) {
        other.NumNames = 0;
        other.Strings = nullptr;
    }

    dll_retrieved_strings_t& dll_retrieved_strings_t::operator=(dll_retrieved_strings_t && other) noexcept {
        NumNames = std::move(other.NumNames);
        other.NumNames = 0;
        Strings = std::move(other.Strings);
        other.Strings = nullptr;
        return *this;
    }
    
}