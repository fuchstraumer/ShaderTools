#pragma once
#ifndef SHADER_TOOLS_FILE_OBSERVER_HPP
#define SHADER_TOOLS_FILE_OBSERVER_HPP
#include "common/CommonInclude.hpp"
#include "Delegate.hpp"

namespace st {

    // The const char* field will be called with the absolute file path that was modified.
    using watch_event_t = delegate_t<void(const char*)>;

    class FileObserverImpl;

    class ST_API FileObserver {
        FileObserver(const FileObserver&) = delete;
        FileObserver& operator=(const FileObserver&) = delete;
    public:

        FileObserver();
        ~FileObserver();
        static FileObserver& GetFileObserver();

        void SetWatchingStatus(bool enable_disable);
        bool IsWatching() const noexcept;
        void SetUpdateInterval(const double& update_interval_in_seconds);
        double GetUpdateInterval() const noexcept;

        void WatchFile(const char* fname, watch_event_t callback_fn);
        void Unwatch(const char* fname);
        void TouchFile(const char* fname);

        void Update();

    private:
        
        std::unique_ptr<FileObserverImpl> impl;

    };
}

#endif //!SHADER_TOOLS_FILE_OBSERVER_HPP