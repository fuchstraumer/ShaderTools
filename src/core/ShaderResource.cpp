#include "core/ShaderResource.hpp"
#include "common/UtilityStructs.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include <set>
#include <unordered_map>

namespace st {

    constexpr static VkImageCreateInfo image_create_info_base{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_MAX_ENUM, // set this invalid at start, because it needs to be set properly
        VK_FORMAT_UNDEFINED, // this can't be set by shadertools, so make it invalid for now
        VkExtent3D{}, // initialize but leave "invalid": ST may not be able to extract the size
        1, // reasonable MIP base
        1, // most images only have 1 "array" layer
        VK_SAMPLE_COUNT_1_BIT, // most images not multisampled
        VK_IMAGE_TILING_MAX_ENUM, // this has to be set based on format and usage
        0, // leave image usage flags blank
        VK_SHARING_MODE_EXCLUSIVE, // next 3 sharing parameters "disabled"
        0,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED // most images start like this
    };

    constexpr static VkImageViewCreateInfo image_view_create_info_base {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        VK_NULL_HANDLE,
        VK_IMAGE_VIEW_TYPE_MAX_ENUM,
        VK_FORMAT_UNDEFINED,
		{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    constexpr static VkSamplerCreateInfo sampler_create_info_base{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, // This is a good default. Its presence rarely causes visible bugs:
        VK_SAMPLER_ADDRESS_MODE_REPEAT, // using other values like clamp though, will cause any bugs that can
        VK_SAMPLER_ADDRESS_MODE_REPEAT, // occur to readily appareny (warped, stretched, weird texturing)
        0.0f,
        VK_FALSE, // anisotropy disabled by default
        1.0f, // max anisotropy is 1.0 by default. depends upon device.
        VK_FALSE,
        VK_COMPARE_OP_ALWAYS,
        0.0f,
        1.0f,
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        VK_FALSE // unnormalized coordinates rare as heck
    };

    constexpr static VkBufferViewCreateInfo buffer_view_info_base {
        VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        nullptr,
        0,
        VK_NULL_HANDLE,
        VK_FORMAT_UNDEFINED,
        0,
        0
    };

    // if trying to retrieve image info for a non-image descriptor type, return this
    // so that the API causes tons of obvious errors and so that the validation layers throw a fit
    constexpr static VkImageCreateInfo invalid_image_create_info{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_MAX_ENUM,
        VK_FORMAT_MAX_ENUM,
        VkExtent3D{},
        std::numeric_limits<uint32_t>::max(),
        std::numeric_limits<uint32_t>::max(),
        VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM,
        VK_IMAGE_TILING_MAX_ENUM,
        VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM,
        VK_SHARING_MODE_MAX_ENUM,
        std::numeric_limits<uint32_t>::max(),
        nullptr,
        VK_IMAGE_LAYOUT_MAX_ENUM
    };

    // When someone tries to retrieve sampler info for a non-sampler resource, return this.
    // It should cause havoc in the API, as almost all the values are invalid.
    constexpr static VkSamplerCreateInfo invalid_sampler_create_info {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_MAX_ENUM,
        VK_FILTER_MAX_ENUM,
        VK_SAMPLER_MIPMAP_MODE_MAX_ENUM,
        VK_SAMPLER_ADDRESS_MODE_MAX_ENUM,
        VK_SAMPLER_ADDRESS_MODE_MAX_ENUM,
        VK_SAMPLER_ADDRESS_MODE_MAX_ENUM,
        -1.0f,
        VK_FALSE,
        -1.0f,
        VK_FALSE,
        VK_COMPARE_OP_MAX_ENUM,
        -1.0f,
        -1.0f,
        VK_BORDER_COLOR_MAX_ENUM,
        VK_FALSE
    };

    constexpr static VkBufferViewCreateInfo invalid_buffer_view_create_info{
        VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        nullptr,
        0,
        VK_NULL_HANDLE,
        VK_FORMAT_MAX_ENUM,
        std::numeric_limits<uint32_t>::max(),
        std::numeric_limits<uint32_t>::max()
    };

    class ShaderResourceImpl {
    public:

        ShaderResourceImpl() = default;
        ~ShaderResourceImpl() {};
        ShaderResourceImpl(const ShaderResourceImpl& other) = default;
        ShaderResourceImpl(ShaderResourceImpl&& other) noexcept = default;
        ShaderResourceImpl& operator=(const ShaderResourceImpl& other) = default;
        ShaderResourceImpl& operator=(ShaderResourceImpl&& other) = default;

        size_t bindingIdx{ std::numeric_limits<size_t>::max() };
        std::string name{ "" };
        size_t memoryRequired{ std::numeric_limits<size_t>::max() };
        VkFormat format{ VK_FORMAT_UNDEFINED };
        bool fromFile{ false };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        size_t inputAttachmentIdx{ std::numeric_limits<size_t>::max() };
        std::string parentSetName{ "" };
        size_class sizeClass{ size_class::Absolute };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        std::vector<std::string> tags;
        std::set<glsl_qualifier> qualifiers;
        std::unordered_map<std::string, std::set<glsl_qualifier>> perUsageQualifiers;
        VkImageCreateInfo imageInfo{ image_create_info_base };
        VkImageViewCreateInfo imageViewInfo{ image_view_create_info_base };
        VkSamplerCreateInfo samplerInfo{ sampler_create_info_base };
        VkBufferViewCreateInfo bufferInfo{ buffer_view_info_base };
        bool needsMipMaps{ false };

    };

    ShaderResource::ShaderResource() : impl(std::make_unique<ShaderResourceImpl>()) {}

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

    const size_t& ShaderResource::BindingIndex() const noexcept {
        return impl->bindingIdx;
    }

    const size_t& ShaderResource::InputAttachmentIndex() const noexcept {
        if (impl->type != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
            LOG(WARNING) << "Tried to retrieve input attachment index for resource that is not an input attachment.";
            // Default constructed value should be invalid enough to be obvious.
        }
        return impl->inputAttachmentIdx;
    }

    const size_t& ShaderResource::MemoryRequired() const noexcept {
        return impl->memoryRequired;
    }

    const VkFormat& ShaderResource::Format() const noexcept {
        return impl->format;
    }

    const bool& ShaderResource::FromFile() const noexcept {
        return impl->fromFile;
    }

    const char* ShaderResource::Name() const {
        return impl->name.c_str();
    }

    const char* ShaderResource::ParentGroupName() const {
        return impl->parentSetName.c_str();
    }

    const VkShaderStageFlags& ShaderResource::ShaderStages() const noexcept {
        return impl->stages;
    }

    const VkDescriptorType& ShaderResource::DescriptorType() const noexcept {
        return impl->type;
    }

    const VkImageCreateInfo& ShaderResource::ImageInfo() const noexcept {
        if (impl->type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || impl->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            return impl->imageInfo;
        }
        else {
            LOG(WARNING) << "Attempted to retrieve VkImageCreateInfo for invalid descriptor type. Invalid VkImageCreateInfo structure returned.";
            return invalid_image_create_info;
        }
    }

    const VkImageViewCreateInfo& ShaderResource::ImageViewInfo() const noexcept {
        if (impl->type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || impl->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            return impl->imageViewInfo;
        }
        else {
            LOG(WARNING) << "Attempted to retrieve VkImageViewCreateInfo for invalid descriptor type. Returning invalid VkImageViewCreateInfo object.";
            return image_view_create_info_base;
        }
    }

    const VkSamplerCreateInfo& ShaderResource::SamplerInfo() const noexcept {
        if (impl->type == VK_DESCRIPTOR_TYPE_SAMPLER || impl->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            return impl->samplerInfo;
        }
        else {
            LOG(WARNING) << "Attempted to retrieve VkSamplerCreateInfo for invalid descriptor type. Invalid VkSamplerCreateInfo structure returned.";
            return invalid_sampler_create_info;
        }
    }

    const VkBufferViewCreateInfo& ShaderResource::BufferViewInfo() const noexcept {
        if (impl->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || impl->type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER || impl->type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
            return impl->bufferInfo;
        }
        else {
            LOG(WARNING) << "Attempted to retrieve VkBufferViewCreateInfo for invalid descriptor type. Invalid VkBufferViewCreateInfo structure returned.";
            return invalid_buffer_view_create_info;
        }
    }

    bool ShaderResource::HasQualifiers() const noexcept {
        return !impl->qualifiers.empty();
    }

    void ShaderResource::GetQualifiers(size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept {
        *num_qualifiers = impl->qualifiers.size();
        if (qualifiers != nullptr) {
            std::vector<glsl_qualifier> vector_copy;
            for (const auto& qual : impl->qualifiers) {
                vector_copy.emplace_back(qual);
            }
            std::copy(vector_copy.begin(), vector_copy.end(), qualifiers);
        }
    }

    void ShaderResource::GetPerUsageQualifiers(const char * shader_name, size_t * num_qualifiers, glsl_qualifier * qualifiers) const noexcept {
        if (auto iter = impl->perUsageQualifiers.find(shader_name); iter != std::end(impl->perUsageQualifiers)) {
            *num_qualifiers = iter->second.size();
            if (qualifiers != nullptr) {
                std::vector<glsl_qualifier> vector_copy;
                for (const auto& qual : iter->second) {
                    vector_copy.emplace_back(qual);
                }
                std::copy(vector_copy.begin(), vector_copy.end(), qualifiers);
            }
        }
    }

    glsl_qualifier ShaderResource::GetReadWriteQualifierForShader(const char * shader_name) const noexcept {
        // First just check general qualifiers for R/W options
        for (const auto& qual : impl->qualifiers) {
            if (qual == glsl_qualifier::ReadOnly || qual == glsl_qualifier::WriteOnly) {
                return qual;
            }
        }

        auto& stage_qualifiers = impl->perUsageQualifiers[std::string(shader_name)];
        for (const auto& qual : stage_qualifiers) {
            if (qual == glsl_qualifier::ReadOnly || qual == glsl_qualifier::WriteOnly) {
                return qual;
            }
        }

        return glsl_qualifier::InvalidQualifier;
    }

    void ShaderResource::GetMembers(size_t* num_members, ShaderResourceSubObject* objects) const noexcept {
        *num_members = impl->members.size();
        if (objects != nullptr) {
            std::copy(impl->members.cbegin(), impl->members.cend(), objects);
        }
    }

    dll_retrieved_strings_t ShaderResource::GetTags() const noexcept {
        dll_retrieved_strings_t result;
        result.SetNumStrings(impl->tags.size());
        for (size_t i = 0; i < impl->tags.size(); ++i) {
            result.Strings[i] = strdup(impl->tags[i].c_str());
        }
        return result;
    }

    void ShaderResource::SetBindingIndex(size_t idx) {
        impl->bindingIdx = std::move(idx);
    }

    void ShaderResource::SetDataFromFile(bool from_file) {
        impl->fromFile = std::move(from_file);
    }

    void ShaderResource::SetInputAttachmentIndex(size_t idx) {
        impl->inputAttachmentIdx = idx;
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

    void ShaderResource::SetName(const char* name) {
        impl->name = std::string{ name };
    }

    void ShaderResource::SetParentGroupName(const char * parent_group_name) {
        impl->parentSetName = parent_group_name;
    }

    void ShaderResource::SetQualifiers(const size_t num_qualifiers, glsl_qualifier* qualifiers) {
        for (size_t i = 0; i < num_qualifiers; ++i) {
            impl->qualifiers.emplace(qualifiers[i]);
        }
    }

    void ShaderResource::AddPerUsageQualifier(const char * shader_name, glsl_qualifier qualifier) {
        impl->perUsageQualifiers[std::string(shader_name)].emplace(qualifier);
    }

    void ShaderResource::AddPerUsageQualifiers(const char * shader_name, const size_t num_qualifiers, const glsl_qualifier * qualifiers) {
        auto& qualifier_set = impl->perUsageQualifiers[std::string(shader_name)];
        for (size_t i = 0; i < num_qualifiers; ++i) {
            qualifier_set.emplace(qualifiers[i]);
        }
    }

    void ShaderResource::SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects) {
        impl->members = std::move(std::vector<ShaderResourceSubObject>{ src_objects, src_objects + num_members });
    }

    void ShaderResource::SetFormat(VkFormat fmt) {
        impl->format = std::move(fmt);
    }

    void ShaderResource::SetImageInfo(VkImageCreateInfo image_info) {
        impl->imageInfo = std::move(image_info);
    }

    void ShaderResource::SetImageViewInfo(VkImageViewCreateInfo view_info) {
        impl->imageViewInfo = std::move(view_info);
    }

    void ShaderResource::SetSamplerInfo(VkSamplerCreateInfo sampler_info) {
        impl->samplerInfo = std::move(sampler_info);
    }

    void ShaderResource::SetBufferViewInfo(VkBufferViewCreateInfo buffer_info) {
        impl->bufferInfo = std::move(buffer_info);
    }

    void ShaderResource::SetTags(const size_t num_tags, const char ** tags) {
        impl->tags.clear(); impl->tags.shrink_to_fit();
        for (size_t i = 0; i < num_tags; ++i) {
            impl->tags.emplace_back(tags[i]);
        }
    }

}
