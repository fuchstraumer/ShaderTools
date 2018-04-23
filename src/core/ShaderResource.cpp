#include "common/ShaderResource.hpp"

namespace st {

    class ShaderResourceImpl {
    public:

        ShaderResourceImpl() = default;
        ShaderResourceImpl(uint32_t parent, uint32_t binding);
        ~ShaderResourceImpl() {};
        ShaderResourceImpl(const ShaderResourceImpl& other) noexcept;
        ShaderResourceImpl(ShaderResourceImpl&& other) noexcept;
        ShaderResourceImpl& operator=(const ShaderResourceImpl& other) noexcept;
        ShaderResourceImpl& operator=(ShaderResourceImpl&& other) noexcept;
        std::string name{ "" };
        uint32_t binding{ 0 };
        uint32_t parentIdx{ std::numeric_limits<uint32_t>::max() };
        size_t memoryRequired{ std::numeric_limits<size_t>::max() };
        std::string parentSetName{ "" };
        size_class sizeClass{ size_class::Absolute };
        storage_class storageClass{ storage_class::Read };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        VkFormat format{ VK_FORMAT_UNDEFINED };

    };

    ShaderResourceImpl::ShaderResourceImpl(uint32_t parent, uint32_t _binding) : parentIdx(std::move(parent)), binding(std::move(_binding)) {}

    ShaderResourceImpl::ShaderResourceImpl(const ShaderResourceImpl & other) noexcept : memoryRequired(other.memoryRequired), parentSetName(other.parentSetName), binding(other.binding), 
        name(other.name), sizeClass(other.sizeClass), storageClass(other.storageClass), stages(other.stages), type(other.type), members(other.members), format(other.format),
        parentIdx(other.parentIdx) {}

    ShaderResourceImpl::ShaderResourceImpl(ShaderResourceImpl && other) noexcept : memoryRequired(std::move(other.memoryRequired)), parentSetName(std::move(other.parentSetName)), binding(std::move(other.binding)),
        name(std::move(other.name)), sizeClass(std::move(other.sizeClass)), storageClass(std::move(other.storageClass)), stages(std::move(other.stages)), type(std::move(other.type)), members(std::move(other.members)),
        format(std::move(other.format)), parentIdx(std::move(other.parentIdx)) {}

    ShaderResourceImpl & ShaderResourceImpl::operator=(const ShaderResourceImpl & other) noexcept {
        memoryRequired = other.memoryRequired;
        parentSetName = other.parentSetName;
        binding = other.binding;
        name = other.name;
        sizeClass = other.sizeClass;
        storageClass = other.storageClass;
        stages = other.stages;
        type = other.type;
        members = other.members;
        format = other.format;
        parentIdx = other.parentIdx;
        return *this;
    }

    ShaderResourceImpl & ShaderResourceImpl::operator=(ShaderResourceImpl && other) noexcept {
        memoryRequired = std::move(other.memoryRequired);
        parentSetName = std::move(other.parentSetName);
        binding = std::move(other.binding);
        name = std::move(other.name);
        sizeClass = std::move(other.sizeClass);
        storageClass = std::move(other.storageClass);
        stages = std::move(other.stages);
        type = std::move(other.type);
        members = std::move(other.members);
        format = std::move(other.format);
        parentIdx = std::move(other.parentIdx);
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
        return impl->binding == other.impl->binding;
    }

    bool ShaderResource::operator<(const ShaderResource & other) const noexcept {
        return impl->binding < other.impl->binding;
    }

    ShaderResource::operator VkDescriptorSetLayoutBinding() const {
        if ((GetType() != VK_DESCRIPTOR_TYPE_MAX_ENUM) && (GetType() != VK_DESCRIPTOR_TYPE_RANGE_SIZE)) {
            return VkDescriptorSetLayoutBinding{
               GetBinding(), GetType(), 1, GetStages(), nullptr
            };
        }
        else {
            return VkDescriptorSetLayoutBinding{
                GetBinding(), GetType(), 0, GetStages(), nullptr
            };
        }
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

    const uint32_t & ShaderResource::GetParentIdx() const noexcept {
        return impl->parentIdx;
    }

    const uint32_t & ShaderResource::GetBinding() const noexcept {
        return impl->binding;
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

    void ShaderResource::SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects) {
        impl->members = std::move(std::vector<ShaderResourceSubObject>{ src_objects, src_objects + num_members });
    }

    void ShaderResource::SetFormat(VkFormat fmt) {
        impl->format = std::move(fmt);
    }

    void ShaderResource::SetParentIdx(uint32_t parent_idx) {
        impl->parentIdx = parent_idx;
    }

    void ShaderResource::SetBinding(uint32_t binding) {
        impl->binding = std::move(binding);
    }


}