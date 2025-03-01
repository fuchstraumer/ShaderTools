#include "core/Shader.hpp"
#include "core/ShaderPack.hpp"
#include "common/stSession.hpp"

#include <iostream>
#include <vector>
#include <array>
#include <chrono>

int main(int argc, char* argv[])
{
    using namespace st; 

    
    for (size_t i = 0; i < 100; ++i)
    {
        Session session;
        std::chrono::high_resolution_clock::time_point beforeExec;
        beforeExec = std::chrono::high_resolution_clock::now();
        ShaderPack pack("../fragments/volumetric_forward/volumetric_forward.yaml", session);
        std::chrono::duration<double, std::milli> work_time = std::chrono::high_resolution_clock::now() - beforeExec;
        std::cout << "Conventional creation of ShaderPack took: " << work_time.count() << "ms\n";

        if (session.HasErrors())
        {
            dll_retrieved_strings_t error_strings = session.GetErrorStrings();
            for (size_t i = 0; i < error_strings.NumStrings; ++i)
            {
                std::cerr << error_strings[i] << "\n";
            }
        }

        ClearAllInternalStorage();
    }

    std::cout << "Tests complete.\n";
    return 0;
}