#include "stSession.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>

namespace st
{

    static std::mutex sessionMergeMutex;

    struct SessionImpl
    {
        SessionImpl();

        ~SessionImpl()
        {
            for (auto& [instance, errors] : errors)
            {
                for (auto& error : errors)
                {
                    // Each error message is allocated via strdup, so we need to free it
                    if (error.message != nullptr)
                    {
                        free((void*)error.message);
                    }
                }
            }
        }
        
        SessionImpl(const SessionImpl&) = delete;
        SessionImpl& operator=(const SessionImpl&) = delete;

        void AddError(
            const void* instance,
            const ShaderToolsErrorSource source,
            const ShaderToolsErrorCode code,
            const char* message)

        {
            decltype(errors)::iterator foundErrorIter = errors.find(instance);
            if (foundErrorIter == errors.end())
            {
                auto newErrorIter = errors.emplace(instance, std::vector<ReportedError>{});
                foundErrorIter = newErrorIter.first;
            }

            foundErrorIter->second.emplace_back(ReportedError{ source, code, message != nullptr ? strdup(message) : nullptr });

        }

        std::unordered_map<const void*, std::vector<ReportedError>> errors;

    };


    Session::Session() : impl{ std::make_unique<SessionImpl>() }
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

    void Session::RetrieveErrors(size_t* numErrors, ReportedError* errors)
    {
        if (numErrors == nullptr || errors == nullptr)
        {

            return;
        }

        *numErrors = impl->errors.size();
        if (errors != nullptr)
        {
            // I LOVE DLLs! We have to copy the reported errors into our local contiguous array before we can copy it back to the user
            // because the user's array may not be contiguous.
            std::vector<ReportedError> localErrors;
            for (const auto& [instance, reportedErrors] : impl->errors)
            {
                localErrors.insert(localErrors.end(), reportedErrors.begin(), reportedErrors.end());
            }

            // NOW we can copy the errors back to the user's array
            std::copy(localErrors.begin(), localErrors.end(), errors);
        }
        
    }
    
    void Session::MergeSessions(Session& rootSession, Session&& otherSession)
    {
        if (otherSession.HasErrors())
        {
            std::lock_guard<std::mutex> lock(sessionMergeMutex);
            auto& otherErrors = otherSession.impl->errors;
            // reserve memory for the new errors
            rootSession.impl->errors.reserve(rootSession.impl->errors.size() + otherErrors.size());
            // Other session is an rvalue, so we can move it's data into the root session and avoid a copy (please don't change this)
            rootSession.impl->errors.insert(otherErrors.begin(), otherErrors.end());
            otherSession.impl->errors.clear();
        }
    }


}