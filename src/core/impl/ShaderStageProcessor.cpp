#include "ShaderStageProcessor.hpp"
#include "../../generation/impl/ShaderGeneratorImpl.hpp"
#include "../../generation/impl/CompilerImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include <experimental/filesystem>
#include "easyloggingpp/src/easylogging++.h"

namespace st {
    namespace fs = std::experimental::filesystem;

    ShaderStageProcessor::ShaderStageProcessor(ShaderStage _stage, yamlFile* rfile) : stage(std::move(_stage)), rsrcFile(rfile),
        generator(std::make_unique<ShaderGeneratorImpl>(_stage)), compiler(std::make_unique<ShaderCompilerImpl>()) {
        generator->resourceFile = rsrcFile;
    }

    ShaderStageProcessor::~ShaderStageProcessor() {}

    void ShaderStageProcessor::Process(std::string body_path, const std::vector<std::string>& extensions, const std::vector<std::string>& includes) {
        bodyPath = std::move(body_path);
        Generate(bodyPath, extensions, includes);
        Compile();
    }

    const std::string& ShaderStageProcessor::Generate(const std::string& body_path_str, const std::vector<std::string>& extensions, const std::vector<std::string>& includes) {
        auto& ftracker = ShaderFileTracker::GetFileTracker();

        if (ftracker.BodyPaths.count(stage) == 0) {
            ftracker.BodyPaths.emplace(stage, body_path_str);
        }

        for (const auto& inc : includes) {
            generator->includes.emplace_back(inc);
        }

        for (const auto& ext : extensions) {
            generator->addExtension(ext);
        }

        // Decide if we're gonna generate
        if (ftracker.FullSourceStrings.count(stage) != 0) {

            fs::path actual_path{ body_path_str };
            if (!fs::exists(actual_path)) {
                throw std::runtime_error("Given body path was invalid!");
            }
            actual_path = fs::canonical(actual_path);

            // Might have potentially generated already. Lets check timestamps.
            auto curr_write_time = fs::last_write_time(actual_path);
            auto stored_write_time = ftracker.StageLastModificationTimes.at(stage);

            if (curr_write_time > stored_write_time) {
                // Update path: might have changed and we'll use this from now on
                ftracker.BodyPaths[stage] = actual_path;
                // Also update last write time
                ftracker.StageLastModificationTimes[stage] = curr_write_time;
                // Get rid of current shader body and generated source to force re-load when we call generate()
                ftracker.ShaderBodies.erase(stage);
                ftracker.FullSourceStrings.erase(stage);
                if (ftracker.Binaries.count(stage) != 0) {
                    ftracker.Binaries.erase(stage);
                }
            }
            else {
                // write time hasn't changed, we already have the full source string we need, just return it
                return ftracker.FullSourceStrings.at(stage);
            }
        }

        generator->generate(stage, body_path_str, 0u, nullptr);
        return generator->getFullSource();
    }

    const std::vector<uint32_t>& ShaderStageProcessor::Compile() {
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        if (ftracker.FullSourceStrings.count(stage) == 0) {
            LOG(ERROR) << "Attempted to compile shader, but full source string has not been generated!";
            if (ftracker.BodyPaths.count(stage) != 0) {
                // we'll try to generate it at least
                try {
                    Generate(ftracker.BodyPaths.at(stage).string(), {}, {});
                }
                catch (const std::exception& e) {
                    LOG(ERROR) << "Attempting to generate shader before compiliation failed: " << e.what();
                    throw e;
                }
            }
        }
        
        if (ftracker.Binaries.count(stage) != 0) {
            return ftracker.Binaries.at(stage);
        }

        compiler->prepareToCompile(stage, ftracker.GetShaderName(stage), ftracker.FullSourceStrings.at(stage));
        return ftracker.Binaries.at(stage);
    }

}
