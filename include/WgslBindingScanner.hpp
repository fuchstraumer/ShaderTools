#pragma once
#ifndef VELOX_SHADER_COOKER_WGSL_BINDING_SCANNER_HPP
#define VELOX_SHADER_COOKER_WGSL_BINDING_SCANNER_HPP
#include "ShaderDataSchema.hpp"
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** Reads `@group`/`@binding` attributes back out of emitted WGSL so reflection output can be checked
 * against the text WebGPU will actually consume. Deliberately not a WGSL parser and must never become
 * one: it exists to validate our use of Slang's reflection API, not to replace it. */
namespace velox::cooker
{

struct WgslDeclaredBinding
{
    std::string Name;
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
};

struct BindingComparison
{
    bool Matches{ false };
    std::string Report;
};

std::vector<WgslDeclaredBinding> ScanWgslBindings(std::string_view wgsl);

/** Slang mangles emitted WGSL identifiers with a numeric suffix (`IfftParams` -> `IfftParams_0`), so
 * comparison is on locations first and de-mangled names second. */
std::string_view StripSlangNameMangling(std::string_view mangled_name) noexcept;

BindingComparison CompareBindings(std::span<const WgslDeclaredBinding> declared,
                                  std::span<const ReflectedBinding> reflected);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_WGSL_BINDING_SCANNER_HPP
