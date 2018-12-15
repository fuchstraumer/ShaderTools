INCLUDE(ExternalProject)
CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
PROJECT(ShaderTools)
# Hate global settings, but this just lets us pack away all those dependencies into one group
# Much better than cluttering up an entire IDE just with our dependencies lol
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

SET(SHADERC_SKIP_TESTS ON)
SET(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
SET(SHADERC_SPIRV_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-headers")
SET(SHADERC_GLSLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glslang")
SET(SHADERC_SPIRV_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-tools")
SET(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building the executable and tests along with the library")
SET(SPIRV_WERROR OFF CACHE BOOL "Enable error on warning")
SET(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Builds glslangValidator and spirv-remap")
SET(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip installation")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/shaderc" EXCLUDE_FROM_ALL)
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-cross" EXCLUDE_FROM_ALL)
SET(SKIP_LUA_WRAPPER ON CACHE BOOL "Do not build and install Lua executable wrappers.")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/third_party/luajit" EXCLUDE_FROM_ALL)

OPTION(SHADERTOOLS_PROFILING "Profile key steps in the execution of ShaderTools, and log timing info to the console" OFF)

SET(ST_COMMON_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/CommonInclude.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/ShaderStage.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/UtilityStructs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/ShaderStage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/UtilityStructs.cpp"
)

SET(ST_CORE_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/ResourceGroup.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/ResourceUsage.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/Shader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/ShaderPack.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/core/ShaderResource.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ResourceGroup.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ResourceUsage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Shader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ShaderPack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ShaderResource.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/shader_pack_file.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderPackImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderPackImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/impl/ShaderImpl.cpp"
)

SET(ST_GENERATION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/generation/Compiler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/generation/ShaderGenerator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/BuiltIns.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/Compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/ShaderGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/CompilerImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/CompilerImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderGeneratorImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/generation/impl/ShaderGeneratorImpl.cpp"
)

SET(ST_LUA_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/LuaEnvironment.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/LuaEnvironment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/ResourceFile.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/ResourceFile.cpp"
)

SET(ST_REFLECTION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection/ReflectionStructs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection/ShaderReflector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/ReflectionStructs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/ShaderReflector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/impl/ShaderReflectorImpl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/reflection/impl/ShaderReflectorImpl.cpp"
)

SET(ST_UTIL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/FilesystemUtils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/FilesystemUtils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ResourceFormats.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ResourceFormats.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderFileTracker.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderFileTracker.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderPackBinary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/ShaderPackBinary.cpp"
)

SOURCE_GROUP("common" FILES ${ST_COMMON_SRCS})
SOURCE_GROUP("util" FILES ${ST_UTIL_SRCS})
SOURCE_GROUP("generation" FILES ${ST_GENERATION_SRCS})
SOURCE_GROUP("reflection" FILES ${ST_REFLECTION_SRCS})
SOURCE_GROUP("core" FILES ${ST_CORE_SRCS})
SOURCE_GROUP("lua" FILES ${ST_LUA_SRCS})

FIND_PACKAGE(Vulkan REQUIRED)

OPTION(BUILD_STATIC "Build as a static library instead of a DLL. Not recommended." OFF)

IF(BUILD_STATIC) 
    ADD_LIBRARY(ShaderTools STATIC ${ST_COMMON_SRCS} ${ST_UTIL_SRCS} ${ST_GENERATION_SRCS} 
        ${ST_REFLECTION_SRCS} ${ST_LUA_SRCS} ${ST_CORE_SRCS}  
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/easyloggingpp/src/easylogging++.cc")
    # Used to avoid initializing easyloggingpp when built as static library. Causes 
    # duplicate symbol errors otherwise
    TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE "ST_BUILDING_STATIC")
ELSE()
    ADD_LIBRARY(ShaderTools SHARED ${ST_COMMON_SRCS} ${ST_UTIL_SRCS} ${ST_GENERATION_SRCS} 
        ${ST_REFLECTION_SRCS} ${ST_LUA_SRCS} ${ST_CORE_SRCS} 
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/easyloggingpp/src/easylogging++.cc")
    TARGET_COMPILE_DEFINITIONS(ShaderTools PUBLIC "$<BUILD_INTERFACE:SHADERTOOLS_BUILD_DLL>" "$<INSTALL_INTERFACE:SHADERTOOLS_DLL>")
ENDIF()

# Require CXX17 for all compilers
SET_PROPERTY(TARGET ShaderTools PROPERTY CXX_STANDARD 17)

IF(MSVC)
    # Set some MSVC-specific compiler options that we need
    TARGET_COMPILE_OPTIONS(ShaderTools PRIVATE "/bigobj" "/MP")
ENDIF()

IF(SHADERTOOLS_PROFILING)
    TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE "SHADERTOOLS_PROFILING_ENABLED")
ENDIF()

TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE -DSHADERTOOLS_BUILD_DLL)

TARGET_LINK_LIBRARIES(ShaderTools PRIVATE shaderc spirv-cross-core spirv-cross-glsl spirv-cross-cpp)
TARGET_LINK_LIBRARIES(ShaderTools PRIVATE liblua)

TARGET_INCLUDE_DIRECTORIES(ShaderTools PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIRS}")
TARGET_INCLUDE_DIRECTORIES(ShaderTools PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/luajit/src" 
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/luabridge/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bitsery/include/"
)
    
TARGET_COMPILE_DEFINITIONS(ShaderTools PRIVATE 
    "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS"
    "_SCL_SECURE_NO_WARNINGS"
    "_SCL_SECURE_NO_DEPRECATE"
    "_CRT_SECURE_NO_WARNINGS"
    "_CRT_SECURE_NO_DEPRECATE"
    "_CRT_NONSTDC_NO_WARNINGS"
    "_CRT_NONSTDC_NO_DEPRECATE"
    "NOMINMAX"
)

# Group all our dependencies together so they don't clutter up the IDE, or the IDE of users
SET_TARGET_PROPERTIES(
    liblua shaderc spirv-cross-core spirv-cross-glsl spirv-cross-cpp glslang
    HLSL buildvm minilua shaderc_util OGLCompiler OSDependent SPIRV SPIRV-Tools SPIRV-Tools-opt
    PROPERTIES FOLDER "ShaderTools Dependencies")

## 
INSTALL(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ShaderTools/")
INSTALL(TARGETS ShaderTools RUNTIME DESTINATION "bin" LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
INSTALL(TARGETS ShaderTools RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/")

##
OPTION(BUILD_TESTS "Build some test executables for shader parsing and compiliation." OFF)
IF(BUILD_TESTS)
    ADD_EXECUTABLE(ParseTest0 "${CMAKE_CURRENT_SOURCE_DIR}/tests/test.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/easyloggingpp/src/easylogging++.cc")
    TARGET_COMPILE_DEFINITIONS(ParseTest0 PRIVATE "-DSHADERTOOLS_DLL")
    ADD_DEPENDENCIES(ParseTest0 ShaderTools)
    TARGET_LINK_LIBRARIES(ParseTest0 ShaderTools)
    TARGET_INCLUDE_DIRECTORIES(ParseTest0 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/luajit/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/luabridge/Source" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/easyloggingpp/src")
ENDIF()
