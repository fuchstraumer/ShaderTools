# ShaderTools - Tools for working with shaders and Vulkan

ShaderTools is a library that supports a number of uses: the most primitive being runtime shader compiliation from GLSL source code and aiding in generating
Vulkan binding/descriptor objects at runtime. Most of the expensive generated data can be saved to a binary file on program exit, and then reloaded later - 
timestamps of the saved data are then evaluated, and if the source code of the generated/compiled shaders in this file has been updated (resulting in a newer write time) 
things will then be re-generated. This can cut load time by as much as 10x! (23ms for binary re-load vs 250ms for conventional load and generation)

### Recently refactored! I'm going to have to work on docs and an article documenting the changes
