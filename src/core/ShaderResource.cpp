#include "core/ShaderResource.hpp"

namespace st {

    class ShaderResourceImpl {
    public:

        ShaderResourceImpl() = default;
        ~ShaderResourceImpl() {};
        ShaderResourceImpl(const ShaderResourceImpl& other) noexcept;
        ShaderResourceImpl(ShaderResourceImpl&& other) noexcept;
        ShaderResourceImpl& operator=(const ShaderResourceImpl& other) noexcept;
        ShaderResourceImpl& operator=(ShaderResourceImpl&& other) noexcept;
        std::string name{ "" };
        size_t memoryRequired{ std::numeric_limits<size_t>::max() };
        std::string parentSetName{ "" };
        size_class sizeClass{ size_class::Absolute };
        storage_class storageClass{ storage_class::Read };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        VkFormat format{ VK_FORMAT_UNDEFINED };

    };

    ShaderResourceImpl::ShaderResourceImpl(const ShaderResourceImpl & other) noexcept : memoryRequired(other.memoryRequired), parentSetName(other.parentSetName),
        name(other.name), sizeClass(other.sizeClass), storageClass(other.storageClass), stages(other.stages), type(other.type), members(other.members), format(other.format) {}

    ShaderResourceImpl::ShaderResourceImpl(ShaderResourceImpl && other) noexcept : memoryRequired(std::move(other.memoryRequired)), parentSetName(std::move(other.parentSetName)),
        name(std::move(other.name)), sizeClass(std::move(other.sizeClass)), storageClass(std::move(other.storageClass)), stages(std::move(other.stages)), type(std::move(other.type)), members(std::move(other.members)),
        format(std::move(other.format)) {}

    ShaderResourceImpl & ShaderResourceImpl::operator=(const ShaderResourceImpl & other) noexcept {
        memoryRequired = other.memoryRequired;
        parentSetName = other.parentSetName;
        name = other.name;
        sizeClass = other.sizeClass;
        storageClass = other.storageClass;
        stages = other.stages;
        type = other.type;
        members = other.members;
        format = other.format;
        return *this;
    }

    ShaderResourceImpl & ShaderResourceImpl::operator=(ShaderResourceImpl && other) noexcept {
        memoryRequired = std::move(other.memoryRequired);
        parentSetName = std::move(other.parentSetName);
        name = std::move(other.name);
        sizeClass = std::move(other.sizeClass);
        storageClass = std::move(other.storageClass);
        stages = std::move(other.stages);
        type = std::move(other.type);
        members = std::move(other.members);
        format = std::move(other.format);
        return *this;
    }

    ShaderResource::ShaderResource() : impl(std::make_unique<ShaderResourceImpl>()) {}

    ShaderResource::ShaderResource(uint32_t parent_idx, uint32_t binding_idx) : impl(std::make_unique<ShaderResourceImpl>(parent_idx, binding_idx)) {}

    ShaderResource::~ShaderResource() {}

    ShaderResource::ShaderResource(const ShaderResource & other) noexcept : impl(std::make_unique<ShaderResourceImpl>(*other.impl)) { }

    ShaderResource::ShaderResource(ShaderResource && other) noexcept : impl(std::move(other.impl)) {}

    ShaderResource & ShaderResource::operator=(const ShaderResource& other) noexcept {
        impl = std::make_unique<ShaderResourceImpl>(*other.impl);
        return *this;
    }

    ShaderResource & ShaderResource::operator=(ShaderResource && other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    bool ShaderResource::operator==(const ShaderResource & other) const noexcept {
        return (impl->name == other.impl->name) && (impl->type == other.impl->type);
    }

    bool ShaderResource::operator<(const ShaderResource & other) const noexcept {
        return impl->memoryRequired < other.impl->memoryRequired;
    }

    const size_t & ShaderResource::GetAmountOfMemoryRequired() const noexcept {
        return impl->memoryRequired;
    }

    const VkFormat& ShaderResource::GetFormat() const noexcept {
        return impl->format;
    }

    const char* ShaderResource::GetName() const {
        return impl->name.c_str();
    }

    const char * ShaderResource::ParentGroupName() const {
        return impl->parentSetName.c_str();
    }

    const VkShaderStageFlags & ShaderResource::GetStages() const noexcept {
        return impl->stages;
    }

    const VkDescriptorType & ShaderResource::GetType() const noexcept {
        return impl->type;
    }

    void ShaderResource::GetMembers(size_t* num_members, ShaderResourceSubObject* objects) const noexcept {
        *num_members = impl->members.size();
        if (objects != nullptr) {
            std::copy(impl->members.cbegin(), impl->members.cend(), objects);
        }
    }

    void ShaderResource::SetMemoryRequired(size_t amt) {
        impl->memoryRequired = std::move(amt);
    }

    void ShaderResource::SetStages(VkShaderStageFlags stages) {
        impl->stages = std::move(stages);
    }

    void ShaderResource::SetType(VkDescriptorType _type) {
        impl->type = std::move(_type);
    }

    void ShaderResource::SetSizeClass(size_class _size_class) {
        impl->sizeClass = std::move(_size_class);
    }

    void ShaderResource::SetStorageClass(storage_class _storage_class) {
        impl->storageClass = std::move(_storage_class);
    }

    void ShaderResource::SetName(const char* name) {
        impl->name = std::string{ name };
    }

    void ShaderResource::SetParentGroupName(const char * parent_group_name) {
        impl->parentSetName = parent_group_name;
    }

    void ShaderResource::SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects) {
        impl->members = std::move(std::vector<ShaderResourceSubObject>{ src_objects, src_objects + num_members });
    }

    void ShaderResource::SetFormat(VkFormat fmt) {
        impl->format = std::move(fmt);
    }

}