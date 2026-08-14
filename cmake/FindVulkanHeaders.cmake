# Satisfies find_package(VulkanHeaders) using Vulkan_INCLUDE_DIR already resolved by find_package(Vulkan).
# Avoids the deprecated FindVulkanHeaders.cmake shipped with the Vulkan SDK.
if(NOT Vulkan_INCLUDE_DIR)
    message(FATAL_ERROR "FindVulkanHeaders: Vulkan_INCLUDE_DIR not set. find_package(Vulkan REQUIRED) must run first.")
endif()

set(VulkanHeaders_INCLUDE_DIR  "${Vulkan_INCLUDE_DIR}" CACHE PATH "Vulkan headers include directory" FORCE)
set(VulkanHeaders_INCLUDE_DIRS "${Vulkan_INCLUDE_DIR}")
set(VulkanHeaders_FOUND TRUE)
mark_as_advanced(VulkanHeaders_INCLUDE_DIR)

if(NOT TARGET VulkanHeaders::VulkanHeaders)
    add_library(VulkanHeaders::VulkanHeaders INTERFACE IMPORTED)
    set_target_properties(VulkanHeaders::VulkanHeaders PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIR}")
endif()
