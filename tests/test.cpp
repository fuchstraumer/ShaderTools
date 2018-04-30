#include <iostream>
#include <vector>
#include "core/ShaderGroup.hpp"
#include "core/ShaderPack.hpp"
#include <array>
#include "easyloggingpp/src/easylogging++.h"
INITIALIZE_EASYLOGGINGPP
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
    
    ShaderPack pack("../fragments/volumetric_forward/ShaderPack.lua");

    std::cerr << "Tests complete.";
}