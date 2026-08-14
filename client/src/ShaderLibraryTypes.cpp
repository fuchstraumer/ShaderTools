#include "shader/ShaderLibraryTypes.hpp"

namespace velox
{

ShaderSourceProvider::ShaderSourceProvider() noexcept {};
ShaderSourceProvider::~ShaderSourceProvider() noexcept {};

const BindingInfo* FindBindingByName(std::span<const BindingInfo> bindings,
                                     std::string_view name) noexcept
{
    for (const BindingInfo& binding : bindings)
    {
        if (binding.Name == name)
        {
            return &binding;
        }
    }

    return nullptr;
}

uint64_t BindingInfo::DerivedByteSize() const noexcept
{
    if (DerivedElementCount == 0u || ElementStride == 0u)
    {
        return 0u;
    }

    return DerivedElementCount * static_cast<uint64_t>(ElementStride);
}

bool BindingInfo::Validate() const noexcept
{
    if (Group == static_cast<uint32_t>(-1) || Binding == static_cast<uint32_t>(-1))
    {
        return false;
    }

    if (Kind == BindingKind::Invalid || Shape == ResourceShape::Invalid)
    {
        return false;
    }

    if (Kind == BindingKind::StorageTexture && StorageAccess == StorageTextureAccess::Invalid)
    {
        return false;
    }

    if (Kind == BindingKind::Sampler && SamplerType == SamplerBindingType::Invalid)
    {
        return false;
    }

    return true;
}

const UniformMemberInfo* FindUniformMember(std::span<const UniformMemberInfo> members,
                                           std::string_view name) noexcept
{
    for (const UniformMemberInfo& member : members)
    {
        if (member.Name == name)
        {
            return &member;
        }
    }

    return nullptr;
}


bool IsBufferBinding(BindingKind kind) noexcept
{
    return kind == BindingKind::UniformBuffer || kind == BindingKind::StorageBuffer ||
           kind == BindingKind::ReadOnlyStorageBuffer;
}

bool IsTextureBinding(BindingKind kind) noexcept
{
    return kind == BindingKind::SampledTexture || kind == BindingKind::StorageTexture;
}

} // namespace velox
