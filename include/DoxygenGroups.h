#pragma once

/**
 * @defgroup Common Common Objects and Utilities
 * @brief Shared utilities, constants, enums, and common definitions used across all modules
 * 
 * The Common module provides foundational types, utility structures, error handling,
 * session management, and shader stage definitions that are used throughout the
 * ShaderTools library. This includes DLL interface definitions, platform-specific
 * macros, and core data structures.
 */

/**
 * @defgroup Core Core Classes
 * @brief Main shader representation and shader pack management classes
 * 
 * The Core module contains the primary classes that represent compiled shaders
 * and collections of shaders. This includes the Shader class which represents
 * a collection of shader stages used together in a pipeline, and ShaderPack
 * which manages multiple related shaders and their metadata.
 */

/**
 * @defgroup Generation Shader Generation
 * @brief Shader compilation and code generation functionality
 * 
 * The Generation module handles the compilation of shader source code into
 * SPIR-V assembly and binary formats. This includes the ShaderCompiler class
 * for compiling shader stages to endpoint binary formats for use with shader 
 * pipelines, and the ShaderGenerator for taking input shader source code 
 * and generating valid shader source strings from it.
 */

/**
 * @defgroup Reflection Shader Reflection
 * @brief SPIR-V reflection and analysis capabilities
 * 
 * The Reflection module provides functionality to analyze compiled SPIR-V shaders
 * and extract metadata about resources, bindings, vertex attributes, push constants,
 * and other reflection information needed for pipeline construction and resource
 * binding.
 */

/**
 * @defgroup Resources Resource Management
 * @brief Resource binding, usage tracking, and descriptor set management
 * 
 * The Resources module handles the representation and management of shader resources
 * such as uniform buffers, storage buffers, textures, and samplers. It provides
 * classes for tracking resource usage across multiple shaders, managing resource
 * groups, and optimizing descriptor set layouts.
 */

/**
 * @defgroup Parser Configuration Parsing
 * @brief YAML configuration file parsing and processing
 * 
 * The Parser module handles the parsing and processing of YAML configuration files
 * that define shader compilation settings, resource bindings, and the shader source files
 * that will become part of the shader pack.
 */
