project(std C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Renderers: the GPU std3D + stdDisplay in Gpu/ over the backends in Vulkan/, GLES2/ and D3D11/
file(GLOB STD_VULKAN_SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.frag)
file(GLOB STD_VULKAN_SHADER_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.glsl)
file(GLOB STD_GLES2_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/GLES2/*.c)
file(GLOB STD_D3D11_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/D3D11/*.c)

if(NOT JONES3D_RENDERER_GLES2 AND STD_GLES2_SOURCES)
    list(REMOVE_ITEM SOURCES ${STD_GLES2_SOURCES})
endif()
if(NOT JONES3D_RENDERER_D3D11 AND STD_D3D11_SOURCES)
    list(REMOVE_ITEM SOURCES ${STD_D3D11_SOURCES})
endif()

# Platform: SDL/ (SDL3) on all platforms, Win95/ has the Windows parts (DirectPlay, Win32 console)
set(STD_WIN32_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/stdComm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/stdConsole.c
)
set(STD_POSIX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL/stdComm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL/stdConsole.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL/stdWin32File.c
)

if(WIN32)
    list(REMOVE_ITEM SOURCES ${STD_POSIX_SOURCES})
else()
    list(REMOVE_ITEM SOURCES ${STD_WIN32_SOURCES})
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        Winmm # timeGetTime
        DirectX61c::dxguid
        DirectX61c::dplayx
    )
    target_link_options(${PROJECT_NAME} INTERFACE "/SAFESEH:NO") # due to DirectX 6.1 libs
    if(JONES3D_RENDERER_D3D11)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d11.lib dxgi.lib) # d3dcompiler_47.dll is loaded at runtime
    endif()
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC m)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::volk)

# Shaders are compiled to SPIR-V C initializer lists (<name>.spv.h) and embedded in the binary
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_HEADERS "")
foreach(shader ${STD_VULKAN_SHADERS})
    get_filename_component(shader_name ${shader} NAME)
    set(shader_header ${SHADER_OUT_DIR}/${shader_name}.spv.h)
    add_custom_command(
        OUTPUT  ${shader_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
        COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O -mfmt=c -o ${shader_header} ${shader}
        DEPENDS ${shader} ${STD_VULKAN_SHADER_INCLUDES}
        COMMENT "Compiling shader ${shader_name}"
        VERBATIM
    )
    list(APPEND SHADER_HEADERS ${shader_header})
endforeach()

add_custom_target(std_shaders DEPENDS ${SHADER_HEADERS})
add_dependencies(${PROJECT_NAME} std_shaders)
target_include_directories(${PROJECT_NAME} PRIVATE ${SHADER_OUT_DIR})
target_sources(${PROJECT_NAME} PRIVATE ${STD_VULKAN_SHADERS} ${STD_VULKAN_SHADER_INCLUDES})

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
