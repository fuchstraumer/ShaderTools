#include "FilesystemUtils.hpp"
#include <map>
#include <fstream>
#include <mutex>
#include <unordered_map>
namespace st {

    namespace fs = std::experimental::filesystem;

    fs::path OutputPath = fs::temp_directory_path();
    std::unordered_map<ShaderStage, std::string> shaderFiles = std::unordered_map<ShaderStage, std::string>{ };
    std::unordered_map<ShaderStage, std::vector<uint32_t>> shaderBinaries = std::unordered_map<ShaderStage, std::vector<uint32_t>>{ };
    std::unordered_multimap<ShaderStage, fs::path> shaderPaths = std::unordered_multimap<ShaderStage, fs::path>{};
    
    static const std::map<VkShaderStageFlagBits, std::string> stage_extension_map {
        { VK_SHADER_STAGE_VERTEX_BIT, ".vert" },
        { VK_SHADER_STAGE_FRAGMENT_BIT, ".frag" },
        { VK_SHADER_STAGE_GEOMETRY_BIT, ".geom" },
        { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ".teval" },
        { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, ".tcntl" },
        { VK_SHADER_STAGE_COMPUTE_BIT, ".comp" }
    };

    static const std::map<std::string, VkShaderStageFlagBits> extension_stage_map{
        { ".vert", VK_SHADER_STAGE_VERTEX_BIT },
        { ".frag", VK_SHADER_STAGE_FRAGMENT_BIT },
        { ".geom", VK_SHADER_STAGE_GEOMETRY_BIT },
        { ".teval", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
        { ".tcntl", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { ".comp", VK_SHADER_STAGE_COMPUTE_BIT }
    };

    ShaderStage WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlagBits stage) {
        const std::string output_name = file_name + stage_extension_map.at(stage);
        const fs::path output_path = OutputPath / fs::path(output_name);
        
        std::ofstream output_stream(output_path);

        if (!output_stream.is_open()) {
            throw std::runtime_error("Failed to open output stream for writing shader source file!");
        }
        output_stream << file_contents;
        output_stream.close();

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard{ insertion_mutex };
        auto c_str_tmp = output_path.string();
        auto inserted = shaderFiles.emplace(ShaderStage{ c_str_tmp.c_str(), stage }, file_contents);
        if (!inserted.second) {
            throw std::runtime_error("Failed to insert Shader + shader file contents into a map.");
        }

        ShaderStage result{ c_str_tmp.c_str(), stage };
        shaderPaths.emplace(result, output_path);

        return result;
    }
    
    void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlagBits stage) {
        const std::string extension = fs::path(base_name).extension().string();

        std::string output_name = base_name;
        if (extension_stage_map.count(extension) == 0) {
            output_name += stage_extension_map.at(stage);
        }

        const fs::path output_path = OutputPath / fs::path(output_name);
        output_name += std::string(".spv"); // Don't want exact same name for actual save: just same hash uint32_t val

        std::ofstream output_stream(output_name, std::ios::binary);
        if (!output_stream.is_open()) {
            throw std::runtime_error("Could not open output stream for writing shader binary file!");
        }
        
        for (auto& val : file_contents) {
            output_stream << val;
        }

        output_stream.close();

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard{ insertion_mutex };
        auto c_str_tmp = output_path.string();
        auto inserted = shaderBinaries.emplace(ShaderStage{ c_str_tmp.c_str(), stage }, file_contents);
        
    }

    bool ShaderSourceNewerThanBinary(const ShaderStage& handle) {
        if (shaderPaths.count(handle) == 0) {
            return true;
        }

        const auto& range = shaderPaths.equal_range(handle);
        fs::path source_path, binary_path;

        for (auto iter = range.first; iter != range.second; ++iter) {
            if (iter->second.extension().string() == std::string{ ".spv" }) {
                binary_path = iter->second;
            }
            else {
                source_path = iter->second;
            }
        }

        const fs::file_time_type source_mod_time(fs::last_write_time(source_path));
        if (source_mod_time == fs::file_time_type::min()) {
            throw std::runtime_error("File write time for a source shader file is invalid - suggests invalid path passed to checker method!");
        }

        const fs::file_time_type binary_mod_time(fs::last_write_time(binary_path));
        if (binary_mod_time == fs::file_time_type::min()) {
            return true;
        }
        else {
            // don't check equals, as they could be equal (very nearly, at least)
            return binary_mod_time < source_mod_time;
        }
    }

    std::string GetOutputDirectory() {
        return OutputPath.string();
    }

    void SetOutputDirectory(const std::string& output_dir) {
        fs::path new_output_path(output_dir);

        if (fs::exists(new_output_path)) {
            OutputPath = fs::canonical(new_output_path);
        }
        else {
            throw std::domain_error("Passed invalid path to SetOutputDirectory");
        }
    }
    
}
