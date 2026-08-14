#pragma once
#ifndef VELOX_SHADER_COOKER_MANIFEST_EMITTER_HPP
#define VELOX_SHADER_COOKER_MANIFEST_EMITTER_HPP
#include "CookedLibrary.hpp"
#include <string>

/**
 * Writes one CookedModule as the binary manifest that `include/shader/ShaderManifest.hpp` reads.
 *
 * The manifest and the generated C++ carry the same tables from the same frozen model. The C++ form
 * compiles into the program. The manifest form arrives as bytes, so a live cooker can replace it while
 * the program runs.
 *
 * Every section starts on an 8-byte boundary, because the binding records and the axis values hold
 * 64-bit fields. The reader maps the bytes in place and does not copy them.
 */
namespace velox::cooker
{

std::string EmitShaderManifest(const CookedModule& module);

std::string MakeManifestFileName(std::string_view module_name);

/** Reads the manifest back and compares every entry point of every variant against the module it came
 * from. It checks the source bytes, the workgroup size, and each binding field.
 *
 * This runs on every cook. A manifest that says something different from the generated C++ is the one
 * failure this format could hide, so the check is not optional. */
CookResult<void> VerifyManifestRoundTrip(const CookedModule& module, const std::string& manifest_bytes);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_MANIFEST_EMITTER_HPP
