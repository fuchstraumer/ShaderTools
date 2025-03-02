#include "SessionImpl.hpp"

namespace st
{
    SessionImpl::SessionImpl(Session& parent) noexcept : parent(parent) {}

    SessionImpl::~SessionImpl()
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

    void SessionImpl::AddError(
        const void* instance,
        const ShaderToolsErrorSource source,
        const ShaderToolsErrorCode code,
        const char* message,
        std::source_location source_location)
    {
        decltype(errors)::iterator foundErrorIter = errors.find(instance);
        if (foundErrorIter == errors.end())
        {
            auto newErrorIter = errors.emplace(instance, std::vector<ReportedError>{});
            foundErrorIter = newErrorIter.first;
        }

        foundErrorIter->second.emplace_back(ReportedError{ source, code, message != nullptr ? strdup(message) : nullptr, source_location });

    }

}