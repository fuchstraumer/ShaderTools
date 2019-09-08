#include <iostream>
#include <vector>
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../src/util/ShaderPackBinary.hpp"
#include "../src/util/ShaderFileTracker.hpp"
#include <array>
#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP

void VerifyLoadedPack(st::ShaderPackBinary* bin0, st::ShaderPackBinary* bin1) {

}

int main(int argc, char* argv[]) {
    using namespace st; 
    
    for (size_t i = 0; i < 4; ++i) {

        std::chrono::high_resolution_clock::time_point beforeExec;
        beforeExec = std::chrono::high_resolution_clock::now();
        ShaderPack pack("../fragments/volumetric_forward/volumetric_forward.yaml");
        std::chrono::duration<double, std::milli> work_time = std::chrono::high_resolution_clock::now() - beforeExec;
        LOG(INFO) << "Conventional creation of ShaderPack took: " << work_time.count() << "ms";
        ShaderPackBinary* binarization_of_pack = CreateShaderPackBinary(&pack);
        SaveBinaryToFile(binarization_of_pack, "VolumetricForwardPack.stbin");

        ClearProgramState();

        ShaderPackBinary* reloaded_pack = LoadShaderPackBinary("VolumetricForwardPack.stbin");
        beforeExec = std::chrono::high_resolution_clock::now();
        ShaderPack binary_loaded_pack(reloaded_pack);
        work_time = std::chrono::high_resolution_clock::now() - beforeExec;
        LOG(INFO) << "Creation of ShaderPack using saved binary took: " << work_time.count() << "ms";

        DestroyShaderPackBinary(binarization_of_pack);
        DestroyShaderPackBinary(reloaded_pack);
    }

    std::cerr << "Tests complete.\n";
    return 0;
}