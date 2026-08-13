# OceanShaderCompiler — native host tool: Slang -> WGSL -> baked C++ header
# Not included in Emscripten builds; invoke as a pre-build step from the host.
set(SHADER_COOKER_SOURCES
    src/ContentHash.cpp
    src/CookedLibrary.cpp
    src/CookerDriver.cpp
    src/DedupeReport.cpp
    src/CookerErrors.cpp
    src/CookerOptions.cpp
    src/OutputSink.cpp
    src/PermutationSpace.cpp
    src/ShaderDataSchema.cpp
    src/ShaderLibraryEmitter.cpp
    src/SizeExpression.cpp
    src/SlangCompiler.cpp
    src/WgslBindingScanner.cpp
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/shader/ShaderLibraryTypes.cpp)

set(SHADER_COOKER_HEADERS
    include/CookerDriver.hpp
    include/CookerErrors.hpp
    include/CookerOptions.hpp
    include/OutputSink.hpp
    include/PermutationSpace.hpp
    include/ContentHash.hpp
    include/ContentInterner.hpp
    include/CookedLibrary.hpp
    include/DedupeReport.hpp
    include/ShaderDataSchema.hpp
    include/ShaderLibraryEmitter.hpp
    include/SizeExpression.hpp
    include/SlangCompiler.hpp
    include/WgslBindingScanner.hpp)

add_executable(OceanShaderCompiler ${SHADER_COOKER_SOURCES} ${SHADER_COOKER_HEADERS})
target_compile_features(OceanShaderCompiler PRIVATE cxx_std_23)
# The engine include path carries shader/ShaderLibraryTypes.hpp and resource/ResourceFlags.hpp. The
# cooker and the rendergraph share that vocabulary, so neither can drift from the other. Neither
# header pulls in Dawn: ResourceFlagConversions.hpp is the only file that names a WebGPU type.
target_include_directories(OceanShaderCompiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/slang/include)
target_link_libraries(OceanShaderCompiler PRIVATE slang magic_enum::magic_enum)
set_target_properties(OceanShaderCompiler PROPERTIES FOLDER "tools")

source_group("include" FILES ${SHADER_COOKER_HEADERS})
source_group("src" FILES ${SHADER_COOKER_SOURCES})

copy_slang_dlls_to_target(OceanShaderCompiler)
