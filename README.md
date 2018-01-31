# ShaderTools - Easy shader reflection for Vulkan

The intent of this library is to make dynamic generation of Vulkan objects required to set up a graphics pipeline easier. It has two main parts:

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
appropriate Vulkan structures for use.

Why compiled binaries? This is because the SPIR-V compiler will implicitly optimize unused items out, so the returned bindings and structures 
only consider what is actually being used. This can be beneficial, but it can also highlight errors in shader code when the generated objects
and the expected interfaces don't match (so, clean up the shader). 

But it's also because I wasn't exactly eager to write a whole parser for shaders when SPIR-V cross can do most of the parsing and reflection
for me. 

## An important note
