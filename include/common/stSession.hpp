#pragma once
#ifndef ST_SESSION_HPP
#define ST_SESSION_HPP
#include "CommonInclude.hpp"
#include "ShaderToolsErrors.hpp"
#include "UtilityStructs.hpp"

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

    struct ST_API Session
    {

        Session();

        ~Session();

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        Session(Session&&) noexcept;
        Session& operator=(Session&&) noexcept;

        bool HasErrors() const;
        dll_retrieved_strings_t GetErrorStrings();

        static void MergeSessions(Session& rootSession, Session&& otherSession);
        static void MergeSessions(SessionImpl* rootSession, Session&& otherSession);

        SessionImpl* GetImpl() noexcept;

    private:
        std::unique_ptr<SessionImpl> impl;
    };



}

#endif // ST_SESSION_HPP
