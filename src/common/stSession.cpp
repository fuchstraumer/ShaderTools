#include "common/stSession.hpp"
#include "impl/SessionImpl.hpp"
#include "common/ShaderToolsErrors.hpp"
#include "../common/impl/SessionImpl.hpp"

#include <vector>
#include <mutex>
#include <string>
#include <string_view>
#include <format>

namespace st
{

    static std::mutex sessionMergeMutex;

    Session::Session() : impl{ std::make_unique<SessionImpl>(*this) }
    {}

    Session::~Session()
    {}

    Session::Session(Session&& other) noexcept : impl(std::move(other.impl))
    {}

    Session& Session::operator=(Session&& other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }

    bool Session::HasErrors() const
    {
        return impl->errors.size() > 0;
    }

    dll_retrieved_strings_t Session::GetErrorStrings()
    {
        if (impl->errors.empty())
        {
            return dll_retrieved_strings_t{};
        }

        size_t numStringsTotal = 0;
        for (auto& [pointer, errors] : impl->errors)
        {
            numStringsTotal += errors.size();
        }

        dll_retrieved_strings_t strings;
        strings.SetNumStrings(numStringsTotal);

        size_t currErrorIdx = 0u;
        for (auto& [pointer, errors] : impl->errors)
        {
            for (auto& error : errors)
            {

                std::string_view errorCodeStr = ErrorCodeToText(error.code);
                std::string_view errorSourceStr = ErrorSourceToText(error.source);
                std::string errorHeaderStr = std::format(
                    "Error code: {} | Error Source: {} \n", errorCodeStr, errorSourceStr);

                std::string_view function_name_str = error.sourceLocation.function_name();
                size_t end_of_name_idx = function_name_str.find_first_of('(');
                if (end_of_name_idx != std::string_view::npos)
                {
                    function_name_str.remove_suffix(function_name_str.size() - end_of_name_idx);
                }

                std::string locationStr = std::format(
                    "File: {} | Line: {} | Function: {} \n",
                    error.sourceLocation.file_name(),
                    std::to_string(error.sourceLocation.line()),
                    function_name_str);

                std::string messageStr = error.message != nullptr ? error.message : "No message provided";
                messageStr += "\n";

                std::string fullErrorStr = errorHeaderStr + locationStr + messageStr;
                strings.Strings[currErrorIdx] = strdup(fullErrorStr.c_str());

                ++currErrorIdx;
            }
        }

        return strings;
    }

    void Session::MergeSessions(Session& rootSession, Session&& otherSession)
    {
        MergeSessions(rootSession.GetImpl(), std::move(otherSession));
    }


    void Session::MergeSessions(SessionImpl* rootSession, Session&& otherSession)
    {
        if (otherSession.HasErrors())
        {
            std::lock_guard<std::mutex> lock(sessionMergeMutex);
            auto& otherErrors = otherSession.impl->errors;
            // reserve memory for the new errors
            rootSession->errors.reserve(rootSession->errors.size() + otherErrors.size());
            // Other session is an rvalue, so we can move it's data into the root session and avoid a copy (please don't change this)
            rootSession->errors.insert(otherErrors.begin(), otherErrors.end());
            otherSession.impl->errors.clear();
        }
    }

    SessionImpl* Session::GetImpl() noexcept
    {
        return impl.get();
    }

}