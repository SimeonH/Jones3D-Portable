project(std C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Renderer backend: Win95/std3D.c + Win95/stdDisplay.c (DirectDraw) or Vulkan/*.c
set(STD_DDRAW_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/std3D.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/stdDisplay.c
)
file(GLOB STD_VULKAN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/*.c)
file(GLOB STD_VULKAN_SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.frag)
file(GLOB STD_VULKAN_SHADER_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan/shaders/*.glsl)

if(JONES3D_RENDERER STREQUAL "Vulkan")
    list(REMOVE_ITEM SOURCES ${STD_DDRAW_SOURCES})
else()
    list(REMOVE_ITEM SOURCES ${STD_VULKAN_SOURCES})
endif()

# Platform: Win95/ (DirectInput, Win32 console, DirectPlay) or SDL/ (SDL3, POSIX)
set(STD_WIN32_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/stdComm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Win95/stdConsole.c
)
file(GLOB STD_SDL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/SDL/*.c)

if(WIN32)
    if(STD_SDL_SOURCES)
        list(REMOVE_ITEM SOURCES ${STD_SDL_SOURCES})
    endif()
else()
    list(REMOVE_ITEM SOURCES ${STD_WIN32_SOURCES})
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        Winmm # timeGetTime
        DirectX61c::ddraw
        DirectX61c::dxguid
        DirectX61c::dinput
        DirectX61c::dplayx
    )
    target_link_options(${PROJECT_NAME} INTERFACE "/SAFESEH:NO") # due to DirectX 6.1 libs
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3 m)
endif()

if(JONES3D_RENDERER STREQUAL "Vulkan")
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::volk $<$<BOOL:${WIN32}>:Gdi32>)

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
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
