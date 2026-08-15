## Repository Context

ShaderTools is a pipeline for shader cooking, packaging Slang modules into cooked blobs (or in-memory blobs) for client applications to use. Clients agree to a data contract, and we aim to fulfill that contract. The goal of this system is to perform the complex mapping of intent and needs between two domains: the shader editing and content workflows domain, where flexibility and ease of use is paramount, and the runtime graphics domain. In the latter, we have different needs more focused on performance and ensuring stable behavior from a renderer - in addition to not blocking the main thread waiting for a shader to build. This library also makes no guarantee or prescription on the output data format: Currently it is WGSL, but further planned work will that make an exchangeable step in a chained compiler pipeline.

This library works as a compiler, in many ways.

## Response style

Follow Zinsser's four principles of quality writing:
1. Simplicity
2. Brevity
3. Clarity
4. Humanity

Additionally, communicate in ASD-STE100, or Simplified Technical English. Cut clutter, give each word one
meaning, and don't always reach for highly abstract verbiage. As a reminder some of those core rules are:

- Make instructions as clear and specific as possible.
- Do not write multi-word nouns that have more than three words.
- Use the approved forms of verbs to make only:
  - The infinitive form
  - The imperative form
  - The simple present tense
  - The simple past tense
  - The simple future tense
  - The past participle (only as an adjective)
- Do not use auxiliary verbs to make complex verb constructions.
- Use the "-ing" form of a verb only as a technical noun or as a modifier in a technical noun.
- Use the active voice. In descriptive writing, one should use the passive voice only when the agent is unknown.
- Write short sentences: no more than 20 words in instructions (procedures) and 25 words in descriptive texts.
- Do not omit parts of the sentence (e.g. verb, subject, article) to make the text shorter.
- Use vertical lists for complex text.
- Write one instruction per sentence.
- Write only one topic per paragraph.
- Do not write more than six sentences in each paragraph.
- Start safety or performance instructions with a clear command or condition.
  
## Code style

- Formatting: `.clang-format` — LLVM base, 4-space indent, 110-column limit, Allman braces, left-aligned
  pointers, always-break template declarations, no bin-packing of args/params.
- Static analysis: `.clang-tidy` is present and expected to be respected.

#### Code Formatting Rules
- **Single-line if statements**: NEVER allowed. All if statements must include brackets placed on a newline
- **Function implementations**: Eagerly define in source. No lazy implementations in headers — not getters,
  not setters, not one-line functions. Templates and `constexpr` functions intended for compile-time
  evaluation sometimes force our hand (`Future.hpp`, `SlotMap.hpp`, generated permutation-key code); that's
  just how it goes, and is not license to inline anything else
- **Indentation**: 4 spaces always (no tabs) for cross-platform consistency
- **Brackets**: Always go on new lines
- **Control Flow**: Always use braces for if statements, even single-line ones
- **Naming**: PascalCase for public APIs, camelCase for private members, snake_case for parameters
- **Single-word parameters**: snake_case and camelCase converge for a single token, so a single-word
  parameter can silently collide with a member of the same name. Prefix those with an underscore
  (`_instance`, `_createInfo`). Most compilers still resolve the member initialization correctly, but
  it is a silent killer when they don't, and the prefix is free. Multi-word parameters stay snake_case
  and need no prefix (`create_info`, `module_path`)
- **Member prefixes**: `m_` must NEVER be used as prefix for member variables
- **Constructor initializers**: Colon on same line as declaration, each initializer on new line with trailing comma:
```cpp
Struct::Struct(int _val0, int _val1, int _val2) :
  val0{ _val0 },
  val1{ _val1 },
  val2{ _val2 }
{}
```
- **Switch Statements**: if a case is going to do more than return a value or call a function, pull that logic out into a separate function with a descriptive name. If brackets would need to be inserted to initialize variables in the case: pull it out into a separate function. Treat switch statements in this usage like a table of functions to be called
- **Comment usage**: Avoid as much as absolutely possible. Comments are no subsititude for descriptive code: I would rather have function names that are 80 characters long than comments that will rapidly drift from the source. Absolutely no comments depicting categories of code: that should be inferred from how functions are grouped (in the same order as they are declared, and in declaration order in the definition file)
- **Local variables**: If doing repeated operations, prefer longer variable names. use `deltaX` instead of `dX`, assign variables to const during long chains of mathematical operations (almost like writing scalarized SSA code), and favor being readable over being clever or taking shortcuts. We will save shortcuts and esoteric performant code for profiling results
- **Eagerly factor out common logic**: If some bit of code is greater than 4-5 lines and being duplicated, factor it out into a common function.
- **`todo` comments**: Spare these for things that are actually worth having greppable as distinct work items. For things that will need to be fixed before shipping this to customers or clients who are not devs or friends: use `todo-ship`. use `todo-perf` for things that could grant sizeable performance benefits. Minimize the usage of `todo` as much as possible: we need to get to MVP, but we also don't need to fill our backlog on that road.

Examples of well formatted code in this codebase: `Future.hpp`, `InputManager.hpp` + `InputManager.cpp`, `Context.hpp` + `Context.cpp`. 

#### C++ Language Preferences
- **Functions**: No implementations in headers; mark `constexpr` and `noexcept` when possible
- **Constructors**: Should be `noexcept` when possible
- **Move/copy operators**: Define `noexcept` versions when beneficial
- **Auto usage**: Minimize except for iterators/complex nested types (e.g., `auto iter = map.find(key)` OK, `auto value = vector.front()` not OK)
- **Virtual classes**: Use `final` when possible to collapse vtables and improve performance
- **Error handling**: Use `Result` types for function return status within RHI code; avoid exceptions. A result type is just `std::expected` with an error code enum as the unexpected value. When working outside the RHI, declare an error code for that subsystem and use that as appropriate. Don't leak error codes
- **Subsystem error pattern**: a subsystem outside the core RHI declares its own enum plus its own alias,
  and never borrows `RhiError`. `tools/shader_cooker` is the reference: `CookError` +
  `template<typename T> using CookResult = std::expected<T, CookError>;` in `CookerErrors.hpp`, with a
  `ToString(CookError)` declared there and defined in the matching source file
- **`Result<void>`**: `std::expected<void, E>` is the return type for "can fail, yields nothing." Return a
  braced `{}` for success and `std::unexpected(Error::Whatever)` for failure. Propagate by returning the
  `Result` itself rather than re-wrapping it
- **Error logging**: For debug code or code that will be executed only on native, log with `std::println` frequently and often if it will help debugging. Same philosophy as comments though: do not fill it up for the sake of saying something.
- **Dynamic allocation**: avoid as much as possible, whenever possible. If required, allocate carefully, reserve upfront, and do not let memory persist
- Used ranged-for loops with the ranges library when possible, and as many of the algorithms header from that library as you can
- When writing output files, coalesce your writes into a single write operation by building the output piece
by piece, and then performing one final output step. This also reduces the number of times you  need to check
for valid paths and directories.

#### Enum Formatting
- Use smallest bitwidth type possible, always prefer `enum class` for scoping
- **Every enum reserves `0` for `Invalid`**, so a zero-initialized or memset value is never mistaken for
  a meaningful one. This mirrors how booleans behave: zero is the absence of a valid answer
- For result/error enums: `Invalid = 0`, then `Success` (or the equivalent "it worked" value) at `1`,
  then every error value beyond that
- For taxonomy enums (no success concept — `BindingKind`, `ShaderStageKind`, input event types): just
  `Invalid = 0`, then the values
- For bitmask enums: add operators for at least `|` and `&` operations
- Boolean conversion operators are preferable

#### Memory & Performance
- **Threading**: This project is not designed for massive threading but it is considered an important design goal
  that it is *thread-hardened*. Atomics are used where threads could compete over resources, and mutexes should
  be a reluctant object of absolute last resort. This app should be designed to scale to multiple threads, but 
  individual objects and functions should be viewed as single-threaded internally. Don't communicate by sharing
  memory, share memory by communicating.
  - **Message Passing**: as such, interfaces between modules of code that may need to talk to each  other should
    use message-passing paradigms. This allows for better thread isolation of work, and encourages a validate-
    try-commit model that is more recoverable.
- **Memory**: Avoid dynamic allocations as often as possible. On web targets, we are running in a virtual env
  with a pre-allocated linear span of memory. We must be frugal with memory, and should favor using statically
  allocated arenas and being efficient with our choice of datatypes.
- **Span**: Use `std::span` for array parameters instead of raw pointers + size, and for passing ranges
  of values between systems. 
- **String conversion**: Use `charconv` instead of C conversion functions for string/char to integral types
- **Error Handling**: Use `Result` types for function return status within RHI code; avoid exceptions. For code outside core Rhi, create a new enum class and use it with `std::expected` for error handling. Bubble up errors to the caller instead of logging and returning a default value.

#### Error Handling
- **Result<T>**: Use a rust-like Result<T> bubbled up through functions to return a value or an error
  code to users
- **Error Values**: Use an enum class of the minimum width required to convey the systems range of errors.
  Use 0 as an invalid initial value, 1 as success, and pin all further error codes to be > 1
- **Error Messages**: For `enum class` error values, provide an enum-to-stringview conversion function in
  a header that uses magic_enum in the source to retrieve the enum name. Further information may be appended
  to the view, but careful consideration of lifetime of error strings should be considered
- **Exceptions**: Exceptions are a tangled mess on web, especially with how event-loop-driven our app is.
  Avoid them as much as possible, and attempt to provide a way for modules of code to shutdown and restart
  in a known-good state to recover from errors.

#### Header Conventions
- Headers carry **both** `#pragma once` and a traditional `#ifndef`/`#define`/`#endif` guard. This is
  intentional and consistent across the repo — do not "clean it up" to one or the other. Guard macros are
  screaming-snake-case derived from the path (`LODESTONE_ASYNC_FUTURE_HPP`,
  `LODESTONE_SHADER_COOKER_ERRORS_HPP`), and the closing `#endif` carries a `// !GUARD_NAME` comment
- Keep standard-library includes minimal and sorted within a group; a header should include what it
  names and nothing more. Prefer forward declarations across module boundaries (see how `Context.hpp`
  forward-declares `Scheduler`)
- Hide third-party types behind a pimpl when a header would otherwise force them on every consumer.
  `tools/shader_cooker/include/SlangCompiler.hpp` names no Slang type for exactly this reason

### C++ Standard Library Usage
- Use `std::upper_bound` and `std::lower_bound` from `<algorithm>` when possible
- Retrieve numerical constants from `<numbers>` header
- Minimize standard library includes across module boundaries
