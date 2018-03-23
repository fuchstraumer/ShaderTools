#pragma once
#ifndef ST_SHADER_HPP
#define ST_SHADER_HPP
#include "CommonInclude.hpp"

namespace st {

    struct ST_API Shader {
        uint32_t ID;
        VkShaderStageFlagBits GetStage() const noexcept;
    };

}

#endif //!ST_SHADER_HPP