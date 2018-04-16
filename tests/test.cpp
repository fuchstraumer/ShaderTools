#include <iostream>
#include <vector>
#include <chrono>
#include "common/ShaderGroup.hpp"
#include "util/FileObserver.hpp"
#include <conio.h>
#include <thread>

int screen_x() {
    return 1920;
}

int screen_y() {
    return 1080;
}

double z_near() {
    return 0.1;
}

double z_far() {
    return 3000.0;
}

double fov_y() {
    return 75.0;
}

int main(int argc, char* argv[]) {
    using namespace st; 
    
    ShaderGroup::RetrievalCallbacks.GetScreenSizeX = &screen_x;
    ShaderGroup::RetrievalCallbacks.GetScreenSizeY = &screen_y;
    ShaderGroup::RetrievalCallbacks.GetZNear = &z_near;
    ShaderGroup::RetrievalCallbacks.GetZFar = &z_far;
    ShaderGroup::RetrievalCallbacks.GetFOVY = &fov_y;

    ShaderGroup group("VolumetricForward", "../fragments/volumetric_forward/VolumetricForward.lua");
    bool exit = false;
    char c;

    std::chrono::system_clock::time_point limiter_a = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point limiter_b = std::chrono::system_clock::now();

    auto limit_frame = [&]() {
        limiter_a = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> work_time = limiter_a - limiter_b;
        if (work_time.count() < 16.0) {
            std::chrono::duration<double, std::milli> delta_ms(16.0 - work_time.count());
            auto delta_ms_dur = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_dur.count()));
        }
        limiter_b = std::chrono::system_clock::now();
    };

    while (!exit) {
        limit_frame();
        auto& obs = FileObserver::GetFileObserver();
        obs.Update();
    }
    std::cerr << "Tests complete.";
}