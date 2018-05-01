#include "common/UtilityStructs.hpp"
#include <string>
#include <unordered_map>
namespace st {


    dll_retrieved_strings_t::dll_retrieved_strings_t() {}

    dll_retrieved_strings_t::~dll_retrieved_strings_t() {
        for (uint32_t i = 0; i < NumStrings; ++i) {
            free(Strings[i]);
        }
        delete[] Strings;
    }

    dll_retrieved_strings_t::dll_retrieved_strings_t(dll_retrieved_strings_t && other) noexcept : NumStrings(std::move(other.NumStrings)), Strings(std::move(other.Strings)) {
        other.NumStrings = 0;
        other.Strings = nullptr;
    }

    dll_retrieved_strings_t& dll_retrieved_strings_t::operator=(dll_retrieved_strings_t && other) noexcept {
        NumStrings = std::move(other.NumStrings);
        other.NumStrings = 0;
        Strings = std::move(other.Strings);
        other.Strings = nullptr;
        return *this;
    }

    void dll_retrieved_strings_t::SetNumStrings(const size_t & num_names) {
        NumStrings = num_names;
        Strings = new char*[num_names];
    }
    
    VkFormat StorageImageFormatToVkFormat(const char* fmt) {
        static const std::unordered_map<std::string, VkFormat> formats{
            { "r8ui", VK_FORMAT_R8_UINT },
            { "r8i", VK_FORMAT_R8_SINT },
            { "rg8ui", VK_FORMAT_R8G8_UINT },
            { "rg8i", VK_FORMAT_R8G8_SINT },
            { "rgb8ui", VK_FORMAT_R8G8B8_UINT },
            { "rgb8i", VK_FORMAT_R8G8B8_SINT },
            { "rbga8ui", VK_FORMAT_R8G8B8A8_UINT },
            { "rgba8i", VK_FORMAT_R8G8B8A8_SINT },
            { "r16ui", VK_FORMAT_R16_UINT },
            { "r16i", VK_FORMAT_R16_SINT },
            { "r16f", VK_FORMAT_R16_SFLOAT },
            { "rg16ui", VK_FORMAT_R16G16_UINT },
            { "rg16i", VK_FORMAT_R16G16_SINT },
            { "rg16f", VK_FORMAT_R16G16_SFLOAT },
            { "rgb16ui", VK_FORMAT_R16G16B16_UINT },
            { "rgb16i", VK_FORMAT_R16G16B16_SINT },
            { "rgb16f", VK_FORMAT_R16G16B16_SFLOAT },
            { "rgba16ui", VK_FORMAT_R16G16B16A16_UINT },
            { "rgba16i", VK_FORMAT_R16G16B16A16_SINT },
            { "rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT },
            { "r32ui", VK_FORMAT_R32_UINT },
            { "r32i", VK_FORMAT_R32_SINT },
            { "r32f", VK_FORMAT_R32_SFLOAT },
            { "rg32ui", VK_FORMAT_R32G32_UINT },
            { "rg32i", VK_FORMAT_R32G32_SINT },
            { "rg32f", VK_FORMAT_R32G32_SFLOAT },
            { "rgb32ui", VK_FORMAT_R32G32B32_UINT },
            { "rgb32i", VK_FORMAT_R32G32B32_SINT },
            { "rgb32f", VK_FORMAT_R32G32B32_SFLOAT },
            { "rgba32ui", VK_FORMAT_R32G32B32A32_UINT },
            { "rgba32i", VK_FORMAT_R32G32B32A32_SINT },
            { "rgba32f", VK_FORMAT_R32G32B32A32_SFLOAT }
        };

        auto iter = formats.find(fmt);
        if (iter != formats.cend()) {
            return formats.at(fmt);
        }
        else {
            return VK_FORMAT_UNDEFINED;
        }
    }

    size_t MemoryFootprintForFormat(const VkFormat & fmt) {
        static const std::unordered_map<VkFormat, size_t> format_sizes{
            { VK_FORMAT_R8_UINT, sizeof(uint8_t) },
            { VK_FORMAT_R8_SINT, sizeof(int8_t) },
            { VK_FORMAT_R8G8_UINT, 2 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8_SINT, 2 * sizeof(int8_t) },
            { VK_FORMAT_R8G8B8_UINT, 3 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8_SINT, 3 * sizeof(int8_t) },
            { VK_FORMAT_R8G8B8A8_UINT, 4 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8A8_SINT, 4 * sizeof(int8_t) },
            { VK_FORMAT_R16_UINT, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_SINT, 1 * sizeof(int16_t) },
            { VK_FORMAT_R16_SFLOAT, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_UINT, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_SINT, 2 * sizeof(int16_t) },
            { VK_FORMAT_R16G16_SFLOAT, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_UINT, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_SINT, 3 * sizeof(int16_t) },
            { VK_FORMAT_R16G16B16_SFLOAT, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_UINT, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_SINT, 4 * sizeof(int16_t) },
            { VK_FORMAT_R16G16B16A16_SFLOAT, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R32_UINT, 1 * sizeof(uint32_t) },
            { VK_FORMAT_R32_SINT, 1 * sizeof(int32_t) },
            { VK_FORMAT_R32_SFLOAT, 1 * sizeof(float) },
            { VK_FORMAT_R32G32_UINT, 2 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32_SINT, 2 * sizeof(int32_t) },
            { VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float) },
            { VK_FORMAT_R32G32B32_UINT, 3 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32B32_SINT, 3 * sizeof(int32_t) },
            { VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float) },
            { VK_FORMAT_R32G32B32A32_UINT, 4 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32B32A32_SINT, 4 * sizeof(int16_t) },
            { VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float) }
        };

        auto iter = format_sizes.find(fmt);
        if (iter != format_sizes.cend()) {
            return format_sizes.at(fmt);
        }
        else {
            return std::numeric_limits<size_t>::max();
        }
    }

    ShaderResourceSubObject::ShaderResourceSubObject(const ShaderResourceSubObject & other) noexcept : Type(strdup(other.Type)), Name(strdup(other.Name)), NumElements(other.NumElements),
        isComplex(other.isComplex) {}

    ShaderResourceSubObject::ShaderResourceSubObject(ShaderResourceSubObject && other) noexcept : Type(std::move(other.Type)), Name(std::move(other.Name)), NumElements(std::move(other.NumElements)),
        isComplex(std::move(other.isComplex)) {
        other.Type = nullptr;
        other.Name = nullptr;
    }

    ShaderResourceSubObject & ShaderResourceSubObject::operator=(const ShaderResourceSubObject & other) noexcept {
        Type = strdup(other.Type);
        Name = strdup(other.Name);
        NumElements = other.NumElements;
        isComplex = other.isComplex;
        return *this;
    }

    ShaderResourceSubObject & ShaderResourceSubObject::operator=(ShaderResourceSubObject && other) noexcept {
        Type = std::move(other.Type);
        Name = std::move(other.Name);
        NumElements = std::move(other.NumElements);
        isComplex = std::move(other.isComplex);
        other.Type = nullptr;
        other.Name = nullptr;
        return *this;
    }

    ShaderResourceSubObject::~ShaderResourceSubObject() {
        if (Type != nullptr) {
            free(Type);
        }
        if (Name != nullptr) {
            free(Name);
        }
    }

}