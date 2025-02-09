#pragma once
#ifndef ST_SESSION_HPP
#define ST_SESSION_HPP
#include "CommonInclude.hpp"
#include "ShaderToolsErrors.hpp"

// A session is a single compiliation or generation session, created by users of the API.
// It contains the lifetimes of the objects created during the session, and the state of the session: especially
// the errors generated during the session. This allows us to track the errors and warnings generated during
// the session, and to report them all at once even if we're threading operations.
// TODO:
// - Add a way to track warnings as well as errors
// - Allow the session to also configure some aspects of the generation and compilation process
// - Store a date and time in the session, allowing us to associate generated files and data with unique sessions
// - Dump a file of stats about the session, including the number of errors and warnings, and the time taken to generate

namespace st
{
    struct SessionImpl;

    struct ST_API ReportedError
    {
        ShaderToolsErrorSource source;
        ShaderToolsErrorCode code;
        const char* message;
    };

    struct ST_API Session
    {

        Session();

        ~Session();

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        Session(Session&&) noexcept;
        Session& operator=(Session&&) noexcept;
        
        // Adds an error to the session: instance is a pointer to the object reporting the error,
        // used for hashing the error into storage. (since the next two params could occur more than once per session)
        // Message is optional, can be nullptr.
        void AddError(
            const void* instance,
            const ShaderToolsErrorSource source,
            const ShaderToolsErrorCode code,
            const char* message);

        bool HasErrors() const;

        // Yay, DLL stuff! Have to use pointers and call it twice, user allocates memory for the errors after first call
        void RetrieveErrors(size_t* numErrors, ReportedError* errors);

        static void MergeSessions(Session& rootSession, Session&& otherSession);

    private:
        std::unique_ptr<SessionImpl> impl;
    };



}

#endif // ST_SESSION_HPP
