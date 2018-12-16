#include "ShaderStageProcessor.hpp"
#include "../../generation/impl/ShaderGeneratorImpl.hpp"
#include "../../generation/impl/CompilerImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include <experimental/filesystem>

namespace st {
    namespace fs = std::experimental::filesystem;

    ShaderStageProcessor::ShaderStageProcessor(ShaderStage _stage, ResourceFile * rfile, std::string name) : stage(std::move(_stage)), rsrcFile(rfile),
        generator(std::make_unique<ShaderGeneratorImpl>(_stage.GetStage())), compiler(std::make_unique<ShaderCompilerImpl>()) {
        generator->luaResources = rsrcFile;
    }

    ShaderStageProcessor::~ShaderStageProcessor() {}

    const std::string& ShaderStageProcessor::Generate(const std::string & body_path, const std::vector<std::string>& extensions, const std::vector<std::string>& includes) {
        auto& ftracker = ShaderFileTracker::GetFileTracker();

        // Decide if we're gonna generate
        if (ftracker.FullSourceStrings.count(stage) != 0) {
            // Might have potentially generated already. Lets check timestamps.
            fs::path actual_path{ body_path };
            if (!fs::exists(actual_path)) {
                throw std::runtime_error("Given body path was invalid!");
            }

            actual_path = fs::canonical(actual_path);

            auto curr_write_time = fs::last_write_time(actual_path);
            auto stored_write_time = ftracker.StageLastModificationTimes.at(stage);

            if (curr_write_time > stored_write_time) {
                // Update path: might have changed and we'll use this from now on
                ftracker.BodyPaths[stage] = actual_path;
                // Also update last write time
                ftracker.StageLastModificationTimes[stage] = curr_write_time;
                // Get rid of current shader body to force re-load when we call generate()
                ftracker.ShaderBodies.erase(stage);
            }
        }
        // TODO: insert return statement here
    }

}
