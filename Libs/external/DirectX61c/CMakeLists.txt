cmake_minimum_required(VERSION 3.12)
project(DirectX61c)

# Function to create an imported library
function(add_directx_library name lib_file)
    add_library(${PROJECT_NAME}::${name} STATIC IMPORTED GLOBAL)
    set_target_properties(DirectX61c::${name} PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/${lib_file}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
endfunction()

# Create imported libraries for each DirectX component
add_directx_library(ddraw ddraw.lib)
if(CMAKE_CROSSCOMPILING)
    # The MinGW-built dxguid.lib used for cross-compiling defines all GUIDs in a single object,
    # which clashes with uuid.lib (needed by wxWidgets). Use the Windows SDK dxguid.lib instead,
    # it still contains all DirectX 6 GUIDs.
    add_library(DirectX61c::dxguid INTERFACE IMPORTED GLOBAL)
    set_target_properties(DirectX61c::dxguid PROPERTIES
        INTERFACE_LINK_LIBRARIES dxguid.lib
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
else()
    add_directx_library(dxguid dxguid.lib)
endif()
add_directx_library(dinput dinput.lib)
add_directx_library(dplayx dplayx.lib)
add_directx_library(dsound dsound.lib)

# Create a meta-target that includes all DirectX components
add_library(DirectX61c INTERFACE)
target_link_libraries(DirectX61c INTERFACE
    DirectX61c::ddraw
    DirectX61c::dxguid
    DirectX61c::dinput
    DirectX61c::dplayx
    DirectX61c::dsound
)