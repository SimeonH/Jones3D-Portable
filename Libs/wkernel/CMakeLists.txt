project(wkernel C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Platform: Win32 window (wkernel.c) or SDL window (wkernelSDL.c)
if(WIN32)
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/wkernelSDL.c)
else()
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/wkernel.c)
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore std)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Comctl32.lib)
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

