project(w32util C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Settings: Windows registry (wuRegistry.c) or a settings file (wuRegistryFile.c)
if(WIN32)
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/wuRegistryFile.c)
else()
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/wuRegistry.c)
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore std)

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
