#pragma once
#ifndef ST_RESOURCE_FILE_HPP
#define ST_RESOURCE_FILE_HPP
#include "CommonInclude.hpp"
#include "sol.hpp"

namespace st {

    struct engine_environment_callbacks_t {
        using screen_size_callback_t = std::add_pointer<void(int*,int*)>::type;
        using z_near_far_callback_t = std::add_pointer<void(float*,float*)>::type;
        using fov_y_callback_t = std::add_pointer<void(float*)>::type;
        screen_size_callback_t GetScreenSize;
        z_near_far_callback_t GetZPlanes;
        fov_y_callback_t GetFOVY;
    };

    class ResourceFile {
    public:
        ResourceFile(const char* fname, sol::state& lstate);
        
        engine_environment_callbacks_t RetrievalCallbacks;
    private:
        sol::environment luaEnvironment;
        sol::state& lState;
    };

}

#endif //!ST_RESOURCE_FILE_HPP