# OceanShaderCompiler — native host tool: Slang -> WGSL -> baked C++ header
# Not included in Emscripten builds; invoke as a pre-build step from the host.
set(SHADER_COOKER_SOURCES
    src/CookerDriver.cpp
    src/CookerErrors.cpp
    src/CookerOptions.cpp
    src/OutputSink.cpp
    src/PermutationSpace.cpp
    src/ShaderDataSchema.cpp
    src/SlangCompiler.cpp
    src/WgslBindingScanner.cpp
    src/main.cpp)

set(SHADER_COOKER_HEADERS
    include/CookerDriver.hpp
    include/CookerErrors.hpp
    include/CookerOptions.hpp
    include/OutputSink.hpp
    include/PermutationSpace.hpp
    include/ShaderDataSchema.hpp
    include/SlangCompiler.hpp
    include/WgslBindingScanner.hpp)

add_executable(OceanShaderCompiler ${SHADER_COOKER_SOURCES} ${SHADER_COOKER_HEADERS})
target_compile_features(OceanShaderCompiler PRIVATE cxx_std_23)
target_include_directories(OceanShaderCompiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/slang/include)
target_link_libraries(OceanShaderCompiler PRIVATE slang magic_enum::magic_enum)
set_target_properties(OceanShaderCompiler PROPERTIES FOLDER "tools")

source_group("include" FILES ${SHADER_COOKER_HEADERS})
source_group("src" FILES ${SHADER_COOKER_SOURCES})

copy_slang_dlls_to_target(OceanShaderCompiler)
