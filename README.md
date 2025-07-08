# ShaderTools - A Vulkan Shader Toolchain

ShaderTools is a modern Vulkan-oriented shader toolchain that dramatically simplifies shader development and Vulkan resource management through intelligent code generation and comprehensive reflection capabilities.

## Key Features

- **Smart Shader Generation**: Write only the essential shader logic - resources, bindings, and boilerplate are generated automatically
- **Advanced Reflection System**: Powered by SPIRV-Reflect, extracts complete metadata about resources, vertex layouts, push constants, and specialization constants
- **Automatic Vulkan Integration**: Generate exact `VkDescriptorPool`s, `VkDescriptorSetLayout`s, and `VkPipelineLayout`s directly from shader analysis
- **Performance Optimized**: Binary caching system reduces compilation times by up to 10x
- **C++23 Modern Design**: Built with modern C++ practices and comprehensive error handling

## How It Works

ShaderTools operates through three main phases:

1. **Generation**: Transform minimal shader source into complete GLSL with automatic resource declarations and `#include` resolution
2. **Compilation**: Leverage the `shaderc` optimizing SPIR-V compiler for efficient binary generation  
3. **Reflection**: Extract comprehensive metadata using SPIRV-Reflect to automate Vulkan resource management

This eliminates the traditional coupling between compiled C++ code and shader resources - your application adapts dynamically to shader changes without requiring recompilation.

## Shader Generation and Resource Management

One of ShaderTools' most powerful features is eliminating verbose Vulkan resource declarations. Instead of writing complex binding specifications like:

```glsl
// Traditional verbose approach
layout (set = 1, binding = 0, rgba8) readonly restrict uniform imageBuffer lightColors;
layout (set = 1, binding = 1, rgba32f) restrict uniform imageBuffer positionRanges;
layout (set = 2, binding = 0, r32ui) restrict uniform uimageBuffer lightCountTotal;
layout (set = 2, binding = 1, r32ui) restrict uniform uimageBuffer lightBounds;
layout (set = 3, binding = 0) uniform MaterialBlock {
    vec4 diffuse;
    vec4 specular;
    float roughness;
    float metallic;
} Material;
layout (set = 3, binding = 1) uniform sampler2D normalMap;
layout (set = 3, binding = 2) uniform sampler2D metallicMap;
```

You simply specify resource usage at the top of your shader:

```glsl
#pragma USE_RESOURCES Lights
#pragma USE_RESOURCES ClusteredForward  
#pragma USE_RESOURCES Material
```

ShaderTools automatically generates the complete resource declarations, handles set/binding assignments, and maintains consistency across multiple shader stages.

### Specialization Constants

Specialization constants are simplified with automatic ID management:

```glsl
// Traditional approach requires manual ID tracking
layout(constant_id = 0) const int MAX_LIGHTS = 256;
layout(constant_id = 1) const bool ENABLE_SHADOWS = true;

// ShaderTools approach - just prefix with #SPC
#SPC const int MAX_LIGHTS = 256;
#SPC const bool ENABLE_SHADOWS = true;
```

### Include System

Full `#include` support allows modular shader development:

```glsl
#include "Structures.glsl"
#include "LightingFunctions.glsl"
```

## Advanced Reflection System

ShaderTools uses **SPIRV-Reflect** (replacing the previous SPIRV-Cross dependency) to extract comprehensive metadata from compiled shaders. This modern reflection system provides:

- **Complete Resource Information**: Descriptor types, binding indices, set assignments, and access patterns
- **Vertex Input/Output Layouts**: Automatic location assignment and format detection
- **Push Constants**: Full structure layout with member offsets and sizes
- **Specialization Constants**: ID mapping and type information
- **Interface Variables**: Input/output variables with location and format data

Instead of manually defining descriptor layouts:

```cpp
// Traditional manual approach
texelBuffersLayout->AddDescriptorBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, fc_flags, 0);
texelBuffersLayout->AddDescriptorBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, fc_flags, 1);
texelBuffersLayout->AddDescriptorBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, fc_flags, 2);
// ... repeat for each binding
```

ShaderTools extracts this information automatically:

```cpp
// Automatic extraction from reflection
const size_t num_descriptor_sets = reflector->GetNumSets();
std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(num_descriptor_sets);

for (size_t i = 0; i < num_descriptor_sets; ++i) {
    size_t num_resources = 0;
    reflector->GetShaderResources(i, &num_resources, nullptr);
    
    std::vector<ResourceUsage> resources(num_resources);
    reflector->GetShaderResources(i, &num_resources, resources.data());
    
    // Convert to VkDescriptorSetLayoutBinding automatically
    for (const auto& resource : resources) {
        setBindings[i].push_back(resource); // Implicit conversion
    }
}
```

### Descriptor Pool Creation

The reflection system provides exact descriptor type counts, enabling precise `VkDescriptorPool` creation without guesswork:

```cpp
// Get exact requirements from multiple shaders
descriptor_type_counts_t totalCounts = {};
for (const auto& shader : shaderPack) {
    auto shaderCounts = shader.GetDescriptorCounts();
    // Accumulate counts for pool sizing
}
```

## Performance & Caching

ShaderTools includes a basic binary caching system that dramatically improves build times:

- **Timestamp Checking**: Only recompiles shaders when source files have changed
- **Binary Serialization**: Saves complete shader metadata to disk for instant loading
- **~10-100x Faster Binary Load**: Unsurprisingly, reading the binary shaderpack data from disk is much much faster than compiling from scratch

The caching system still needs some further improvements to make sure that updating the .yaml schema file or included shader files also propagates an update to the relevant shaders, but that is planned work.

## Project Architecture

### Core Classes

- **`ShaderStage`**: Represents a single programmable pipeline stage with hashed identification
- **`Shader`**: Groups related `ShaderStage` objects for complete rendering operations  
- **`ShaderPack`**: Collections of `Shader` objects with shared resources for complex rendering systems
- **`ShaderResource`**: Individual resources with comprehensive metadata from reflection analysis
- **`ResourceGroup`**: Logical collections of `ShaderResource` objects (equivalent to descriptor sets)
- **`ShaderReflector`**: Main reflection interface powered by SPIRV-Reflect

### Error Handling & Debugging

ShaderTools now features comprehensive error handling with detailed diagnostics:

- **Structured Error Reporting**: Clear error messages with source locations
- **Session-based Error Management**: Centralized error collection and reporting
- **Debug Information Preservation**: Maintains source mappings for debugging compiled shaders

## Current Capabilities & Future Development

### Recently Added Features (2024-2025)

- **SPIRV-Reflect Integration**: Replaced SPIRV-Cross with SPIRV-Reflect, which is a more lightweight library in general
- **Enhanced Include System**: `#include` processing with dependency tracking  
- **Improved Error Handling**: Session-based error reporting attached to each compile/generation/reflection session
- **Push Constant Support**: Full push constant reflection
- **Specialization Constants**: Complete specialization constant extraction

### Planned Features

Based on the current roadmap:

- **Acceleration Structure Support**: Full ray tracing resource generation and reflection
- **Enhanced Testing**: Comprehensive unit and integration test coverage  
- **Extended Pipeline State**: Complete PSO definition through data-driven configuration
- **HLSL Support**: Potential DirectX shader support with structure-based I/O
- **Performance Optimizations**: Circular buffer caching for large data retrievals

See `todo.md` for detailed development priorities.

## Configuration & Usage

### Resource Definition

Resources are defined in YAML configuration files that specify both the logical grouping and technical metadata:
```yaml
resource_groups:
  GlobalResources:
    CameraUBO:
      Type: "UniformBuffer"
      Members: |+1
        mat4 viewMatrix;
        mat4 projectionMatrix;
        vec3 cameraPos;
        float time;
    
  LightingResources:
    LightData:
      Type: "StorageBuffer"  
      Members: |+1
        PointLight lights[];
    
    ShadowMap:
      Type: "SampledImage"
      Format: "d32_sfloat"
```

### Resource Types & Configuration

**Buffer Resources** require a `Members` field using literal GLSL syntax:

```yaml
MaterialBuffer:
  Type: "UniformBuffer"
  Members: |+1
    vec3 diffuseColor;
    vec3 specularColor; 
    float roughness;
    float metallic;
```

**Image Resources** should specify format information:

```yaml
ColorAttachment:
  Type: "StorageImage"
  Format: "rgba8_unorm"
  
DepthBuffer:
  Type: "SampledImage"
  Format: "d32_sfloat"
```

**Access Qualifiers** can be applied globally or per-shader:

```yaml
AtomicCounter:
  Type: "StorageTexelBuffer"
  Format: "r32ui"
  Qualifiers: "restrict"
  PerUsageQualifiers:
    ComputeShader: "readonly"
    FragmentShader: "writeonly"
```

You can use these qualifiers to apply readonly/writeonly specifications for a resource across all usages, or give it a "PerUsageQualifier" that references a ShaderStage name it'll have a certain qualifier applied in.

### Shader Pack Definition

ShaderPacks group related shaders that share resources. Each pack is defined in the same YAML file:

```yaml
shader_groups:
  ForwardLighting:
    Shaders:
      Vertex: "forward/lighting.vert"
      Fragment: "forward/lighting.frag"
    Tags: ["Opaque", "MainPass"]
    Extensions: ["GL_EXT_control_flow_attributes"]
  
  DepthPrePass:
    Shaders:
      Vertex: "forward/depth.vert"  
      Fragment: "forward/depth.frag"
    Tags: ["DepthOnly"]
    
  ComputeLighting:
    Shaders:
      Compute: "compute/lighting.comp"
    Extensions: ["GL_KHR_shader_subgroup_ballot"]
```

**Optional Fields:**
- **`Tags`**: Text-based metadata for frontend applications (e.g., "DepthOnly" for depth-only passes)
- **`Extensions`**: GLSL extensions enabled for all shaders in the group

### Reflection API Usage

Once configured, the reflection system provides comprehensive shader analysis:

```cpp
// Load and compile shader pack
ShaderPack pack("volumetric_forward.yaml", session);

// Get reflection data for a specific shader
auto& shader = pack["ForwardLighting"];
ShaderReflector reflector = shader.GetReflector();

// Enumerate descriptor sets
uint32_t numSets = reflector.GetNumSets();
for (uint32_t i = 0; i < numSets; ++i) {
    size_t numResources = 0;
    reflector.GetShaderResources(i, &numResources, nullptr);
    
    std::vector<ResourceUsage> resources(numResources);
    reflector.GetShaderResources(i, &numResources, resources.data());
    
    // Use resources to create VkDescriptorSetLayout
}

// Get vertex input attributes  
size_t numAttributes = 0;
reflector.GetInputAttributes(VK_SHADER_STAGE_VERTEX_BIT, &numAttributes, nullptr);

std::vector<VertexAttributeInfo> attributes(numAttributes);
reflector.GetInputAttributes(VK_SHADER_STAGE_VERTEX_BIT, &numAttributes, attributes.data());

// Get push constants
auto pushConstants = reflector.GetStagePushConstantInfo(VK_SHADER_STAGE_VERTEX_BIT);
VkPushConstantRange range = pushConstants; // Implicit conversion

// Get specialization constants
size_t numSpecConstants = 0;
shader.GetSpecializationConstants(&numSpecConstants, nullptr);
std::vector<SpecializationConstant> specConstants(numSpecConstants);
shader.GetSpecializationConstants(&numSpecConstants, specConstants.data());
```

## Building ShaderTools

### Requirements
- **CMake 3.3+**
- **C++23 Compatible Compiler** (MSVC 2022, GCC 13+, Clang 16+)
- **Vulkan SDK** (for headers)

### Build Process

1. **Clone with submodules:**
   ```bash
   git clone --recursive https://github.com/fuchstraumer/ShaderTools.git
   cd ShaderTools
   ```

2. **Initial CMake configuration** (may fail initially):
   ```bash
   mkdir build && cd build
   cmake ..
   ```

3. **Reconfigure** (required for SPIRV-Tools detection):
   ```bash
   cmake ..  # Run again - this should succeed
   ```

4. **Build:**
   ```bash
   cmake --build . --config Release
   ```

### Build Options

- **`SHADERTOOLS_BUILD_STATIC`**: Build as static library (not recommended due to large dependencies)
- **`SHADERTOOLS_BUILD_TESTS`**: Enable test target for validation

### Library Packaging

ShaderTools builds as a shared library (DLL) by default. This is strongly recommended because:

- We have to haul in a ton of SPIRV-ecosystem-related libraries that'll bloat a static library considerably
- Provides stable C ABI for cross-compiler compatibility  
- And of course, it enables runtime updates without client recompilation

## Testing

Enable testing with `-DSHADERTOOLS_BUILD_TESTS=ON` to build the validation target that compiles the included `VolumetricTiledForward` shader pack - a comprehensive test featuring dozens of shader files and complex resource dependencies.

## Current Limitations

- **Ray Tracing**: Acceleration structure support is planned but not yet implemented
- **Documentation**: Some advanced features may lack comprehensive documentation
- **Platform Support**: Primarily tested on Windows; Linux/macOS compatibility may vary

## Contributing

ShaderTools is actively developed with a focus on modern Vulkan applications. Contributions, bug reports, and feature requests are welcome. See `todo.md` for current development priorities.

## Acknowledgments

Built with:
- **SPIRV-Reflect**: Modern SPIR-V reflection library
- **shaderc**: Google's SPIR-V compiler
- **glslang**: Reference GLSL compiler
- **yaml-cpp**: YAML parsing library

---

*ShaderTools aims to make Vulkan shader development as straightforward as possible while maintaining the performance and flexibility that makes Vulkan powerful.*
