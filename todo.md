# General work and upgrades
- Jesus *christ* we need a new name. ShaderTools is so blah and generic. I want something fun!
- Add handling of acceleration structures to the generator, as right now the reflection system could handle them but we can't actually use/generate them 
- Add unit and integration tests
- Identify how much more info we can pack into the YAML file to allow for the definition of PSOs entirely in data
    - Will need to identify in these requests what vulkan extensions and features are needed to do this, and save them in the data as well
- Identify a new binary format for the generated content
- Generator now needs to extract type information and a name from the specialization constants it parses, so that we can correlate that with what reflection finds
    - Reflection system no longer gives type info, but does give name and binding index. This will allow us to reduce it down to only use SPCs, at least.
- Structure support for interface inputs and outputs: Will help with possible eventual HLSL support, and will let us clean things up a bit.
    - Potentially look at making it so we auto-generate locations even in cases of interface overrides if we use this
- Add a circular buffer caching retrievals of large vectors from client-level API calls, and move things internally so that clients and the internal API use a different function to retrieve data from other objects.
    - This means client code won't be constantly locking, and also that the two-step call functionality required by the DLL will proceed much faster (as the data is cached)
    - But internal API calls will still retrieve the most up to date version of the data, and can also be used to clear or reset the cache
- Maybe we should provide a way to get a unique hash for things like push constant range members and such, so that frontends can use this for associativety to set these values at runtime

# Bindless transition updates
- Make sure "unbounded" status is passed fully through and preserved during parsing/generation, and distinguish this status from "not an array type" when querying ArraySize in ShaderResource struct
- Find a way to still track read or write access of a resource, ideally infer it or extract it from a shader resource usage in the SPIR-V. Otherwise fallback on user annotations (unreliable!)
    - User annotations less of a problem if we start finding a data-flow-y way (read: nodegraph) to define and create our shaders

