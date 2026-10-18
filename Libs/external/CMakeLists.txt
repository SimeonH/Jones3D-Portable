cmake_minimum_required(VERSION 3.12)

if(WIN32)
    add_subdirectory(DirectX61c)
else()
    add_subdirectory(SDL3)
    add_subdirectory(OpenAL)
endif()

if(JONES3D_BUILD_GAME)
    add_subdirectory(wxWidgets) # game dialogs

    if(JONES3D_BUILD_HLP_VIEWER)
        if(NOT JONES3D_CARGO)
            message(FATAL_ERROR "JONES3D_BUILD_HLP_VIEWER needs cargo")
        endif()
        add_subdirectory(HlpViewer) # in-game help
    endif()
endif()

if(JONES3D_RENDERER STREQUAL "Vulkan")
    add_subdirectory(Vulkan)
endif()
