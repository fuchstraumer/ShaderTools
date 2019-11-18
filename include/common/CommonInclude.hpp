#pragma once

// Declare DLL interface 
#if defined(__GNUC__) 
#define EXPORT __attribute__((visiblity("default"))) 
#define IMPORT 
#else 
#define EXPORT __declspec(dllexport) 
#define IMPORT __declspec(dllimport) 
#endif 

#ifdef SHADERTOOLS_DLL 
#define ST_API IMPORT 
#elif defined(SHADERTOOLS_BUILD_DLL) 
#define ST_API EXPORT 
#else 
#define ST_API 
#endif 

#ifdef _MSC_VER 
// Disable warning about dll interface for impl members.
#pragma warning(disable: 4251 )
#endif

#include <vulkan/vulkan_core.h>
#include <cstdint>
#include <memory>
