cmake_minimum_required(VERSION 3.24) # need multiconfig support
project(lodestone LANGUAGES CXX VERSION 0.1.0)

set(SHADER_COOKER_CLIENT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/ShaderLibraryTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/ShaderManifest.hpp")

set(SHADER_COOKER_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/client/src/ShaderLibraryTypes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/src/ShaderManifest.cpp")

set(SHADER_COOKER_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CookerDriver.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CookerErrors.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CookerOptions.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/OutputSink.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/PermutationSpace.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ContentHash.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ContentInterner.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CookedLibrary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/DedupeReport.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderDataSchema.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderLibraryEmitter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderManifestEmitter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SizeExpression.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SlangCompiler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/WgslBindingScanner.hpp")

set(SHADER_COOKER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ContentHash.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CookedLibrary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CookerDriver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/DedupeReport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CookerErrors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CookerOptions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/OutputSink.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PermutationSpace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderDataSchema.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderLibraryEmitter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderManifestEmitter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SizeExpression.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SlangCompiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WgslBindingScanner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(lodestone::cooker ${SHADER_COOKER_SOURCES} ${SHADER_COOKER_HEADERS})
target_compile_features(lodestone::cooker PRIVATE cxx_std_23)
# The engine include path carries shader/ShaderLibraryTypes.hpp and resource/ResourceFlags.hpp. The
# cooker and the rendergraph share that vocabulary, so neither can drift from the other. Neither
# header pulls in Dawn: ResourceFlagConversions.hpp is the only file that names a WebGPU type.
target_include_directories(lodestone::cooker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/slang/include)
target_link_libraries(lodestone::cooker PRIVATE slang magic_enum::magic_enum)
set_target_properties(lodestone::cooker PROPERTIES FOLDER "tools")

source_group("include" FILES ${SHADER_COOKER_HEADERS})
source_group("src" FILES ${SHADER_COOKER_SOURCES})
source_group("client" FILES ${SHADER_COOKER_CLIENT_HEADERS} ${SHADER_COOKER_CLIENT_SOURCES})

copy_slang_dlls_to_target(lodestone::cooker)
