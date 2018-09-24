#include "ShaderImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "generation/Compiler.hpp"
#include "generation/ShaderGenerator.hpp"
#include "reflection/ShaderReflector.hpp"
#include "easyloggingpp/src/easylogging++.h"

namespace st {


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
        generator = std::make_unique<ShaderGenerator>(handle.GetStage());

        generator->SetResourceFile(rsrcFile);
        generator->Generate(handle, src_str_path.c_str(), extensionStrPtrs.size(), extensionStrPtrs.data(), includePaths.size(), includePaths.data());
        size_t completed_src_size = 0;
        generator->GetFullSource(&completed_src_size, nullptr);
        std::string completed_src_str; completed_src_str.resize(completed_src_size);
        generator->GetFullSource(&completed_src_size, completed_src_str.data());

        const std::string& name = FileTracker.ShaderNames.at(handle);
        compiler->Compile(handle, name.c_str(), completed_src_str.c_str(), completed_src_size);
        std::string recompiled_src_str; size_t size = 0;
        compiler->RecompileBinaryToGLSL(handle, &size, nullptr);
        recompiled_src_str.resize(size);
        compiler->RecompileBinaryToGLSL(handle, &size, recompiled_src_str.data());

        reflector->ParseBinary(handle);

        // Need to reset generator to neutral state each time.
        generator.reset();
    }

}