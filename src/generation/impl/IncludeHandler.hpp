#pragma once
#ifndef ST_INCLUDE_HANDLER_HPP
#define ST_INCLUDE_HANDLER_HPP
#include "../../common/impl/SessionImpl.hpp"
#include <shaderc/shaderc.hpp>
#include <vector>
#include <filesystem>

namespace st
{
    class IncludeHandler : public shaderc::CompileOptions::IncluderInterface
    {
    public:
        IncludeHandler(const std::vector<std::filesystem::path>& include_paths, SessionImpl* error_session) noexcept;
        ~IncludeHandler() noexcept = default;
        IncludeHandler(const IncludeHandler&) noexcept = delete;
        IncludeHandler& operator=(const IncludeHandler&) noexcept = delete;
        IncludeHandler(IncludeHandler&&) noexcept = default;
        IncludeHandler& operator=(IncludeHandler&&) noexcept = default;

        shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) final;

        void ReleaseInclude(shaderc_include_result* data) final;

        static void AddLibraryIncludePath(const std::filesystem::path& path) noexcept;
        static void AddLibraryIncludePaths(const std::vector<std::filesystem::path>& paths) noexcept;

    private:
        static std::vector<std::filesystem::path> libaryIncludePaths;
        std::vector<std::filesystem::path> includePaths;
        SessionImpl* errorSession;
    };
}

#endif // ST_INCLUDE_HANDLER_HPP

