#include <iostream>
#include <vector>
#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "../src/util/ShaderFileTracker.hpp"
#include <array>

int main(int argc, char* argv[])
{
    using namespace st; 
    
    for (size_t i = 0; i < 100; ++i)
    {

        std::chrono::high_resolution_clock::time_point beforeExec;
        beforeExec = std::chrono::high_resolution_clock::now();
        ShaderPack pack("../fragments/volumetric_forward/volumetric_forward.yaml");
        std::chrono::duration<double, std::milli> work_time = std::chrono::high_resolution_clock::now() - beforeExec;
        std::cout << "Conventional creation of ShaderPack took: " << work_time.count() << "ms\n";

        DumpProgramStateToCache();
        ClearProgramState();

    }

    std::cout << "Tests complete.\n";
    return 0;
}