#include <iostream>
#include <vector>
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../src/util/ShaderPackBinary.hpp"
#include <array>
#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP

static int screen_x() {
    return 1920;
}

static int screen_y() {
    return 1080;
}

static double z_near() {
    return 0.1;
}

static double z_far() {
    return 3000.0;
}

static double fov_y() {
    return 75.0;
}

void VerifyLoadedPack(st::ShaderPackBinary* bin0, st::ShaderPackBinary* bin1) {

}

int main(int argc, char* argv[]) {
    using namespace st; 

    auto& callbacks = ShaderPack::RetrievalCallbacks();
    
    callbacks.GetScreenSizeX = &screen_x;
    callbacks.GetScreenSizeY = &screen_y;
    callbacks.GetZNear = &z_near;
    callbacks.GetZFar = &z_far;
    callbacks.GetFOVY = &fov_y;
    
    //ShaderPack pack("../fragments/volumetric_forward/ShaderPack.lua");

    //ShaderPackBinary* binarization_of_pack = CreateShaderPackBinary(&pack);
    //SaveBinaryToFile(binarization_of_pack, "VolumetricForwardPack.stbin");

    ShaderPackBinary* reloaded_pack = LoadShaderPackBinary("VolumetricForwardPack.stbin");

    ShaderPack binary_loaded_pack(reloaded_pack);

    //DestroyShaderPackBinary(binarization_of_pack);
    DestroyShaderPackBinary(reloaded_pack);

    std::cerr << "Tests complete.\n";
    return 0;
}