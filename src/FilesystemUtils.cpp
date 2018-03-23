#include "FilesystemUtils.hpp"
#include <map>
#include <fstream>
#include <mutex>
#include <unordered_map>
namespace st {

    namespace fs = std::experimental::filesystem;

    std::unordered_map<uint32_t, std::string> shaderFiles = std::unordered_map<uint32_t, std::string>{ };
    std::unordered_map<uint32_t, std::vector<uint32_t>> shaderBinaries = std::unordered_map<uint32_t, std::vector<uint32_t>>{ };
    
    
    static const std::map<VkShaderStageFlags, std::string> stage_extension_map {
        { VK_SHADER_STAGE_VERTEX_BIT, ".vert" },
        { VK_SHADER_STAGE_FRAGMENT_BIT, ".frag" },
        { VK_SHADER_STAGE_GEOMETRY_BIT, ".geom" },
        { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ".teval" },
        { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, ".tcntl" },
        { VK_SHADER_STAGE_COMPUTE_BIT, ".comp" }
    };

    ShaderHandle WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlags stage) {
        const std::string output_name = file_name + stage_extension_map.at(stage);
        const fs::path output_path = fs::temp_directory_path() / fs::path(file_name);
        const uint32_t output_hash = GetPathHash(output_path);
        std::ofstream output_stream(output_path);

        if (!output_stream.is_open()) {
            throw std::runtime_error("Failed to open output stream for writing shader source file!");
        }

        static std::mutex insertion_mutex;
        std::lock_guard<std::mutex> insertion_guard{ insertion_mutex };
        auto inserted = shaderFiles.emplace(output_hash, file_contents);

        output_stream << file_contents;
        output_stream.close();

        return ShaderHandle{ GetPathHash(output_path), stage };
    }
    
    void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlags stage) {
        
        std::string output_name = base_name + stage_extension_map.at(stage);
        const fs::path output_path = fs::temp_directory_path() / fs::path(output_name);
        const uint32_t output_hash = GetPathHash(output_path);
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
        auto inserted = shaderBinaries.emplace(output_hash, file_contents);

        output_stream.close();
        
    }

}