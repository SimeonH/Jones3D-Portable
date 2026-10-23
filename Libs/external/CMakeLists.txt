cmake_minimum_required(VERSION 3.12)

add_subdirectory(SDL3)
if(WIN32)
    add_subdirectory(DirectX61c) # DirectSound, DirectPlay
else()
    add_subdirectory(OpenAL)
endif()

if(JONES3D_BUILD_GAME)
    if(NOT WIN32)
        add_subdirectory(libmspack) # CHM help in the wxHTML help viewer
    endif()
    add_subdirectory(wxWidgets) # game dialogs and help viewer
endif()

add_subdirectory(Vulkan)
