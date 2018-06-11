#include "reflection/ReflectionStructs.hpp"
#include "../util/ResourceFormats.hpp"
#include "spirv-cross/spirv_cross.hpp"

namespace st {

    struct PushConstantInfoImpl {
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        std::string name{};
        std::vector<ShaderResourceSubObject> members{};
        uint32_t offset{ std::numeric_limits<uint32_t>::max() };
    };

    PushConstantInfo::PushConstantInfo() noexcept : impl(std::make_unique<PushConstantInfoImpl>()) {}

    PushConstantInfo::~PushConstantInfo() noexcept {
        impl.reset();
    }

    PushConstantInfo::PushConstantInfo(const PushConstantInfo & other) noexcept : impl(std::make_unique<PushConstantInfoImpl>(*other.impl)) {}

    PushConstantInfo& PushConstantInfo::operator=(const PushConstantInfo & other) noexcept {
        impl = std::make_unique<PushConstantInfoImpl>(*other.impl);
        return *this;
    }

    void PushConstantInfo::SetStages(VkShaderStageFlags flags) noexcept {
        impl->stages = std::move(flags);
    }

    void PushConstantInfo::SetName(const char * _name) noexcept {
        impl->name = _name;
    }

    void PushConstantInfo::SetOffset(uint32_t amt) noexcept {
        impl->offset = std::move(amt);
    }

    void PushConstantInfo::SetMembers(const size_t num_members, ShaderResourceSubObject * members) {
        impl->members = std::vector<ShaderResourceSubObject>{ members, members + num_members };
    }

    const VkShaderStageFlags & PushConstantInfo::Stages() const noexcept {
        return impl->stages;
    }

    const char* PushConstantInfo::Name() const noexcept {
        return impl->name.c_str();
    }

    const uint32_t & PushConstantInfo::Offset() const noexcept {
        return impl->offset;
    }

    void PushConstantInfo::GetMembers(size_t * num_members, ShaderResourceSubObject * members) const {
        *num_members = impl->members.size();
        if (members != nullptr) {
            std::copy(impl->members.cbegin(), impl->members.cend(), members);
        }
    }

    PushConstantInfo::operator VkPushConstantRange() const noexcept {
        VkPushConstantRange result;
        result.stageFlags = impl->stages;
        result.offset = impl->offset;
        uint32_t size = 0;
        for (auto& obj : impl->members) {
            size += obj.Size;
        }
        result.size = size;
        return result;
    }

    struct VertexAttributeInfoImpl {
        std::string name;
        spirv_cross::SPIRType type;
        uint32_t location;
        uint32_t offset;
    };

    VertexAttributeInfo::VertexAttributeInfo() noexcept : impl(std::make_unique<VertexAttributeInfoImpl>()) {}

    VertexAttributeInfo::~VertexAttributeInfo() noexcept {}

    VertexAttributeInfo::VertexAttributeInfo(const VertexAttributeInfo & other) noexcept : impl(std::make_unique<VertexAttributeInfoImpl>(*other.impl)) {}

    VertexAttributeInfo & VertexAttributeInfo::operator=(const VertexAttributeInfo & other) noexcept {
        impl = std::make_unique<VertexAttributeInfoImpl>(*other.impl);
        return *this;
    }

    void VertexAttributeInfo::SetName(const char * _name) {
        impl->name = _name;
    }

    void VertexAttributeInfo::SetType(const std::any spir_type_ptr) {
        impl->type = std::any_cast<spirv_cross::SPIRType>(spir_type_ptr);
    }

    void VertexAttributeInfo::SetTypeFromText(const char* str) {
        impl->type = SPIR_TypeFromString(str);
    }

    void VertexAttributeInfo::SetLocation(uint32_t loc) noexcept {
        impl->location = std::move(loc);
    }

    void VertexAttributeInfo::SetOffset(uint32_t _offset) noexcept {
        impl->offset = std::move(_offset);
    }

    const char * VertexAttributeInfo::Name() const noexcept {
        return impl->name.c_str();
    }

    const char* VertexAttributeInfo::TypeAsText() const noexcept {
        return SPIR_TypeToString(impl->type).c_str();
    }

    const uint32_t & VertexAttributeInfo::Location() const noexcept {
        return impl->location;
    }

    const uint32_t & VertexAttributeInfo::Offset() const noexcept {
        return impl->offset;
    }

    VkFormat VertexAttributeInfo::GetAsFormat() const noexcept {
        return VkFormatFromSPIRType(impl->type);
    }

    VertexAttributeInfo::operator VkVertexInputAttributeDescription() const noexcept {
        return VkVertexInputAttributeDescription{ impl->location, 0, GetAsFormat(), impl->offset };
    }

}
