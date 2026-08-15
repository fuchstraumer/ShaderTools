# SlangCompiler
- Right now, we map to a super small subset of formats and features. We should support the full range, and extract them untouched
    - Then, during output format mapping we collapse to what that platform
    actually supports
    - Alternatively, use foresight about output target to fail builds if unsupported
# General Cleanup
- Break apart some of the really long functions in the codebase
- Subdivide functionality into some smaller objects, and reorg folder structure to be a little less flat
# Feature additions/removals:
- Remove the header/cpp generator. It's not necessary with manifest files, and is much less efficient. 
- Add the live process cooker. Not sure if persistent .exe or hotloaded DLL on windows with dllMain. Probably not too hard to support both.
# Resource Layer
- We should provide a way for clients to call something like `SetDeviceLimits` or `SetApiLimits` - we can use this to validate resource sizing expressions when being run as a live compiler,
  or we can use it against cooked content (in the device form) to make sure we don't try to create a shader a device can't support
# Permutation system
- Given the above, we'll also want a way to flag the "domain" of a parameter and it's optionality? Rootness? How important it is to the output, and if we expect it to depend on device properties.
  This could accelerate queries and allow for internal optimizations to help section data into platform/scalability presets perhaps.