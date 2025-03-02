INCLUDE(ExternalProject)
CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
PROJECT(ShaderTools)
# Hate global settings, but this just lets us pack away all those dependencies into one group
# Much better than cluttering up an entire IDE just with our dependencies lol
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

SET(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-headers")
SET(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip installation")
SET(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building the executable and tests along with the library")
SET(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip building tests along with the library")
SET(SPIRV_WERROR OFF CACHE BOOL "Enable error on warning")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-tools")
SET(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip installation")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/glslang")

SET(SHADERC_SKIP_TESTS ON)
SET(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
SET(SHADERC_SPIRV_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-headers")
SET(SHADERC_GLSLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glslang")
SET(SHADERC_SPIRV_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-tools")
SET(SHADERC_SKIP_INSTALL ON CACHE BOOL "Skip installation")
SET(SHADERC_ENABLE_SHARED_CRT ON CACHE BOOL "Use the shared CRT instead of the static CRT")
SET(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Builds glslangValidator and spirv-remap")
SET(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip installation")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/shaderc" EXCLUDE_FROM_ALL)
SET(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build spirv-reflect executable")
SET(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build a SPIRV-Reflect static library")
SET(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "Enable asserts for debugging")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-reflect" EXCLUDE_FROM_ALL)
SET(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Enable contrib stuff in library")
SET(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Enable testing")
SET(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Enable parse tools")
SET(YAML_CPP_INSTALL OFF CACHE BOOL "Enable generation of install target")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp")

OPTION(SHADERTOOLS_PROFILING "Profile key steps in the execution of ShaderTools, and log timing info to the console" OFF)

SET(ST_COMMON_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/CommonInclude.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/ShaderStage.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/ShaderToolsErrors.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/stSession.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/UtilityStructs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/impl/SessionImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/impl/SessionImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/UtilityStructsInternal.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/ShaderStage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/ShaderToolsErrors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/UtilityStructs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/stSession.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/UtilityStructsInternal.cpp")

SET(ST_CORE_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/Shader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/ShaderPack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Shader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ShaderPack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderPackImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderPackImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderImpl.cpp")

SET(ST_GENERATION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/generation/Compiler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/generation/ShaderGenerator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/BuiltIns.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/Compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/ShaderGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/CompilerImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/CompilerImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/IncludeHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/IncludeHandler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderGeneratorImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderGeneratorImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderStageProcessor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderStageProcessor.cpp")

SET(ST_PARSER_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/yamlFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/yamlFile.cpp")

SET(ST_REFLECTION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection/ReflectionStructs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection/ShaderReflector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/ReflectionStructs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/ShaderReflector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/impl/ShaderReflectorImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/impl/ShaderReflectorImpl.cpp")

SET(ST_RESOURCES_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/resources/ResourceGroup.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/resources/ResourceUsage.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/resources/ShaderResource.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceGroup.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceUsage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ShaderResource.cpp")

SET(ST_UTIL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/FilesystemUtils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/FilesystemUtils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ResourceFormats.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ResourceFormats.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderFileTracker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderFileTracker.cpp")

SOURCE_GROUP("common" FILES ${ST_COMMON_SRCS})
SOURCE_GROUP("core" FILES ${ST_CORE_SRCS})
SOURCE_GROUP("generation" FILES ${ST_GENERATION_SRCS})
SOURCE_GROUP("parser" FILES ${ST_PARSER_SRCS})
SOURCE_GROUP("reflection" FILES ${ST_REFLECTION_SRCS})
SOURCE_GROUP("resources" FILES ${ST_RESOURCES_SRCS})
SOURCE_GROUP("util" FILES ${ST_UTIL_SRCS})

FIND_PACKAGE(Vulkan REQUIRED)

OPTION(SHADERTOOLS_BUILD_STATIC "Build as a static library instead of a DLL. Not recommended." OFF)

IF(SHADERTOOLS_BUILD_STATIC) 
    ADD_LIBRARY(ShaderTools STATIC ${ST_COMMON_SRCS} ${ST_UTIL_SRCS} ${ST_GENERATION_SRCS} 
        ${ST_REFLECTION_SRCS} ${ST_CORE_SRCS} ${ST_PARSER_SRCS} ${ST_RESOURCES_SRCS})
    TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE "ST_BUILDING_STATIC")
ELSE()
    ADD_LIBRARY(ShaderTools SHARED ${ST_COMMON_SRCS} ${ST_UTIL_SRCS} ${ST_GENERATION_SRCS} 
        ${ST_REFLECTION_SRCS} ${ST_CORE_SRCS} ${ST_PARSER_SRCS} ${ST_RESOURCES_SRCS})
    TARGET_COMPILE_DEFINITIONS(ShaderTools PUBLIC "$<BUILD_INTERFACE:SHADERTOOLS_BUILD_DLL>" "$<INSTALL_INTERFACE:SHADERTOOLS_DLL>")
ENDIF()

set_target_properties(ShaderTools PROPERTIES FOLDER "ShaderTools")

# Require CXX23 for all compilers
SET_PROPERTY(TARGET ShaderTools PROPERTY CXX_STANDARD 23)

IF(SHADERTOOLS_PROFILING)
    TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE "SHADERTOOLS_PROFILING_ENABLED")
ENDIF()

TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE -DSHADERTOOLS_BUILD_DLL)

TARGET_LINK_LIBRARIES(ShaderTools PRIVATE shaderc spirv-reflect-static yaml-cpp)

TARGET_INCLUDE_DIRECTORIES(ShaderTools PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIRS}")
TARGET_INCLUDE_DIRECTORIES(ShaderTools PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bitsery/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-reflect")

IF(MSVC)
    TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE 
        "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS"
        "_SCL_SECURE_NO_WARNINGS"
        "_SCL_SECURE_NO_DEPRECATE"
        "_CRT_SECURE_NO_WARNINGS"
        "_CRT_SECURE_NO_DEPRECATE"
        "_CRT_NONSTDC_NO_WARNINGS"
        "_CRT_NONSTDC_NO_DEPRECATE"
        "NOMINMAX")
ENDIF()

INSTALL(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ShaderTools/")
INSTALL(TARGETS ShaderTools RUNTIME DESTINATION "bin" LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
INSTALL(TARGETS ShaderTools RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

##
OPTION(SHADERTOOLS_BUILD_TESTS "Build some test executables for shader parsing and compiliation." OFF)
IF(SHADERTOOLS_BUILD_TESTS)
    ADD_EXECUTABLE(ParseTest0 "${CMAKE_CURRENT_SOURCE_DIR}/tests/test.cpp")
    TARGET_COMPILE_DEFINITIONS(ParseTest0 PRIVATE "-DSHADERTOOLS_DLL")
    ADD_DEPENDENCIES(ParseTest0 ShaderTools)
    TARGET_LINK_LIBRARIES(ParseTest0 ShaderTools)
    TARGET_INCLUDE_DIRECTORIES(ParseTest0 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    SET_TARGET_PROPERTIES(ParseTest0 PROPERTIES FOLDER "ShaderTools")
    SET_PROPERTY(TARGET ParseTest0 PROPERTY CXX_STANDARD 23)
ENDIF()