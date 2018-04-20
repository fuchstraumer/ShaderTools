# ShaderTools - Tools for working with shaders and Vulkan

### Currently tremendously WIP as I work on getting it to "v1.0" of sorts, to make it a more truly useful and applicable library!

Vague and probably rarely updated to-do list:

- [ ] Finish ShaderGroup and ShaderPack, so groups of shaders and their accompanying resources can be fully described from Lua scripts
    and a few corresponding shaders: allowing users to generate backing resources AND Vulkan objects/bindings using parsed data from this library
- [ ] Improve performance and reuseability by somehow caching the shader groups and shader packs, especially the results of the compiliation
    and parsing stages. Currently, attempts are made to avoid re-loading or parsing data during program execution, but nothing is saved between sessions
- [ ] Improve the shader compiliation toolchain by inserting optimizations and extensions based on current hardware (and hardware support), along with
    adding a tiny "standard library" of algorithms like compute-shader sorting systems (which in turn can use things like subgroup operations when supported)
- [ ] Get people to use this! Are you reading this? Take a look through the code and let me know if there are features you want, issues you have with using
    this project, and so on. I'm always open to criticism and would love to hear from people using/browsing my code.


