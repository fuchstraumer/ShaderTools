#include <map>
#include <iostream>
#include <vector>
#include "Compiler.hpp"
#include "BindingGenerator.hpp"
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::vector<uint32_t>> binaries;
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