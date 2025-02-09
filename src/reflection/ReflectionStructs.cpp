#include "reflection/ReflectionStructs.hpp"
#include "../util/ResourceFormats.hpp"
#include "spirv-cross/spirv_cross.hpp"

namespace st
{

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
        VertexAttributeInfoImpl() noexcept : name{}, type{ spv::Op::OpNop }, location{ 0 }, offset{ 0 }
        {
        }
        VertexAttributeInfoImpl(const VertexAttributeInfoImpl&) noexcept = default;
        VertexAttributeInfoImpl(VertexAttributeInfoImpl&&) noexcept = default;
        VertexAttributeInfoImpl& operator=(const VertexAttributeInfoImpl&) noexcept = default;
        VertexAttributeInfoImpl& operator=(VertexAttributeInfoImpl&&) noexcept = default;
        std::string name;
        spirv_cross::SPIRType type;
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

    void VertexAttributeInfo::SetType(const void* spir_type_ptr)
    {
        // note: this is mostly just to avoid having to do stupid includes across DLL boundaries, or spreading the mega-web
        // of SPIR-V includes further than source files (yucky)
        impl->type = *reinterpret_cast<const spirv_cross::SPIRType*>(spir_type_ptr);
    }

    void VertexAttributeInfo::SetTypeFromText(const char* str)
    {
        impl->type = SPIR_TypeFromString(str);
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
        return SPIR_TypeToString(impl->type).c_str();
    }

    const uint32_t& VertexAttributeInfo::Location() const noexcept
    {
        return impl->location;
    }

    const uint32_t& VertexAttributeInfo::Offset() const noexcept
    {
        return impl->offset;
    }

    VkFormat VertexAttributeInfo::GetAsFormat() const noexcept
    {
        return VkFormatFromSPIRType(impl->type);
    }

    VertexAttributeInfo::operator VkVertexInputAttributeDescription() const noexcept
    {
        return VkVertexInputAttributeDescription{ impl->location, 0, GetAsFormat(), impl->offset };
    }

}
