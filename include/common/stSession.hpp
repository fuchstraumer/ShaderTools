#pragma once
#ifndef ST_SESSION_HPP
#define ST_SESSION_HPP
#include "CommonInclude.hpp"
#include "ShaderToolsErrors.hpp"
#include "UtilityStructs.hpp"


namespace st
{
    struct SessionImpl;

     /**
     * @brief A session represents a single compilation or generation session created by API users
     * 
     * It contains the lifetimes of the objects created during the session, and the state of the session: especially
     * the errors generated during the session. This allows us to track the errors and warnings generated during
     * the session, and to report them all at once even if we're threading operations.
     * 
     * @todo Add a way to track warnings as well as errors
     * @todo Allow the session to also configure some aspects of the generation and compilation process
     * @todo Store a date and time in the session, allowing us to associate generated files and data with unique sessions
     * @todo Dump a file of stats about the session, including the number of errors and warnings, and the time taken to generate
     * 
     * @see SessionImpl for further implementation details
     * @ingroup Common
     */
    struct ST_API Session
    {

        Session();
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) noexcept;
        Session& operator=(Session&&) noexcept;

        bool HasErrors() const;
        
        /**
         * @brief Retrieves all error strings from the session
         * @return Collection of error strings generated during this session
         * @note The strings are copied manually, so make sure that dll_retrieved_strings_t is allowed to be destroyed to free this memory
         */
        dll_retrieved_strings_t GetErrorStrings();

        /**
         * @brief Merges errors and state from another session into the root session
         * @param rootSession Target session to receive the merged state
         * @param otherSession Source session to merge from (will be consumed, not valid after this call)
         */
        static void MergeSessions(Session& rootSession, Session&& otherSession);
        
        /**
         * @brief Merges errors and state from another session into the root session implementation
         * @param rootSession Target session implementation to receive the merged state
         * @param otherSession Source session to merge from (will be consumed, not valid after this call)
         */
        static void MergeSessions(SessionImpl* rootSession, Session&& otherSession);

        /**
         * @brief Gets the implementation pointer for internal use
         * @note Users should not be using this function, but making it private would've caused more mess.
         */
        SessionImpl* GetImpl() noexcept;

    private:
        std::unique_ptr<SessionImpl> impl;
    };



}

#endif // ST_SESSION_HPP
