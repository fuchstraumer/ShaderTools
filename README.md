# ShaderTools - Tools for working with shaders and Vulkan

This project initially started as a GLSL -> SPIR-V compiler, along with being able to generate `VkDescriptorSetLayoutBinding`s + `VkVertexInputAttributeInfo`s from SPIR-V shader files, along with the singular `VkPushConstantRange` for a set of shaders.

It now also includes ShaderGenerator, which has a number of useful advantages over manually writing + compiling all your GLSL shaders that I'll review shortly.

# ShaderCompiler 

The shader compiler takes a path to a complete shader file, and what stage this shader file is for. It then compiles the shader to SPIR-V
and saves it internally, storing it in a map with the path as the key and the binary as the value. This is really all this class is for -
it was primarily just intended to allow for runtime shader compiliation using the various SPIR-V tools and repositories.
#### Compiling shaders
Compiling is simple:
```cpp
ShaderCompiler compiler;

// Supplying the stage is optional - so long as the file extension is one of the following:
/*
    - .vert : vertex shader
    - .frag : fragment shader
    - .geom : geometry shader
    - .teval : tesselation evaluation shader
    - .tcntl : tesselation control shader
    - .comp : compute shader
    
    Note: Using tesselation or geometry shaders on Mac OSX will cause an exception to be thrown,
    as API shims like MoltenVK do not support these shader types when being used to run Vulkan code
     / SPIR-V shaders on Mac.
*/
compiler.Compile("my_vertex_shader.vert");

// Otherwise, explicitly specify the stage
compiler.Compile("my_fragment_shader.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

```

#### Retrieving compiled binaries
Retrieval works like retrieval of array/vector data works in other similar APIs: call a function first with a size variable to get the size.
Allocate a container able to fit the retrieved size, then call the function again with a pointer to the container as well.
```cpp

uint32_t binary_size = 0;
compiler.GetBinary("my_vertex_shader.vert", &binary_size, nullptr);
std::vector<uint32_t> vertex_shader_binary(binary_size);
compiler.GetBinary("my_vertex_shader.vert", &binary_size, vertex_shader_binary.data());
```

#### Additional functions

`Compiler.HasShader(shader_path)` checks to see if the shader at the given path has already been compiled to a binary and exists 
in the Compiler object.

`AddBinary()` can be used to add already-compiled binaries to the object.

#### WIP features
- Saving compiled shaders to file, or compiling a given shader at a path then saving it to file. Not implemented in the interface class, 
but has already been implemented in the implementation class
- Checking for pre-existing binaries before compiling, and then comparing the last mod. date between the source and binary to see if
we need to actually compile the shader. Currently also implemented, but not exposed in the interface class. Both bonus features that aren't
high priority for me right now.

# BindingGenerator

The binding generator is where the real "magic" happens. It takes compiled binaries and parses their various attributes and can construct
appropriate Vulkan structures for use. Despite the fact that it mostly parses, I called it a "BindingGenerator" as it takes care of generating
the Vulkan resources explicitly related to resource bindings and layouts (guess I could've called it LayoutGenerator, but that's even more banal)

Why compiled binaries? This is because the SPIR-V compiler will implicitly optimize unused items out, so the returned bindings and structures 
only consider what is actually being used. This can be beneficial, but it can also highlight errors in shader code when the generated objects
and the expected interfaces don't match (so, clean up the shader). 

But it's also because I wasn't exactly eager to write a whole parser for shaders when SPIR-V cross can do most of the parsing and reflection
for me. 

## An important note

The binding generator works on multiple stages at a time, and really requires all stages being used for a given pipeline be parsed by
successive calls to `ParseBinary` before valid objects can be retrieved. This is due to how the parsing/sorting system works, and due
to how things like push constants work.

Once you're done submitting shader stages and want to use a new shader group, call `BindingGenerator.Clear()` to reset the implementations
state and clear the data stored thus far.

#### Parsing binaries

Is done by calling the function briefly referenced above, `ParseBinary()`. Example showing compiling then parsing:
```cpp
ShaderCompiler compiler;
compiler.Compile("my_vertex_shader.vert");

uint32_t sz = 0;
compiler.GetBinary("my_vertex_shader.vert", &sz, nullptr);
std::vector<uint32_t> binary_data(sz);
compiler.GetBinary("my_vertex_shader.vert", &sz, binary_data.data());

BindingGenerator bindings;
// instead of passing sz, could also use binary_data.size()
// Stage must be explicitly passed as stage can't be inferred from the extension here.
bindings.ParseBinary(sz, binary_data.data(), VK_SHADER_STAGE_FLAGS_VERTEX_BIT);
```

#### Retrieving objects after parsing

##### VkDescriptorSetLayout retrieval

```cpp
// First, we need to figure out how many sets we have. 
uint32_t num_sets = bindings.GetNumSets();
// Just going to get items for set 0, but process remains the same for retrieving SetLayouts for other sets
uint32_t num_bindings = 0;
bindings.GetLayoutBindings(0, &num_bindings, nullptr);
std::vector<VkDescriptorSetLayoutBinding> bindings(num_bindings);
bindings.GetLayoutBindings(0, &num_bindings, bindings.data());
```

The retrieved bindings can now be used to create a `VkPipelineLayout` object, alongside
the next member we'll retrieve:

##### VkPushConstantRange and VkVertexInputAttribute retrieval

Retrieval of both these objects proceeds like it did `VkDescriptorSetLayoutBinding`s -
1. Find out how many objects will be retrieved
2. Create a container able to hold X objects when retrieved
3. Call the function again with the size and container pointers as parameters

#### VkVertexInputBindings

An important note is that these types of objects cannot possible be parsed or retrieved by the shader tools. These
objects are going to change based on how the resources and attributes are bound/attached to different buffers, as well
as depending on these objects being either per-vertex or per-instance attributes. These is no way I currently know of to:
- Retrieve vertex buffer binding from an attribute
- Find out if an attribute is per instance or per vertex 
- and lastly, find out what the exact offset of an attribute in a vbo is

# ShaderGenerator
###### This section is still WIP as all hell!

ShaderGenerator is intended to greatly simplify the process of writing shaders, especially given that Vulkan requires specifying resource layouts using `(set = N, binding = M)` specifiers. Instead of requiring the copy and pasting of these bindings across multiple shader files, along with making sure each shader file has the same bindings and number of bindings per set, this is all managed using a preprocessing step.

Like most shader compilers, `#include`s are also supported now.

### Resource Blocks

In Vulkan, descriptor objects are grouped in sets: this is most useful to reduce binding commands between drawing various objects. But it also provides a useful way to group resources logically, and this is how ShaderGenerator approaches the problem. The first step is adding a resource file to the shader generator - at this point, the various blocks are run through initial processing.

Declaring a new block is done with: `#pragma BEGIN_RESOURCES (BLOCK_NAME)`
Add a newline after the last object in a block, and close it with `#pragma END_RESOURCES (BLOCK_NAME)`
Using a block in a shader is done with: `#pragma USE_RESOURCES (BLOCK_NAME)` 

When using a block, make sure to `#include` any structures in your blocks BEFORE you use a resource block. 

### Resource types

###### WIP

### Specialization constants

Specialization constants are easily declared by simply prefixing the usual `const T val = (value)` with `SPC`. So, a constant index for a loop could be `SPC const uint LoopLimit = 25;`. This will then be parsed and assigned the proper `constant_id` decorator during the final generation step.

### Push constants

###### Also WIP as hell

## Generation, from start to finish
```cpp
// Stage must be passed to constructor, as it effects the parsing somewhat significantly
// and different stages have different built-in fragments to insert during generation
st::ShaderGenerator shader(VK_SHADER_STAGE_VERTEX_BIT);
// Several include paths can be added, either one-by-one like so or
// by passing a const char* array
shader.AddIncludePath("my_include_dir");
// Add one resource file (easiest to do, imo), or several before adding
// the body of the shader
shader.AddResources("Resources.glsl");
// Before requesting the full source, complete the generation step
// by adding the "body" of your shader: usually, the parts around main()
shader.AddBody(fname.second.c_str());

// To keep the API dll-friendly, std library containers aren't used.
// Instead, like most C-style APIs, you need to query a size before
// actually retrieving data
size_t sz = 0;
shader.GetFullSource(&sz, nullptr);
// With the size found, create a string and resize it to fit the 
// generated shader file.
std::string src; src.resize(sz);
shader.GetFullSource(&sz, src.data());
```

