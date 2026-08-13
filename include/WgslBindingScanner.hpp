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

/** The address space and the access mode in a WGSL `var<...>` template list.
 *
 * WebGPU rejects a bind group layout whose buffer type disagrees with this word. The scanner reads
 * the word, and CompareBindings checks it against the BindingKind that reflection reported. */
enum class WgslAddressSpace : uint8_t
{
    Invalid = 0,
    /** No template list. WGSL calls this the handle space: a texture or a sampler. */
    Handle,
    Uniform,
    /** `var<storage, read>`, and also `var<storage>`, because read is the default. */
    StorageRead,
    StorageReadWrite,
};

struct WgslDeclaredBinding
{
    std::string Name;
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
    WgslAddressSpace AddressSpace{ WgslAddressSpace::Invalid };
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

/** True when the WGSL address space is the one that this binding kind must have.
 *
 * The rule is a strict pairing, not a subset test. A uniform block that reflection calls a storage
 * buffer still emits valid WGSL, so only this check finds the error. */
bool AddressSpaceAgreesWithKind(WgslAddressSpace address_space, BindingKind kind) noexcept;

std::string_view ToString(WgslAddressSpace address_space) noexcept;

BindingComparison CompareBindings(std::span<const WgslDeclaredBinding> declared,
                                  std::span<const ReflectedBinding> reflected);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_WGSL_BINDING_SCANNER_HPP
