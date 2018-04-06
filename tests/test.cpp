#include <map>
#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include "Compiler.hpp"
#include "BindingGenerator.hpp"
#include "ShaderGenerator.hpp"
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

void CompiliationTest() {
    const std::vector<std::string> args{ "../tests/Clustered.vert", "../tests/Clustered.frag" };
    using namespace st;
    ShaderCompiler compiler;
    BindingGenerator parser;
    for(const auto& path : args) {
        Shader handle = compiler.Compile(path.c_str());
        fs::path shader_path(path);
        const std::string fname = std::string("compiled_") + shader_path.filename().string();
        compiler.SaveBinaryBackToText(handle, fname.c_str());
        parser.ParseBinary(handle);        
    }
    parser.SaveToJSON("out.json");
}

void GenerationTest() {
    const std::string fname{ "MergeSort" };
    std::string ComputeShaderTest{ "../fragments/volumetric_forward/compute/" };
    ComputeShaderTest += fname + ".comp";
    using namespace st;
    ShaderGenerator gen(VK_SHADER_STAGE_COMPUTE_BIT);
    std::array<const char*, 1> IncludePaths{ "../fragments/volumetric_forward" };
    gen.AddResources("../fragments/volumetric_forward/Resources.glsl");
    gen.AddBody(ComputeShaderTest.c_str(), 1, IncludePaths.data());
    size_t len = 0;
    gen.GetFullSource(&len, nullptr);
    std::vector<char> generated_src(len);
    gen.GetFullSource(&len, generated_src.data());
    std::string src_string{ generated_src.cbegin(), generated_src.cend() };
    std::ofstream output_src(fname + "_Generated.glsl");
    if (!output_src.is_open()) {
        throw std::runtime_error("Couldn't open output for generated glsl.");
    }
    output_src << src_string;
    output_src.flush();
    output_src.close();

    ShaderCompiler compiler;
    Shader handle = compiler.Compile(fname.c_str(), src_string.c_str(), src_string.size(), VK_SHADER_STAGE_COMPUTE_BIT);
    std::string compiled_fname{ fname + "_Compiled.glsl" };
    compiler.SaveBinaryBackToText(handle, compiled_fname.c_str());

    BindingGenerator parser;
    parser.ParseBinary(handle);
    parser.SaveToJSON(std::string(fname + ".json").c_str());

}

int main(int argc, char* argv[]) {
    //CompiliationTest();
    GenerationTest(); 
}