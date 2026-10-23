cmake_minimum_required(VERSION 3.14)
project(Vulkan_external C)

# Vulkan headers and the volk meta-loader, used by the Vulkan renderer (Libs/std/Vulkan).
# volk loads vulkan-1.dll at runtime, so no Vulkan SDK or import library is needed to build.
# Set FETCHCONTENT_SOURCE_DIR_VULKANHEADERS / FETCHCONTENT_SOURCE_DIR_VOLK to use local source trees instead of downloading.

include(FetchContent)

FetchContent_Declare(VulkanHeaders
    URL      https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.304.tar.gz
    URL_HASH SHA256=5e1a06b3f27e7581b55d1dea176fd97ee0a2f299406db2f437c1d2f297ceba5b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _none # only the headers are needed, don't add the project
)

FetchContent_Declare(volk
    URL      https://github.com/zeux/volk/archive/refs/tags/1.4.304.tar.gz
    URL_HASH SHA256=ab3d4a8ccaeb32652259cdd008399504a41792675b0421d90b67729ee274746f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _none # volk.c is compiled below
)

FetchContent_MakeAvailable(VulkanHeaders volk)

# Don't build volk with the project's strict warning flags
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

add_library(volk STATIC ${volk_SOURCE_DIR}/volk.c)
target_include_directories(volk SYSTEM PUBLIC
    ${volk_SOURCE_DIR}
    ${vulkanheaders_SOURCE_DIR}/include
)

add_library(Vulkan::volk ALIAS volk)
set_target_properties(volk PROPERTIES FOLDER "Libs/external")
