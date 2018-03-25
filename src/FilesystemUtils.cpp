#include "FilesystemUtils.hpp"
#include <map>
#include <fstream>
#include <mutex>
#include <unordered_map>
namespace st {

    namespace fs = std::experimental::filesystem;

    std::unordered_map<Shader, std::string> shaderFiles = std::unordered_map<Shader, std::string>{ };
    std::unordered_map<Shader, std::vector<uint32_t>> shaderBinaries = std::unordered_map<Shader, std::vector<uint32_t>>{ };
    
    
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

    Shader WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlagBits stage) {
        const std::string output_name = file_name + stage_extension_map.at(stage);
        const fs::path output_path = fs::temp_directory_path() / fs::path(file_name);
        
        std::ofstream output_stream(output_path);

        if (!output_stream.is_open()) {
            throw std::runtime_error("Failed to open output stream for writing shader source file!");
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard{ insertion_mutex };
        auto c_str_tmp = output_path.string();
        auto inserted = shaderFiles.emplace(Shader{ c_str_tmp.c_str(), stage }, file_contents);

        output_stream << file_contents;
        output_stream.close();

        return Shader{ c_str_tmp.c_str(), stage };
    }
    
    void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlagBits stage) {
        const std::string extension = fs::path(base_name).extension().string();

        std::string output_name = base_name;
        if (extension_stage_map.count(extension) == 0) {
            output_name += stage_extension_map.at(stage);
        }

        const fs::path output_path = fs::temp_directory_path() / fs::path(output_name);
        output_name += std::string(".spv"); // Don't want exact same name for actual save: just same hash uint32_t val
        std::ofstream output_stream(output_name, std::ios::binary);

        if (!output_stream.is_open()) {
            throw std::runtime_error("Could not open output stream for writing shader binary file!");
        }
        
        for (auto& val : file_contents) {
            output_stream << val;
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard{ insertion_mutex };
        auto c_str_tmp = output_path.string();
        auto inserted = shaderBinaries.emplace(Shader{ c_str_tmp.c_str(), stage }, file_contents);
        output_stream.close();
        
    }

}