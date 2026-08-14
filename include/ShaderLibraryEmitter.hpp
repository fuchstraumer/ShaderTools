#pragma once
#ifndef VELOX_SHADER_COOKER_SHADER_LIBRARY_EMITTER_HPP
#define VELOX_SHADER_COOKER_SHADER_LIBRARY_EMITTER_HPP
#include "CookedLibrary.hpp"
#include <string>

/** Turns the frozen library into C++.
 *
 * Two artifacts come out. The header carries identity and lookup only: the id enums, one permutation
 * struct for each module, `Canonicalize`, `VariantIndex`, and the accessor declarations. It holds no
 * shader text, so a demo that names a shader does not recompile when the text changes.
 *
 * One source file for each module carries the bulk: the WGSL literals, the binding tables, and the
 * accessor definitions.
 *
 * The rendergraph includes shader/ShaderLibraryTypes.hpp, never this output. Only the code that names
 * a shader includes the generated header.
 */
namespace velox::cooker
{

/** MSVC rejects a string literal longer than 16380 bytes with C2026. The emitter splits a long source
 * at a line end and lets the compiler join the pieces again. A split inside a token would still work,
 * but it makes the generated file unreadable when you diff it to find a broken shader. */
inline constexpr size_t k_MaxStringLiteralBytes = 8192u;

std::string EmitShaderLibraryHeader(const CookedLibrary& library);
std::string EmitShaderLibraryModuleSource(const CookedModule& module, std::string_view header_name);

std::string MakeModuleSourceFileName(std::string_view header_stem, std::string_view module_name);

/** Converts a Slang axis name to a C++ field name: `IFFT_SIZE` becomes `IfftSize`. */
std::string MakeFieldIdentifier(std::string_view axis_name);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_SHADER_LIBRARY_EMITTER_HPP
