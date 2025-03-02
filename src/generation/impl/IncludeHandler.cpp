#include "IncludeHandler.hpp"
#include <fstream>
#include <string>
#include <format>

namespace st
{
    std::vector<std::filesystem::path> IncludeHandler::libaryIncludePaths;

    
    IncludeHandler::IncludeHandler(const std::vector<std::filesystem::path>& include_paths, SessionImpl* error_session) noexcept : includePaths(include_paths), errorSession(error_session) {}

    shaderc_include_result* IncludeHandler::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
    {
        namespace fs = std::filesystem;
        fs::path found_include_path;
        bool found_include = false;

        if (type == shaderc_include_type_relative)
        {
            // Relative include, pull from the requesting source directory
            fs::path requesting_path = fs::path(requesting_source).parent_path();
            found_include_path = requesting_path / fs::path(requested_source);
            if (fs::exists(found_include_path))
            {
                found_include = true;
            }
            else
            {
                // scan through the include paths
                for (const auto& include_path : includePaths)
                {
                    fs::path candidate = include_path / fs::path(requested_source);
                    if (fs::exists(candidate))
                    {
                        found_include_path = candidate;
                        found_include = true;
                        break;
                    }
                }
            }
        }
        else if (type == shaderc_include_type_standard)
        {
            // Library include, pull from our cache of includes
            for (const auto& include_path : libaryIncludePaths)
            {
                fs::path candidate = include_path / fs::path(requested_source);
                if (fs::exists(candidate))
                {
                    found_include_path = candidate;
                    found_include = true;
                    break;
                }
            }
        }

        if (!found_include)
        {
            std::string error_message = std::format("Failed to find include file {} for {}.", requested_source, requesting_source);
            errorSession->AddError(this, ShaderToolsErrorSource::IncludeHandler, ShaderToolsErrorCode::IncludeHandlerFileNotFound, error_message.c_str());
            return nullptr;
        }

        // Let's just make sure the file path is absolute, since that's what shaderc says it wants anyways
        found_include_path = fs::absolute(found_include_path);
        std::ifstream include_file(found_include_path);
        if (!include_file.is_open())
        {
            std::string error_message = std::format("Failed to open include file: {}", found_include_path.string());
            errorSession->AddError(this, ShaderToolsErrorSource::IncludeHandler, ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened, error_message.c_str());
        }

        std::string include_content{ (std::istreambuf_iterator<char>(include_file)), std::istreambuf_iterator<char>() };

        // We have to allocate this because shaderc owns it and we also have to use raw pointers :( this makes the fox sad
        shaderc_include_result* result = new shaderc_include_result();
        char* content_ptr = new char[include_content.size()];
        std::strcpy(content_ptr, include_content.c_str());
        result->content = content_ptr;
        result->content_length = include_content.size();

        const std::string source_name_str = found_include_path.string();
        char* source_name_ptr = new char[source_name_str.size()];
        std::strcpy(source_name_ptr, source_name_str.c_str());
        result->source_name = source_name_ptr;
        result->source_name_length = source_name_str.size();

        return result;
    }
    
    void IncludeHandler::ReleaseInclude(shaderc_include_result* data)
    {
        if (data != nullptr)
        {
            delete[] data->content;
            delete[] data->source_name;
            delete data;
        }
    }


}
