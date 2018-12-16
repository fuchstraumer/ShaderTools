#include "ShaderStageProcessor.hpp"
#include "../../generation/impl/ShaderGeneratorImpl.hpp"
#include "../../generation/impl/CompilerImpl.hpp"

namespace st {

    ShaderStageProcessor::ShaderStageProcessor(ShaderStage _stage, ResourceFile * rfile) : stage(std::move(_stage)), rsrcFile(rfile),
        generator(std::make_unique<ShaderGeneratorImpl>(_stage.GetStage())), compiler(std::make_unique<ShaderCompilerImpl>()) {
        generator->luaResources = rsrcFile;
    }

    ShaderStageProcessor::~ShaderStageProcessor() {}

}
