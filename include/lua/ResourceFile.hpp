#pragma once
#ifndef ST_RESOURCE_FILE_HPP
#define ST_RESOURCE_FILE_HPP
#include "common/CommonInclude.hpp"
#include "LuaEnvironment.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <map>
namespace st {

    struct UniformBuffer {
        std::vector<std::pair<std::string, std::string>> MemberTypes;
    };

    struct StorageBuffer {
        std::string ElementType;
        size_t NumElements;
    };

    struct StorageImage {
        std::string Format;
        size_t Size;
    };

    struct Texture {
        enum class texture_type {
            e1D,
            e2D,
            e3D,
            e1D_Array,
            e2D_Array,
            eCubeMap
        } TextureType{ texture_type::e2D };
        enum class size_class {
            SwapchainRelative,
            Absolute
        };
    };

    using lua_resource_t = std::variant<
        UniformBuffer,
        StorageBuffer,
        StorageImage,
        Texture
    >;

    using set_resource_map_t = std::map<std::string, lua_resource_t>;

    class ResourceFile {
        ResourceFile(const ResourceFile&) = delete;
        ResourceFile& operator=(const ResourceFile&) = delete;
    public:

        ResourceFile(LuaEnvironment* _env);
        void Execute(const char* fname);
        const bool& IsReady() const noexcept;
        const set_resource_map_t& GetResources(const std::string& block_name) const;

    private:
        bool ready{ false };
        LuaEnvironment* env{ nullptr };
        std::unordered_map<std::string, set_resource_map_t> setResources;
        void parseResources();
    };

}

#endif //!ST_RESOURCE_FILE_HPP