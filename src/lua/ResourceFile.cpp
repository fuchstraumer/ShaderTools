#include "ResourceFile.hpp"
#include "generation/ShaderGenerator.hpp"
#include "core/ShaderPack.hpp"
#include "common/UtilityStructs.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../util/ResourceFormats.hpp"
#include <iostream>
#include "easyloggingpp/src/easylogging++.h"
#ifdef FindResource
#undef FindResource
#endif // FindResource

namespace st {

    constexpr size_t MINIMUM_REQUIRED_MAX_STORAGE_BUFFER_SIZE = 134217728;
    constexpr size_t MINIMUM_REQUIRED_MAX_UNIFORM_BUFFER_SIZE = 16384;

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

    constexpr static VkBufferViewCreateInfo buffer_view_info_base{
        VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        nullptr,
        0,
        VK_NULL_HANDLE,
        VK_FORMAT_UNDEFINED,
        0,
        0
    };

    static const std::unordered_map<std::string, size_class> size_class_from_string_map{
        { "Absolute", size_class::Absolute },
        { "SwapchainRelative", size_class::SwapchainRelative },
        { "ViewportRelative", size_class::ViewportRelative }
    };

    static const std::unordered_map<int, VkSampleCountFlagBits> sample_count_from_int_map = {
        { 1,  VK_SAMPLE_COUNT_1_BIT },
        { 2,  VK_SAMPLE_COUNT_2_BIT },
        { 4,  VK_SAMPLE_COUNT_4_BIT },
        { 8,  VK_SAMPLE_COUNT_8_BIT },
        { 16, VK_SAMPLE_COUNT_16_BIT },
        { 32, VK_SAMPLE_COUNT_32_BIT },
        { 64, VK_SAMPLE_COUNT_64_BIT }
    };

    static const std::unordered_map<std::string, VkSamplerAddressMode> address_mode_from_str_map = {
        { "ClampToEdge", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
        { "ClampToBorder", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER },
        { "Repeat", VK_SAMPLER_ADDRESS_MODE_REPEAT },
        { "MirroredRepeat", VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT },
        { "MirroredClampToEdge", VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE }
    };

    static const std::unordered_map<std::string, VkCompareOp> compare_op_from_str_map = {
        { "Never", VK_COMPARE_OP_NEVER },
        { "Less", VK_COMPARE_OP_LESS },
        { "Equal", VK_COMPARE_OP_EQUAL },
        { "LessOrEqual", VK_COMPARE_OP_LESS_OR_EQUAL },
        { "Greater", VK_COMPARE_OP_GREATER },
        { "NotEqual", VK_COMPARE_OP_NOT_EQUAL },
        { "GreaterOrEqual", VK_COMPARE_OP_GREATER_OR_EQUAL },
        { "Always", VK_COMPARE_OP_ALWAYS }
    };

    static const std::unordered_map<std::string, VkDescriptorType> descriptor_type_from_str_map = {
        { "UniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        { "DynamicUniformBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
        { "StorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
        { "DynamicStorageBuffer", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC },
        { "StorageImage", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        { "UniformTexelBuffer", VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
        { "StorageTexelBuffer", VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER },
        { "Sampler", VK_DESCRIPTOR_TYPE_SAMPLER },
        { "SampledImage", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { "CombinedImageSampler", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        { "InputAttachment", VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT }
    };

    size_class sizeClassFromString(const std::string& sz_class)  {
        auto iter = size_class_from_string_map.find(sz_class);
        if (iter == size_class_from_string_map.cend()) {
            LOG(WARNING) << "Failed to find requested size class " << sz_class << " in available size classes.";
            return size_class::Invalid;
        }
        else {
            return iter->second;
        }
    }

    void imageTypeFromStr(const std::string& type_str, VkImageCreateInfo& image_info) {
        if (type_str == "1D") {
            image_info.imageType = VK_IMAGE_TYPE_1D;
            return;
        }
        else if (type_str == "2D") {
            image_info.imageType = VK_IMAGE_TYPE_2D;
            return;
        }
        else if (type_str == "3D") {
            image_info.imageType = VK_IMAGE_TYPE_3D;
            return;
        }
        else if (type_str == "2D_Array") {
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
            return;
        }
        else if (type_str == "CubeMap") {
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            return;
        }
        else {
            throw std::domain_error("Invalid image type.");
        }
    }

    VkSampleCountFlagBits SampleCountEnumFromInt(const int& samples) noexcept {
        auto iter = sample_count_from_int_map.find(samples);
        if (iter != sample_count_from_int_map.cend()) {
            return iter->second;
        }
        else {
            LOG(WARNING) << "Couldn't match requested sample count " << std::to_string(samples) << " to a valid enum value.";
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
        }
    }

    VkFilter filterModeFromStr(const std::string& str) {
        if (str == "Nearest") {
            return VK_FILTER_NEAREST;
        }
        else if (str == "Linear") {
            return VK_FILTER_LINEAR;
        }
        else if (str == "CubicImg") {
            return VK_FILTER_CUBIC_IMG;
        }
        else {
            throw std::domain_error("Invalid filter type string passed to enum conversion method");
        }
    }

    VkSamplerAddressMode addressModeFromStr(const std::string& str) {
        auto iter = address_mode_from_str_map.find(str);
        if (iter != address_mode_from_str_map.cend()) {
            return iter->second;
        }
        else {
            LOG(WARNING) << "Failed to convert string " << str << " to valid VkSamplerAddressMode value";
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
        }
    }

    VkCompareOp compareOpFromStr(const std::string& str) {
        auto iter = compare_op_from_str_map.find(str);
        if (iter != compare_op_from_str_map.cend()) {
            return iter->second;
        }
        else {
            LOG(WARNING) << "Couldn't convert string " << str << " to valid VkCompareOp value.";
            return VK_COMPARE_OP_MAX_ENUM;
        }
    }

    VkBorderColor borderColorFromStr(const std::string& str) {
        if (str == "FloatTransparentBlack") {
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        }
        else if (str == "IntTransparentBlack") {
            return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        }
        else if (str == "FloatOpaqueBlack") {
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        }
        else if (str == "IntOpaqueBlack") {
            return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        }
        else if (str == "FloatOpaqueWhite") {
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        }
        else if (str == "IntOpaqueWhite") {
            return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        }
        else {
            throw std::domain_error("Invalid border color string passed to enum conversion method.");
        }
    }

    ResourceFile::ResourceFile() : environment(std::make_unique<LuaEnvironment>()) {
        using namespace luabridge;
        auto& callbacks = ShaderPack::RetrievalCallbacks();
        getGlobalNamespace(environment->GetState())
            .addFunction("GetWindowX", callbacks.GetScreenSizeX)
            .addFunction("GetWindowY", callbacks.GetScreenSizeY)
            .addFunction("GetZNear", callbacks.GetZNear)
            .addFunction("GetZFar", callbacks.GetZFar)
            .addFunction("GetFieldOfViewY", callbacks.GetFOVY);
    }

    const set_resource_map_t& ResourceFile::GetResources(const std::string & block_name) const {
        return setResources.at(block_name);
    }

    const std::unordered_map<std::string, set_resource_map_t>& ResourceFile::GetAllResources() const noexcept {
        return setResources;
    }

    const ShaderResource * ResourceFile::FindResource(const std::string & name) const {
        for (auto& group : setResources) {
            const ShaderResource* rsrc_ptr = searchSingleGroupForResource(group.first, name);
            if (rsrc_ptr != nullptr) {
                return rsrc_ptr;
            }
        }
        return nullptr;
    }

    void ResourceFile::Execute(const char* fname)  {

        if (luaL_dofile(environment->GetState(), fname)) {
            std::string err = lua_tostring(environment->GetState(), -1);
            LOG(ERROR) << "Failed to execute Lua script " << fname << " - produced errors as follows:\n" << err.c_str();
            throw std::logic_error(err.c_str());
        }

        parseResources();
    }

    const ShaderResource* ResourceFile::searchSingleGroupForResource(const std::string& group, const std::string & name) const {
        
        auto iter = std::find_if(setResources.at(group).cbegin(), setResources.at(group).cend(), [name](const ShaderResource& rsrc) {
            return name == std::string(rsrc.GetName());
        });

        if (iter == setResources.at(group).cend()) {
            return nullptr;
        }
        else {
            return &(*iter);
        }

    }

    VkImageCreateInfo ResourceFile::parseImageOptions(ShaderResource& rsrc, const std::unordered_map<std::string, luabridge::LuaRef>& image_info_table) const {
        // TODO: Flags, usage flags, format, extent
        VkImageCreateInfo results = image_create_info_base;

        auto has_member = [image_info_table](const std::string& name)->bool {
            return image_info_table.count(name) != 0;
        };

        auto member_as_str = [image_info_table](const std::string& name)->std::string {
            return image_info_table.at(name).cast<std::string>();
        };

        if (has_member("SizeClass")) {
            rsrc.SetSizeClass(sizeClassFromString(member_as_str("SizeClass")));
        }

        if (has_member("ImageType")) {
            // Might get cubemaps or arrays as the string, which requires setting certain flags PLUS the type
            imageTypeFromStr(member_as_str("ImageType"), results);
        }

        if (has_member("MipLevels")) {
            results.mipLevels = static_cast<uint32_t>(image_info_table.at("MipLevels").cast<int>());
        }

        if (has_member("ArrayLayers")) {
            results.arrayLayers = static_cast<uint32_t>(image_info_table.at("ArrayLayers").cast<int>());
        }

        if (has_member("SampleCount")) {
            results.samples = SampleCountEnumFromInt(image_info_table.at("SampleCount").cast<int>());
        }

        return results;
    }

    VkSamplerCreateInfo ResourceFile::parseSamplerOptions(const std::unordered_map<std::string, luabridge::LuaRef>& sampler_info_table) const {

        VkSamplerCreateInfo results = sampler_create_info_base;

        auto has_member = [sampler_info_table](const std::string& name)->bool {
            return sampler_info_table.count(name) != 0;
        };

        auto member_as_str = [sampler_info_table](const std::string& name)->std::string {
            return sampler_info_table.at(name).cast<std::string>();
        };

        if (has_member("MagFilter")) {
            results.magFilter = filterModeFromStr(member_as_str("MagFilter"));
        }

        if (has_member("MinFilter")) {
            results.minFilter = filterModeFromStr(member_as_str("MinFilter"));
        }

        if (has_member("AddressModeU")) {
            results.addressModeU = addressModeFromStr(member_as_str("AddressModeU"));
        }

        if (has_member("AddressModeV")) {
            results.addressModeV = addressModeFromStr(member_as_str("AddressModeV"));
        }

        if (has_member("AddressModeW")) {
            results.addressModeW = addressModeFromStr(member_as_str("AddressModeW"));
        }

        if (has_member("EnableAnisotropy")) {
            results.anisotropyEnable = static_cast<VkBool32>(sampler_info_table.at("EnableAnisotropy").cast<bool>());
        }

        if (has_member("MaxAnisotropy")) {
            results.maxAnisotropy = static_cast<float>(sampler_info_table.at("MaxAnisotropy").cast<double>());
        }

        if (has_member("CompareEnable")) {
            results.compareEnable = static_cast<VkBool32>(sampler_info_table.at("CompareEnable").cast<bool>());
        }

        if (has_member("CompareOp")) {
            results.compareOp = compareOpFromStr(member_as_str("CompareOp"));
        }

        if (has_member("MinLod")) {
            results.minLod = static_cast<float>(sampler_info_table.at("MinLOD").cast<double>());
        }

        if (has_member("MaxLod")) {
            results.maxLod = static_cast<float>(sampler_info_table.at("MaxLod").cast<double>());
        }

        if (has_member("BorderColor")) {
            results.borderColor = borderColorFromStr(member_as_str("BorderColor"));
        }

        if (has_member("UnnormalizedCoordinates")) {
            results.unnormalizedCoordinates = static_cast<VkBool32>(sampler_info_table.at("UnnormalizedCoordinates").cast<bool>());
        }

        return results;
    }

    VkBufferViewCreateInfo ResourceFile::getStorageImageBufferViewInfo(ShaderResource& rsrc) const {
        VkBufferViewCreateInfo results = buffer_view_info_base;
        results.format = rsrc.GetFormat();
        results.offset = 0;
        results.range = rsrc.GetAmountOfMemoryRequired();
        return results;
    }

    ShaderResourceSubObject ResourceFile::createSimpleBufferSubresource(const std::string& name, const luabridge::LuaRef& object_ref, uint32_t& offset_total) const {
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
        ShaderResourceSubObject object;
        object.isComplex = false;
        object.Name = strdup(name.c_str());
        std::string type_str = object_ref.cast<std::string>();
        object.Type = strdup(type_str.c_str());

        auto iter = f_tracker.ObjectSizes.find(type_str);
        if (iter != f_tracker.ObjectSizes.cend()) {
            object.Size = static_cast<uint32_t>(iter->second);
            object.Offset = offset_total;
            offset_total += object.Size;
        }
        else {
            LOG(ERROR) << "Could not find size of resource " << type_str << " in programs object sizes map.";
            throw std::runtime_error("Couldn't find resources size.");
        }

        return object;
    }

    ShaderResourceSubObject ResourceFile::createComplexBufferSubresource(const std::string& name, const luabridge::LuaRef& object_ref, uint32_t& offset_total) const {
        // Current member is a complex type, probably an array.
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
        ShaderResourceSubObject object;
        object.isComplex = true;
        auto complex_member_table = environment->GetTableMap(object_ref);

        if (complex_member_table.empty()) {
            LOG(ERROR) << "Could not extract complex/composite member metadata";
            throw std::runtime_error("Failed to extract complex member type from a storage/uniform buffer");
        }

        std::string type_str = complex_member_table.at("Type").cast<std::string>();

        if (type_str == "Array") {
            std::string element_type = complex_member_table.at("ElementType").cast<std::string>();
            size_t num_elements = static_cast<size_t>(complex_member_table.at("NumElements").cast<int>());
            object.NumElements = static_cast<uint32_t>(num_elements);
            object.Offset = offset_total;
           
            auto iter = f_tracker.ObjectSizes.find(element_type);
            if (iter != f_tracker.ObjectSizes.cend()) {
                offset_total += static_cast<uint32_t>(iter->second * num_elements);
            }
            object.Name = strdup(std::string(name + std::string("[]")).c_str());
            object.Type = strdup(element_type.c_str());
            
        }
        else {
            LOG(ERROR) << "Unsupported type for buffer complex/composite member!";
            throw std::domain_error("Unsupported type for uniform buffer complex/composite member!");
        }

        return object;
    }

    std::vector<ShaderResourceSubObject> ResourceFile::getBufferSubobjects(ShaderResource& parent_resource, const std::unordered_map<std::string, luabridge::LuaRef>& subobject_table) const {
        uint32_t offset_total = 0;
        

        std::vector<ShaderResourceSubObject> results;
        results.resize(subobject_table.size());
        for (auto& rsrc : subobject_table) {
            // Each item is a table: contains type, and relative index.
            // Index says where in "results" it should go.
            luabridge::LuaRef type_ref = rsrc.second[1];
            size_t idx = static_cast<size_t>(rsrc.second[2].cast<int>());
            if (!type_ref.isTable()) {
                results[idx] = createSimpleBufferSubresource(rsrc.first, type_ref, offset_total);
            }
            else {
                results[idx] = createComplexBufferSubresource(rsrc.first, type_ref, offset_total);
            }
        }

        parent_resource.SetMemoryRequired(offset_total);
        return results;
    }

    void ResourceFile::setBaseResourceInfo(const std::string& parent_name, const std::string& name, const VkDescriptorType type, ShaderResource& rsrc) const {
        rsrc.SetParentGroupName(parent_name.c_str());
        rsrc.SetName(name.c_str());
        rsrc.SetType(type);
    }

    void ResourceFile::createUniformBufferResources(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        auto buffer_resources = environment->GetTableMap(table.at("Members"));
        auto subobjects = getBufferSubobjects(rsrc, buffer_resources);
        rsrc.SetMembers(subobjects.size(), subobjects.data());
    }

    void ResourceFile::createStorageBufferResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        auto buffer_resources = environment->GetTableMap(table.at("Members"));
        auto subobjects = getBufferSubobjects(rsrc, buffer_resources);
        rsrc.SetMembers(subobjects.size(), subobjects.data());
    }

    void ResourceFile::createStorageTexelBufferResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        std::string format_str = table.at("Format").cast<std::string>();
        rsrc.SetFormat(VkFormatFromString(format_str));
        size_t image_size = static_cast<size_t>(table.at("Size").cast<int>());
        size_t footprint = VkFormatSize(rsrc.GetFormat());
        if (footprint != std::numeric_limits<size_t>::max()) {
            rsrc.SetMemoryRequired(footprint * image_size);
        }
        rsrc.SetBufferViewInfo(getStorageImageBufferViewInfo(rsrc));
    }

    void ResourceFile::createCombinedImageSamplerResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        if (table.count("ImageOptions") != 0) {
            rsrc.SetImageInfo(parseImageOptions(rsrc, environment->GetTableMap(table.at("ImageOptions"))));
        }
        
        if (table.count("SamplerOptions") != 0) {
            rsrc.SetSamplerInfo(parseSamplerOptions(environment->GetTableMap(table.at("SamplerOptions"))));
        }
    }

    void ResourceFile::createSampledImageResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        if (table.count("ImageOptions") != 0) {
            rsrc.SetImageInfo(parseImageOptions(rsrc, environment->GetTableMap(table.at("ImageOptions"))));
        }
    }

    void ResourceFile::createSamplerResource(const std::unordered_map < std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const {
        if (table.count("SamplerOptions") != 0) {
            rsrc.SetSamplerInfo(parseSamplerOptions(environment->GetTableMap(table.at("SamplerOptions"))));
        }
    }

    void ResourceFile::parseResources() {
        using namespace luabridge;

        auto& f_tracker = ShaderFileTracker::GetFileTracker();

        LuaRef size_ref = getGlobal(environment->GetState(), "ObjectSizes");
        if (!size_ref.isNil()) {
            auto size_table = environment->GetTableMap(size_ref);
            for (auto & entry : size_table) {
                size_t size = static_cast<size_t>(entry.second.cast<int>());
                f_tracker.ObjectSizes.emplace(entry.first, std::move(size));
            }
        }

        LuaRef set_table = getGlobal(environment->GetState(), "Resources");
        auto resource_sets = environment->GetTableMap(set_table);

        for (auto& entry : resource_sets) {
            // Now accessing single "group" of resources
            setResources.emplace(entry.first, set_resource_map_t{});

            auto per_set_resources = environment->GetTableMap(entry.second);
            for (auto& set_resource : per_set_resources) {

                // Now accessing/parsing single resource per set
                auto set_resource_data = environment->GetTableMap(set_resource.second);
                std::string type_of_resource = set_resource_data.at("Type");
                ShaderResource resource;

                auto type_iter = descriptor_type_from_str_map.find(type_of_resource);
                if (type_iter == descriptor_type_from_str_map.cend()) {
                    LOG(ERROR) << "Couldn't convert Lua script's resource type string " << type_of_resource << " to valid VkDescriptorType value!";
                    throw std::runtime_error("Failed to convert Lua resource type string to valid Vulkan enum value.");
                }

                const VkDescriptorType vk_type = type_iter->second;
                setBaseResourceInfo(entry.first, set_resource.first, vk_type, resource);

                if (vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                    createUniformBufferResources(set_resource_data, resource);
                }
                else if (vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
                    createStorageBufferResource(set_resource_data, resource);
                }
                else if (vk_type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER || vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
                    createStorageTexelBufferResource(set_resource_data, resource);
                }
                else if (vk_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    createCombinedImageSamplerResource(set_resource_data, resource);
                }
                else if (vk_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                    createSamplerResource(set_resource_data, resource);
                }
                else if (vk_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                    createSampledImageResource(set_resource_data, resource);
                }
                else {
                    LOG(ERROR) << "Requested resource type " << type_of_resource << " is currently unsupported.";
                    throw std::domain_error("Unsupported resource type used.");
                }

                setResources[entry.first].emplace_back(resource);

            }
        }

    }

}