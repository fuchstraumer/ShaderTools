#include "DescriptorStructs.hpp"
#include "../util/ResourceFormats.hpp"

namespace st {

    std::string VertexAttributeInfo::GetTypeStr() const {
        return SPIR_TypeToString(Type);
    }

    void VertexAttributeInfo::SetTypeWithStr(std::string str) {
        Type = SPIR_TypeFromString(str);
    }

    VertexAttributeInfo::operator VkVertexInputAttributeDescription() const {
        return VkVertexInputAttributeDescription{ Location, 0, VkFormatFromSPIRType(Type), Offset };
    }

    void StageAttributes::SetStageWithStr(std::string str) {
        Stage = GetShaderStageFromString(str);
    }

    std::string StageAttributes::GetStageStr() const {
        return GetShaderStageString(Stage);
    }

    PushConstantInfo::operator VkPushConstantRange() const noexcept {
        VkPushConstantRange result;
        result.stageFlags = Stages;
        result.offset = Offset;
        uint32_t size = 0;
        for (auto& obj : Members) {
            size += obj.Size;
        }
        result.size = size;
        return result;
    }

}