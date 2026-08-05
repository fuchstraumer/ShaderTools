# OceanShaderCompiler — native host tool: Slang -> WGSL -> baked C++ header
# Not included in Emscripten builds; invoke as a pre-build step from the host.
add_executable(OceanShaderCompiler shader_compiler.cpp)
target_compile_features(OceanShaderCompiler PRIVATE cxx_std_23)
target_include_directories(OceanShaderCompiler PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/slang/include)
target_link_libraries(OceanShaderCompiler PRIVATE slang)
set_target_properties(OceanShaderCompiler PROPERTIES FOLDER "tools")
copy_slang_dlls_to_target(OceanShaderCompiler)
