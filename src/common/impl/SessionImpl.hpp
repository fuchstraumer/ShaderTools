#pragma once
#ifndef SHADER_TOOLS_SESSION_IMPL_HPP
#define SHADER_TOOLS_SESSION_IMPL_HPP
#include "common/ShaderToolsErrors.hpp"
#include "common/stSession.hpp"
#include <unordered_map>
#include <source_location>

namespace st
{

    struct ReportedError
    {
        ShaderToolsErrorSource source;
        ShaderToolsErrorCode code;
        const char* message;
        std::source_location sourceLocation;
    };

    struct SessionImpl
    {
        SessionImpl(Session& parent) noexcept;

        ~SessionImpl();

        SessionImpl(const SessionImpl&) = delete;
        SessionImpl& operator=(const SessionImpl&) = delete;

        // Adds an error to the session: instance is a pointer to the object reporting the error,
        // used for hashing the error into storage. (since the next two params could occur more than once per session)
        // Message is optional, can be nullptr.
        void AddError(
            const void* instance,
            const ShaderToolsErrorSource source,
            const ShaderToolsErrorCode code,
            const char* message,
            std::source_location source_location = std::source_location::current());

        std::unordered_map<const void*, std::vector<ReportedError>> errors;

        // to keep the API sane and safe for clients, provide parent ref (sobbing_emoji.jpg)
        Session& parent;
    };

}

#endif //!SHADER_TOOLS_SESSION_IMPL_HPP