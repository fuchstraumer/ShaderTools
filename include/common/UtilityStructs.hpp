#pragma once
#ifndef SHADER_TOOLS_UTILITY_STRUCTS_HPP
#define SHADER_TOOLS_UTILITY_STRUCTS_HPP
#include "CommonInclude.hpp"

namespace st {

    struct ST_API dll_retrieved_strings_t {
        dll_retrieved_strings_t(const dll_retrieved_strings_t&) = delete;
        dll_retrieved_strings_t& operator=(const dll_retrieved_strings_t&) = delete;
        // Names are retrieved using strdup(), so we need to free the duplicated names once done with them.
        // Use this structure to "buffer" the names, and copy them over.
        // Once this structure exits scope the memory should be cleaned up.
        dll_retrieved_strings_t();
        ~dll_retrieved_strings_t();
        dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept;
        dll_retrieved_strings_t& operator=(dll_retrieved_strings_t&& other) noexcept;
        char** Strings{ nullptr };
        size_t NumNames{ 0 };
    };

    struct engine_environment_callbacks_t {
        std::add_pointer<int()>::type GetScreenSizeX{ nullptr };
        std::add_pointer<int()>::type GetScreenSizeY{ nullptr };
        std::add_pointer<double()>::type GetZNear{ nullptr };
        std::add_pointer<double()>::type GetZFar{ nullptr };
        std::add_pointer<double()>::type GetFOVY{ nullptr };
    };
    
}

#endif //!SHADER_TOOLS_UTILITY_STRUCTS_HPP