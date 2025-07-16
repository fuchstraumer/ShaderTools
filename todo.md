# General work and upgrades
- Jesus *christ* we need a new name. ShaderTools is so blah and generic. I want something fun!
- Add handling of acceleration structures to the generator, as right now the reflection system could handle them but we can't actually use/generate them 
- Add unit and integration tests
- Identify how much more info we can pack into the YAML file to allow for the definition of PSOs entirely in data
    - Will need to identify in these requests what vulkan extensions and features are needed to do this, and save them in the data as well
- Identify a new binary format for the generated content
- Generator now needs to extract type information and a name from the specialization constants it parses, so that we can correlate that with what reflection finds
    - Reflection system no longer gives type info, but does give name and binding index. This will allow us to reduce it down to only use SPCs, at least.
- Add a circular buffer caching retrievals of large vectors from client-level API calls, and move things internally so that clients and the internal API use a different function to retrieve data from other objects.
    - This means client code won't be constantly locking, and also that the two-step call functionality required by the DLL will proceed much faster (as the data is cached)
    - But internal API calls will still retrieve the most up to date version of the data, and can also be used to clear or reset the cache
- Maybe we should provide a way to get a unique hash for things like push constant range members and such, so that frontends can use this for associativety to set these values at runtime
- Change usages of streams to use std::format
- Change generator to use a collection of passes, instead of one big monolithic object
- Can we make MAX_INCLUDE_DEPTH a CMake parameter? This would allow for per-project configuration of it.

# Bindless transition updates
- Make sure "unbounded" status is passed fully through and preserved during parsing/generation, and distinguish this status from "not an array type" when querying ArraySize in ShaderResource struct
- Find a way to still track read or write access of a resource, ideally infer it or extract it from a shader resource usage in the SPIR-V. Otherwise fallback on user annotations (unreliable!)
    - User annotations less of a problem if we start finding a data-flow-y way (read: nodegraph) to define and create our shaders

# Vertex interface changes
- As part of move to bindless, we will be able to use vertex pulling. We'll need to remove assumptions about vertex interfaces to account for that, and make it an option that's possible to use
- For the sake of preparing for potential DirectX compat, we should probably revise how we pull or read vertex data. If we use a structure or interface to read our vertices, it'll then become
  a lot easier to eventually integrate DX12
- Vertex pulling should be an optional feature, and enabling/disabling it should ideally not change how we write our shaders... just some markup in the YAML file.

# Resource Accessors
- Also part of bindless work, we should generate accessors for most resources, so that we can either use bindless or explicitly bind as appropriate
- This will include accessors for the vertex data (note that we will still want to keep the index buffer around, to preserve vertex cache)