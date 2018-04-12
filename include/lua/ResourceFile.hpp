#pragma once
#ifndef ST_RESOURCE_FILE_HPP
#define ST_RESOURCE_FILE_HPP
#include "common/CommonInclude.hpp"
#include "LuaEnvironment.hpp"
#include <functional>
namespace st {

    struct engine_environment_callbacks_t {
        std::function<int()> GetScreenSizeX;
        std::function<int()> GetScreenSizeY;
        std::function<double()> GetZNear;
        std::function<double()> GZFar;
        std::function<double()> GetFOVY;
    };

    struct UniformBuffer {
        std::vector<std::string> MemberTypes;
    };

    struct StorageBuffer {
        std::string ElementType;
        size_t NumElements;
    };

    struct StorageImage {
        enum class data_format {
            Unsigned,
            Signed,
            Float,
            Invalid 
        } DataFormat{ data_format::Invalid };
        size_t Size;
    };

    struct Texture {
        std::string Name;
        enum class texture_type {
            e1D,
            e2D,
            e3D,
            e1D_Array,
            e2D_Array,
            eCubeMap
        } TextureType{ texture_type::e2D };
    };

    class ResourceFile {
    public:
        ResourceFile(const char* fname, LuaEnvironment* _env);
        
        engine_environment_callbacks_t RetrievalCallbacks;
    private:
        LuaEnvironment * env;
    };

}

#endif //!ST_RESOURCE_FILE_HPP