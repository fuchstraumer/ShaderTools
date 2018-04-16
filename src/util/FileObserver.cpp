#include "util/FileObserver.hpp"
#include <vector>
#include <experimental/filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <chrono>
#include <algorithm>

namespace st {

    /*
        Pretty much just the file watcher from Cinder:
        https://github.com/cinder/Cinder/blob/master/src/cinder/FileWatcher.cpp
    */

    namespace fs = std::experimental::filesystem;

    struct QueryLimiter {
        QueryLimiter() : LimiterA(std::chrono::system_clock::now()), LimiterB(std::chrono::system_clock::now()) {}

        void SleepForInterval(const double& interval);

        std::chrono::system_clock::time_point LimiterA;
        std::chrono::system_clock::time_point LimiterB;
    };

    struct ObserverItem {
        ObserverItem(const std::experimental::filesystem::path& _path, const std::experimental::filesystem::file_time_type& _mod_time,
            bool enabled, watch_event_t _signal);
        ~ObserverItem();

        void EmitSignal();

        std::experimental::filesystem::path Path;
        std::experimental::filesystem::file_time_type Timestamp;
        bool Enabled{ true };
        bool DispatchSignal{ false };
        watch_event_t Signal;
    };
    
    class FileObserverImpl {
    public:

        FileObserverImpl();
        ~FileObserverImpl();

        void watchFile(const char* fname, watch_event_t callback_fn);
        void unwatch(const char* fname);
        void touch(const char* fname);

        void start();
        void stop();
        void update();
        void threadEntry();

        std::list<std::unique_ptr<ObserverItem>> items;
        std::unique_ptr<std::thread> watchThread;
        mutable std::recursive_mutex guardMutex;
        std::atomic<double> updateInterval{ 1000.0 };
        std::atomic<bool> endThread{ false };
        std::atomic<bool> watchEnabled{ true };
        QueryLimiter limiter;

    };

    void QueryLimiter::SleepForInterval(const double& interval)  {
        LimiterA = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> work_time = LimiterA - LimiterB;
        if (work_time.count() < interval) {
            std::chrono::duration<double, std::milli> delta_ms(interval - work_time.count());
            auto delta_mus_dur = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delta_mus_dur.count()));
        }
        LimiterB = std::chrono::system_clock::now();
    }

    ObserverItem::ObserverItem(const std::experimental::filesystem::path& _path, const std::experimental::filesystem::file_time_type& _mod_time,
            bool enabled, watch_event_t _signal) : Path(_path), Timestamp(_mod_time), Enabled(enabled), Signal(_signal) {}

    ObserverItem::~ObserverItem() {}

    void ObserverItem::EmitSignal() {
        const std::string path_str = Path.string();
        Signal(path_str.c_str());
        DispatchSignal = false;
    }

    FileObserverImpl::FileObserverImpl() {
        start();
    }

    FileObserverImpl::~FileObserverImpl() {
        stop();
    }

    void FileObserverImpl::watchFile(const char* fname, watch_event_t callback_fn) {
        const fs::path file_path = fs::absolute(fs::path(fname));
        if (!fs::exists(file_path)) {
            throw std::runtime_error("Tried to watch nonexistent file.");
        }
        std::lock_guard<std::recursive_mutex> insertionGuard(guardMutex);
        items.emplace_back(std::make_unique<ObserverItem>(file_path, fs::last_write_time(file_path), true, callback_fn));
    }

    void FileObserverImpl::unwatch(const char* fname) {
        const fs::path fname_path = fs::absolute(fs::path(fname));
        auto iter = std::find_if(items.cbegin(), items.cend(), [fname_path](const std::unique_ptr<ObserverItem>& item){ 
            return item->Path == fname_path;
        });

        if (iter != items.cend()) {
            std::lock_guard<std::recursive_mutex> eraseGuard(guardMutex);
            items.erase(iter);
        }
    }

    void FileObserverImpl::touch(const char * fname) {
        const fs::path file_path = fs::absolute(fs::path(fname));
        auto iter = std::find_if(items.begin(), items.end(), [file_path](const std::unique_ptr<ObserverItem>& item) {
            return item->Path == file_path;
        });

        if (iter != items.end()) {
            std::lock_guard<std::recursive_mutex> touchGuard(guardMutex);
            (*iter)->Timestamp = fs::file_time_type(std::chrono::system_clock::now());
        }
    }

    void FileObserverImpl::start() {
        if (!watchThread->joinable()) {
            endThread = false;
            watchThread = std::make_unique<std::thread>(std::bind(&FileObserverImpl::threadEntry, this));
        }
    }

    void FileObserverImpl::stop() {
        endThread = true;
        if (watchThread->joinable()) {
            watchThread->join();
        }
    }

    void FileObserverImpl::update() {
        std::unique_lock<std::recursive_mutex> main_lock(guardMutex, std::try_to_lock);
        if (!main_lock.owns_lock()) {
            return;
        }

        for (auto& watch_item : items) {

            if (!watch_item->DispatchSignal) {
                break;
            }

            watch_item->EmitSignal();

        }
    }

    void FileObserverImpl::threadEntry() {
        while (!endThread) {
            {
                std::lock_guard<std::recursive_mutex> updateGuard(guardMutex);

                for (auto iter = items.begin(); iter != items.end(); ++iter) {
                    auto& watch = *iter;
                    if (watch->Enabled) {
                        auto mod_time = fs::last_write_time(watch->Path);
                        if (mod_time != watch->Timestamp) {
                            watch->DispatchSignal = true;
                            if (iter != items.begin()) {
                                items.splice(items.begin(), items, iter);
                            }
                            watch->Timestamp = mod_time;
                        }
                    }
                }

            }
            
            limiter.SleepForInterval(updateInterval);
        }
    }

    FileObserver::FileObserver() : impl(std::make_unique<FileObserverImpl>()) {}

    FileObserver::~FileObserver() {}

    FileObserver& FileObserver::GetFileObserver() {
        static FileObserver observer;
        return observer;
    }

    void FileObserver::SetWatchingStatus(bool enable_disable) {
        if (impl->watchEnabled.exchange(enable_disable)) {
            enable_disable ? impl->start() : impl->stop();
        }
    }

    bool FileObserver::IsWatching() const noexcept {
        return impl->watchEnabled;
    }

    void FileObserver::SetUpdateInterval(const double& interval) {
        impl->updateInterval = interval;
    }

    double FileObserver::GetUpdateInterval() const noexcept {
        return impl->updateInterval;
    }

    void FileObserver::WatchFile(const char* fname, watch_event_t callback_fn) {
        impl->watchFile(fname, callback_fn);
    }

    void FileObserver::Unwatch(const char* fname) {
        impl->unwatch(fname);
    }

    void FileObserver::Update() {
        impl->update();
    }
    
    void FileObserver::TouchFile(const char* fname) {
        impl->touch(fname);
    }

}
