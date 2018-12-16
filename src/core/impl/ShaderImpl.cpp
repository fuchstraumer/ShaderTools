#include "ShaderImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "reflection/ShaderReflector.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include <experimental/filesystem>

namespace st {

    namespace fs = std::experimental::filesystem;

    ShaderGroupImpl::ShaderGroupImpl(const std::string& group_name, size_t num_extensions, const char* const* extensions, size_t num_includes, const char* const* include_paths) : groupName(group_name), compiler(std::make_unique<ShaderCompiler>()),
        reflector(std::make_unique<ShaderReflector>()) {
        for (size_t i = 0; i < num_includes; ++i) {
            includePaths.emplace_back(include_paths[i]);
        }
        for (size_t i = 0; i < num_extensions; ++i) {
            extensionStrCopies.emplace_back(std::string(extensions[i]));
            extensionStrPtrs.emplace_back(extensionStrCopies.back().c_str());
        }
    }

    ShaderGroupImpl::~ShaderGroupImpl() { }

    ShaderGroupImpl::ShaderGroupImpl(ShaderGroupImpl && other) noexcept : includePaths(std::move(other.includePaths)), stHandles(std::move(other.stHandles)), generator(std::move(other.generator)),
        compiler(std::move(other.compiler)), reflector(std::move(other.reflector)), rsrcFile(std::move(other.rsrcFile)), groupName(std::move(other.groupName)), resourceScriptPath(std::move(other.resourceScriptPath)) {}

    ShaderGroupImpl & ShaderGroupImpl::operator=(ShaderGroupImpl && other) noexcept {
        includePaths = std::move(other.includePaths);
        stHandles = std::move(other.stHandles);
        generator = std::move(other.generator);
        compiler = std::move(other.compiler);
        reflector = std::move(other.reflector);
        rsrcFile = std::move(other.rsrcFile);
        groupName = std::move(other.groupName);
        resourceScriptPath = std::move(other.resourceScriptPath);
        return *this;
    }

    void ShaderGroupImpl::addShaderStage(const ShaderStage& handle, std::string src_str_path) {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();

        size_t completed_src_size = 0;
        std::string completed_src_str;
        completed_src_str.reserve(4092);
        bool need_compile{ false };

        if (FileTracker.FullSourceStrings.count(handle) != 0) {
            // might have already generated string, lets double check write time
            fs::path actual_path{ src_str_path };
            if (!fs::exists(actual_path)) {
                throw std::runtime_error("Path to body source string for a shader does not exist!");
            }

            auto curr_write_time = fs::last_write_time(actual_path);
            if (curr_write_time > FileTracker.StageLastModificationTimes.at(handle)) {
                generateFullShaderSource(handle, src_str_path, completed_src_size, completed_src_str);
                need_compile = true;
            }
            else {
                completed_src_size = FileTracker.FullSourceStrings.at(handle).length();
                completed_src_str = FileTracker.FullSourceStrings.at(handle);
            }
        }
        else {
            need_compile = true;
            generateFullShaderSource(handle, src_str_path, completed_src_size, completed_src_str);
        }

        completed_src_str.shrink_to_fit();
        const std::string& name = FileTracker.ShaderNames.at(handle);

        if ((FileTracker.Binaries.count(handle) != 0) && !need_compile) {
            reflector->ParseBinary(handle);
        }
        else {
            compiler->Compile(handle, name.c_str(), completed_src_str.c_str(), completed_src_size);
            std::string recompiled_src_str; size_t size = 0;
            compiler->RecompileBinaryToGLSL(handle, &size, nullptr);
            recompiled_src_str.resize(size);
            compiler->RecompileBinaryToGLSL(handle, &size, recompiled_src_str.data());
            reflector->ParseBinary(handle);
        }

        // Need to reset generator to neutral state each time.
        generator.reset();
    }

    void ShaderGroupImpl::generateFullShaderSource(const ShaderStage & handle, const std::string & src_path, size_t & dest_src_sz, std::string & src_str) {
        generator = std::make_unique<ShaderGenerator>(handle.GetStage());
        generator->SetResourceFile(rsrcFile);
        generator->Generate(handle, src_path.c_str(), extensionStrPtrs.size(), extensionStrPtrs.data(), includePaths.size(), includePaths.data());
        generator->GetFullSource(&dest_src_sz, nullptr);
        src_str.resize(dest_src_sz);
        generator->GetFullSource(&dest_src_sz, src_str.data());
    }

}