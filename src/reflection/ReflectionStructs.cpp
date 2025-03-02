#include "reflection/ReflectionStructs.hpp"
#include "../util/ResourceFormats.hpp"
#include <spirv_reflect.h>

namespace st
{

    constexpr VkFormat GetVkFormatFromSpvReflectFormat(SpvReflectFormat format) noexcept
    {
        switch (format)
        {
        case SPV_REFLECT_FORMAT_R16_UINT:
            return VK_FORMAT_R16_UINT;
        case SPV_REFLECT_FORMAT_R16_SINT:
            return VK_FORMAT_R16_SINT;
        case SPV_REFLECT_FORMAT_R16_SFLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case SPV_REFLECT_FORMAT_R32_UINT:
            return VK_FORMAT_R32_UINT;
        case SPV_REFLECT_FORMAT_R32_SINT:
            return VK_FORMAT_R32_SINT;
        case SPV_REFLECT_FORMAT_R32_SFLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case SPV_REFLECT_FORMAT_R64_UINT:
            return VK_FORMAT_R64_UINT;
        case SPV_REFLECT_FORMAT_R64_SINT:
            return VK_FORMAT_R64_SINT;
        case SPV_REFLECT_FORMAT_R64_SFLOAT:
            return VK_FORMAT_R64_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case SPV_REFLECT_FORMAT_R16G16_SINT:
            return VK_FORMAT_R16G16_SINT;
        case SPV_REFLECT_FORMAT_R16G16_SFLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16B16_UINT:
            return VK_FORMAT_R16G16B16_UINT;
        case SPV_REFLECT_FORMAT_R16G16B16_SINT:
            return VK_FORMAT_R16G16B16_SINT;
        case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32_UINT:
            return VK_FORMAT_R32G32_UINT;
        case SPV_REFLECT_FORMAT_R32G32_SINT:
            return VK_FORMAT_R32G32_SINT;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64_UINT:
            return VK_FORMAT_R64G64_UINT;
        case SPV_REFLECT_FORMAT_R64G64_SINT:
            return VK_FORMAT_R64G64_SINT;
        case SPV_REFLECT_FORMAT_R64G64_SFLOAT:
            return VK_FORMAT_R64G64_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64B64_UINT:
            return VK_FORMAT_R64G64B64_UINT;
        case SPV_REFLECT_FORMAT_R64G64B64_SINT:
            return VK_FORMAT_R64G64B64_SINT;
        case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:
            return VK_FORMAT_R64G64B64_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:
            return VK_FORMAT_R64G64B64A64_UINT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:
            return VK_FORMAT_R64G64B64A64_SINT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:
            return VK_FORMAT_R64G64B64A64_SFLOAT;       
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

    struct PushConstantInfoImpl
    {
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        std::string name{};
        std::vector<ShaderResourceSubObject> members{};
        uint32_t offset{ 0u };
    };

    PushConstantInfo::PushConstantInfo() noexcept : impl(std::make_unique<PushConstantInfoImpl>()) {}

    PushConstantInfo::~PushConstantInfo() noexcept
    {
        impl.reset();
    }

    PushConstantInfo::PushConstantInfo(const PushConstantInfo& other) noexcept : impl(std::make_unique<PushConstantInfoImpl>(*other.impl)) {}

    PushConstantInfo& PushConstantInfo::operator=(const PushConstantInfo& other) noexcept
    {
        impl = std::make_unique<PushConstantInfoImpl>(*other.impl);
        return *this;
    }

    void PushConstantInfo::SetStages(VkShaderStageFlags flags) noexcept
    {
        impl->stages = std::move(flags);
    }

    void PushConstantInfo::SetName(const char* _name) noexcept
    {
        impl->name = _name;
    }

    void PushConstantInfo::SetOffset(uint32_t amt) noexcept
    {
        impl->offset = std::move(amt);
    }

    void PushConstantInfo::SetMembers(const size_t num_members, ShaderResourceSubObject* members)
    {
        impl->members.resize(num_members);
        for (size_t i = 0; i < num_members; ++i)
        {
            impl->members[i] = members[i];
        }
    }

    const VkShaderStageFlags & PushConstantInfo::Stages() const noexcept
    {
        return impl->stages;
    }

    const char* PushConstantInfo::Name() const noexcept
    {
        return impl->name.c_str();
    }

    const uint32_t& PushConstantInfo::Offset() const noexcept {
        return impl->offset;
    }

    void PushConstantInfo::GetMembers(size_t* num_members, ShaderResourceSubObject* members) const
    {
        *num_members = impl->members.size();
        if (members != nullptr)
        {
            std::copy(impl->members.cbegin(), impl->members.cend(), members);
        }
    }

    // Weird logic of potentially optimized out members makes this one more complex than it should have to be...
    PushConstantInfo::operator VkPushConstantRange() const noexcept
    {
        VkPushConstantRange result;
        result.stageFlags = impl->stages;
        result.offset = impl->offset;

        // early out for single-member case
        if (impl->members.size() == 1)
        {
            result.size = impl->members.front().Size;
            return result;
        }

        uint32_t size = 0;
        // Weird size calculation: sometimes, a variable in the middle of our range
        // will be optimized out.
        // note many years later: maybe we should figure out why this is happening, lol
        for (auto& obj : impl->members)
        {
            size = std::max(size, obj.Offset);
        }
        result.size = size + impl->members.back().Size;
        return result;
    }

    struct VertexAttributeInfoImpl
    {
        VertexAttributeInfoImpl() noexcept : name{}, type{ 0 }, format{ VK_FORMAT_UNDEFINED }, location { 0 }, offset{ 0 }
        {
        }
        VertexAttributeInfoImpl(const VertexAttributeInfoImpl&) noexcept = default;
        VertexAttributeInfoImpl(VertexAttributeInfoImpl&&) noexcept = default;
        VertexAttributeInfoImpl& operator=(const VertexAttributeInfoImpl&) noexcept = default;
        VertexAttributeInfoImpl& operator=(VertexAttributeInfoImpl&&) noexcept = default;
        std::string name;
        SpvReflectTypeFlags type;
        VkFormat format;
        uint32_t location;
        uint32_t offset;
    };

    VertexAttributeInfo::VertexAttributeInfo() noexcept : impl(std::make_unique<VertexAttributeInfoImpl>()) {}

    VertexAttributeInfo::~VertexAttributeInfo() noexcept {}

    VertexAttributeInfo::VertexAttributeInfo(const VertexAttributeInfo& other) noexcept : impl(std::make_unique<VertexAttributeInfoImpl>(*other.impl)) {}

    VertexAttributeInfo& VertexAttributeInfo::operator=(const VertexAttributeInfo& other) noexcept
    {
        impl = std::make_unique<VertexAttributeInfoImpl>(*other.impl);
        return *this;
    }

    void VertexAttributeInfo::SetName(const char* _name)
    {
        impl->name = _name;
    }

    void VertexAttributeInfo::SetFormatFromSpvReflectFlags(uint32_t spv_reflect_format_flags)
    {
        impl->format = GetVkFormatFromSpvReflectFormat(static_cast<SpvReflectFormat>(spv_reflect_format_flags));
    }

    void VertexAttributeInfo::SetLocation(uint32_t loc) noexcept
    {
        impl->location = std::move(loc);
    }

    void VertexAttributeInfo::SetOffset(uint32_t _offset) noexcept
    {
        impl->offset = std::move(_offset);
    }

    const char* VertexAttributeInfo::Name() const noexcept
    {
        return impl->name.c_str();
    }

    const char* VertexAttributeInfo::TypeAsText() const noexcept
    {
        return spvReflect_TypeToString(impl->type);
    }

    uint32_t VertexAttributeInfo::Location() const noexcept
    {
        return impl->location;
    }

    uint32_t VertexAttributeInfo::Offset() const noexcept
    {
        return impl->offset;
    }

    VkFormat VertexAttributeInfo::GetAsFormat() const noexcept
    {
        return impl->format;
    }

    VertexAttributeInfo::operator VkVertexInputAttributeDescription() const noexcept
    {
        return VkVertexInputAttributeDescription{ impl->location, 0, impl->format, impl->offset };
    }

}
