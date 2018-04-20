#include "parser/ShaderResource.hpp"

namespace st {

    class ShaderResourceImpl {
    public:

        size_t memoryRequired;
        std::string parentSetName{ "" };
        uint32_t binding{ 0 };
        std::string name{ "" };
        size_class sizeClass{ size_class::Absolute };
        storage_class storageClass{ storage_class::Read };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        VkFormat format{ VK_FORMAT_UNDEFINED };
    };

    ShaderResource::operator VkDescriptorSetLayoutBinding() const {
        if (type != VK_DESCRIPTOR_TYPE_MAX_ENUM && type != VK_DESCRIPTOR_TYPE_RANGE_SIZE) {
            return VkDescriptorSetLayoutBinding{
                0, type, 1, stages, nullptr
            };
        }
        else {
            return VkDescriptorSetLayoutBinding{
                0, type, 0, stages, nullptr
            };
        }
    }

    VkFormat ShaderResource::GetFormat() const noexcept {
        return impl->format;
    }

    const char* ShaderResource::GetName() const {
        return impl->name.c_str();
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

    void ShaderResource::SetBinding(uint32_t binding) {
        impl->binding = std::move(binding);
    }

}